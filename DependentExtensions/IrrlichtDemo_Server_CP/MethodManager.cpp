#include "MethodManager.h"
#include "NetworkManager.h"
#include "NetLogManager.h"
#include "CInGame.h"
#include <cmath>
#include <vector>
#include <numeric>

using namespace RakNet;
using namespace irr;

int RttKeyComparison(const RttKey& key1, const RttKey& key2) {
    if (key1.systemAddress > key2.systemAddress) return 1;
    if (key1.systemAddress < key2.systemAddress) return -1;

    if (key1.sequenceIndex > key2.sequenceIndex) return 1;
    if (key1.sequenceIndex < key2.sequenceIndex) return -1;

    return 0; 
}

MethodManager* MethodManager::instance = nullptr;

MethodManager* MethodManager::Instance() {
    if (instance == nullptr) instance = new MethodManager();
    return instance;
}

void MethodManager::DestroyInstance() {
    if (instance) {
        instance->isRunning = false;
        instance->queueCondVar.notify_all(); // 자고 있는 스레드 깨우기
        if (instance->schedulerThread && instance->schedulerThread->joinable()) {
            instance->schedulerThread->join();
            delete instance->schedulerThread;
        }

        // 2. [추가] 큐에 남아있는 미전송 패킷들의 메모리 해제
         // priority_queue는 clear()가 없으므로 빌 때까지 꺼내야 함
        {
            std::lock_guard<std::mutex> lock(instance->queueMutex); // 안전하게 락 걸고 진행
            while (!instance->taskQueue.empty()) {
                ScheduledPacket packet = instance->taskQueue.top();
                instance->taskQueue.pop();

                if (packet.dataStream) {
                    delete packet.dataStream;
                }
            }
        }

        delete instance;
        instance = nullptr;
    }
}

MethodManager::MethodManager() {
    currentMethodsBitmask = 0;
	currentMethodsLogBitmask = 0;
    currentCongestedUserCount = 0;
    reactionTime = 0;
	scenarioNum = 0;
    method1bool = false;
    method1cnt = 0;
    method3cnt = 0;
    method3LogTime = 0;
    um_cnt = 0;
    am_cnt = 0; 
    sumScore = 0; 
	schedulerThread = nullptr;
    isServer = false;
}

MethodManager::~MethodManager() {
    RakNet::TimeMS currentTime = RakNet::GetTimeMS();
    if (IsMethodActive(METHOD_3) && currentTime - method3LogTime >= 1000) // 1000ms = 1초
    {
		if (IsMethodLogActive(METHOD_3)) NetLogManager::Instance()->SaveLogsToCSV(3);
        method3LogTime = currentTime;
    }
}

void MethodManager::Initialize(int methodMask, int methodLogMask, int scenario, bool isServer) {
    currentMethodsBitmask = methodMask;
	currentMethodsLogBitmask = methodLogMask;
	scenarioNum = scenario;

	this->isServer = isServer;
    if (scenarioNum == 1) CInGame::Instance()->BOT_MOVE_TIME = 1000;

    if (this->isServer) {
        isRunning = true;
        schedulerThread = new std::thread(&MethodManager::ThreadLoop, this);
    }
}

void MethodManager::Activate() {
}

int MethodManager::ConvertMethodToIndex(int method) {
    switch (method) {
    case METHOD_1: return 1;
    case METHOD_2: return 2;
    case METHOD_3: return 3;
    default: return 0; // 유효하지 않은 경우  
    }
}

bool MethodManager::IsMethodActive(int methodFlag) const {
	return (currentMethodsBitmask & methodFlag) != 0;
}

bool MethodManager::IsMethodLogActive(int methodLogFlag) const {
    return (currentMethodsLogBitmask & methodLogFlag) != 0;
}

int MethodManager::GetSequenceIndexForMethod(int method) {
    switch (method) {
    case METHOD_1: return method1cnt;
    case METHOD_2: return 2;
    case METHOD_3: return method3cnt;
    default: return 0;
    }
}

void MethodManager::UpdateMethod(int methodFlag) {
	
    if(IsMethodActive(METHOD_1)) if (method1bool) method1cnt++;
    if (IsMethodActive(METHOD_3)) {
        RakNet::TimeMS currentTime = RakNet::GetTimeMS();
        if (currentTime - method3LogTime >= 1000) // 1000ms = 1초
        {
            UpdateMethod3(); //통계 계산 및 N_CI 업데이트
        	method3LogTime = currentTime;
        }
	}
}

void MethodManager::EnqueuePacket(RakNet::SystemAddress target, RakNet::BitStream* bs, RakNet::TimeMS delayMs) {
    if (!isServer || !isRunning) {
        return;
    }
    
    RakNet::TimeMS now = RakNet::GetTimeMS();

    ScheduledPacket packet;
    packet.executionTime = now + delayMs;
    packet.targetAddress = target;

    // BitStream은 메인 루프에서 사라질 수 있으므로 반드시 복사해서 저장해야 함
    packet.dataStream = new RakNet::BitStream();
    bs->ResetReadPointer();
    packet.dataStream->Write(bs);
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        taskQueue.push(packet);
    }

    // 스레드가 자고 있다면 깨워서 새로운 가장 빠른 시간을 인지하게 함
    queueCondVar.notify_one();
}

void MethodManager::ThreadLoop() {
    while (isRunning) {
        ScheduledPacket task;
        bool taskFound = false;

        {
            std::unique_lock<std::mutex> lock(queueMutex);

            if (taskQueue.empty()) {
                // 큐가 비었으면 패킷이 들어올 때까지 대기
                queueCondVar.wait(lock);
            }
            else {
                RakNet::TimeMS now = RakNet::GetTimeMS();
                const ScheduledPacket& top = taskQueue.top();

                if (now >= top.executionTime) {
                    // 전송 시간이 되었으면 꺼냄
                    task = top;
                    taskQueue.pop();
                    taskFound = true;
                }
                else {
                    // 아직 시간이 안 되었으면, 남은 시간만큼 대기 (CPU 절약)
                    // wait_for는 지정된 시간만큼 자거나, 새 패킷이 들어오면 깸
                    auto waitDuration = std::chrono::milliseconds(top.executionTime - now);
                    queueCondVar.wait_for(lock, waitDuration);
                }
            }
        }

        // 락이 풀린 상태에서 전송 (RakPeer::Send는 Thread-Safe함)
        if (taskFound && isRunning) {
            // 주의: NetworkManager::Instance()->GetPeer()가 유효한지 확인 필요
            if (NetworkManager::Instance() && NetworkManager::Instance()->GetPeer()) {
                NetworkManager::Instance()->GetPeer()->Send(
                    task.dataStream,
                    HIGH_PRIORITY,
                    RELIABLE_ORDERED,
                    0,
                    task.targetAddress,
                    false // broadcast false
                );
            }
            // 힙에 할당한 BitStream 해제
            delete task.dataStream;
        }
    }
}

void MethodManager::RecordSendTime(RakNet::SystemAddress sa, int sequenceIndex, RakNet::TimeMS time, int methodType) {
    std::lock_guard<std::mutex> lock(mapMutex);
    RttKey key(sa, sequenceIndex);

    if (methodType == METHOD_1) {
        method1RttMap.Set(key, time);
    }
    else if (methodType == METHOD_3) {
        method3RttMap.Set(key, time);
    }
}

RakNet::TimeMS MethodManager::GetAndRemoveSendTime(RakNet::SystemAddress sa, int sequenceIndex, int methodType) {
    std::lock_guard<std::mutex> lock(mapMutex);

    RttKey key(sa, sequenceIndex);
    RakNet::TimeMS sentTime = 0;

    if (methodType == METHOD_1) {
        if (method1RttMap.Has(key)) {
            sentTime = method1RttMap.Get(key);
            method1RttMap.Delete(key); // 확인 후 삭제 (메모리 관리)
        }
    }
    else if (methodType == METHOD_3) {
        if (method3RttMap.Has(key)) {
            sentTime = method3RttMap.Get(key);
            method3RttMap.Delete(key);
        }
    }

    return sentTime; // 없으면 0 반환
}

void MethodManager::SendManagedPacket(RakNet::BitStream* bs, RakNet::SystemAddress target, int methodType) {

    // 1. Method 1(스폰 패킷)적용
    if (methodType == METHOD_1) {
        if (IsMethodActive(METHOD_1)) ExecuteMethod1(bs);
        else NetworkManager::Instance()->GetPeer()->Send(bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, UNASSIGNED_SYSTEM_ADDRESS, true);
        return;
    }

    // 2. 나머지 전체 Method 3 (혼잡 제어) 적용 / 입력, 이동, 총알 발사 등
    if (methodType == METHOD_3) {
        ExecuteMethod3(bs, target);
        return;
    }
    
    // 아무 메소드도 없으면 그냥 전송
	if (target == RakNet::UNASSIGNED_SYSTEM_ADDRESS) NetworkManager::Instance()->GetPeer()->Send(bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, UNASSIGNED_SYSTEM_ADDRESS, true);
	else NetworkManager::Instance()->GetPeer()->Send(bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, target , false);
}

void MethodManager::ExecuteMethod3(RakNet::BitStream* bs, RakNet::SystemAddress target) {
    // 현재 혼잡도(N_CI) 체크
    int currentCongestion = GetCongestedUserCount(); // 혹은 계산된 N_CI
    if (currentCongestion > 1) { // 혼잡 상황이면
        
        int calculatedDelay = CalculateDelayForPlayer(target);
        //스케줄러에 등록 (이제 1ms 단위 정밀도로 제어됨)
        MethodManager::Instance()->EnqueuePacket(target, bs, calculatedDelay);
    }
    else {
        // 혼잡하지 않으면 즉시 전송
        NetworkManager::Instance()->GetPeer()->Send(bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, target, false);
    }
}

void MethodManager::ExecuteMethod1(RakNet::BitStream* bs) {
    unsigned short numberOfSystems = NetworkManager::Instance()->GetPeer()->NumberOfConnections();
    RakNet::TimeMS now = RakNet::GetTimeMS();
    for (unsigned short i = 0; i < numberOfSystems; i++)
    {
        RakNet::SystemAddress sa = NetworkManager::Instance()->GetPeer()->GetSystemAddressFromIndex(i);
        RecordSendTime(sa, MethodManager::Instance()->method1cnt,now, METHOD_1);
        int calculatedDelay = CalculateDelayForPlayer(sa);
		//NetLogManager::Instance()->DebugPrintf("Method 1: Calculated Delay for %s is %d ms\n", sa.ToString(), calculatedDelay);
        
        // 스케줄러에 등록 (이제 1ms 단위 정밀도로 제어됨)
        MethodManager::Instance()->EnqueuePacket(sa, bs, calculatedDelay);
    }
}

int MethodManager::CalculateDelayForPlayer(RakNet::SystemAddress sa) {
    RakNet::RakPeerInterface* peer = NetworkManager::Instance()->GetPeer();
    if (!peer) return 0;

    // N_CI (혼잡 인덱스) 설정
    int N_CI = 1;
    if (IsMethodActive(METHOD_3)) {
        N_CI = GetCongestedUserCount();
        if (N_CI <= 0) return 0;
    }

    // 1. 해당 플레이어(Target)의 평균 RTT 가져오기 (RTT_f)
    int playerRTT = peer->GetAveragePing(sa);
    if (playerRTT <= 0) playerRTT = 1; // 0으로 나눔 방지 (최소 1ms 보장)

    // 2. 전체 통계 계산 (Global Average, Standard Deviation)
    std::vector<int> allRTTs;
    unsigned short numberOfSystems = peer->NumberOfConnections();
    double sumRTT = 0;

    for (unsigned short i = 0; i < numberOfSystems; i++) {
        RakNet::SystemAddress tempSA = peer->GetSystemAddressFromIndex(i);
        // 서버 자기 자신(UNASSIGNED)은 통계에서 제외 (보통 클라이언트만 계산)
        if (tempSA == RakNet::UNASSIGNED_SYSTEM_ADDRESS) continue;
        int rtt = peer->GetAveragePing(tempSA);
        if (rtt < 0) rtt = 0; // 유효하지 않은 핑 방지
        allRTTs.push_back(rtt);
        sumRTT += rtt;
    }
    if (allRTTs.empty()) return 0;
    double globalAvg = sumRTT / allRTTs.size(); 
    
    // 표준편차 (Standard Deviation, Sigma) 계산
    double varianceSum = 0;
    for (int rtt : allRTTs) {
        varianceSum += std::pow(rtt - globalAvg, 2);
    }
    double stdDev = std::sqrt(varianceSum / allRTTs.size());

    // 3. 임계값(Threshold) 계산: (전체평균 - 표준편차)
    double threshold = globalAvg - stdDev;
    // 4. 조건 확인: 플레이어 RTT가 임계값보다 작으면 (즉, 너무 빠르면) 딜레이 부여
    if (playerRTT <= threshold) {
        
        // 5. 공식 적용: Delay = N_CI * ((GlobalAvg - StdDev) / PlayerRTT)
         // 해석: 혼잡한 유저(N_CI)가 많을수록, 여유로운 유저는 더 많이 기다려주어야 함.
        // 공식의 결과는 '배수(Ratio)'
		
        int c = 10; // 기본 지연 단위 (예: 10ms)
        int maxDelay = 1000; // 최대 딜레이 제한 (예: 1s)

		double calculatedDelayRatio = N_CI * (threshold / (double)playerRTT); //상황의 심각성 * 불공정 비율
		//double calculatedDelaydiff = N_CI * (threshold - playerRTT);

		int finalDelay = static_cast<int>(calculatedDelayRatio * c); //Method 1
        //int finalDelay = static_cast<int>(calculatedDelaydiff);

        if (finalDelay > maxDelay) finalDelay = maxDelay;
		return finalDelay; 
    }

    // 조건에 해당하지 않으면 딜레이 없음
    return 0; 
}

void MethodManager::UpdateMethod3() {
    RakNet::RakPeerInterface* peer = NetworkManager::Instance()->GetPeer();
    if (!peer) return;

    unsigned short numberOfSystems = peer->NumberOfConnections();
    if (numberOfSystems == 0) return;

    double N_c = (double)numberOfSystems;
    int tempCongestedCount = 0; // 이번 프레임의 N_CI 카운트

    std::lock_guard<std::mutex> lock(congestionMutex);

    for (unsigned short i = 0; i < numberOfSystems; i++) {
        RakNet::SystemAddress sa = peer->GetSystemAddressFromIndex(i);
        if (sa == RakNet::UNASSIGNED_SYSTEM_ADDRESS) continue;

        RakNet::RakNetStatistics stats;
        if (peer->GetStatistics(sa, &stats)) {

            // 1. Queue Length (Q_fi) 계산 / 우선순위별 큐의 바이트 합산
            // ---------------------------------------------------------
            double Q_fi = 0;
            for (int p = 0; p < NUMBER_OF_PRIORITIES; p++) {
                Q_fi += stats.bytesInSendBuffer[p];
            }
            // 2. Flow Rate (FR_fi) 계산
            // RakNetStatistics.h 참조: 지난 1초간 전송된 유저 메시지 바이트 수
            double FR_fi = (double)stats.valueOverLastSecond[USER_MESSAGE_BYTES_SENT];

            // 3. CI_f (혼잡 인덱스) 계산: Queue / FlowRate
            // 단위: Bytes / (Bytes/sec) = Seconds (큐를 비우는 데 걸리는 시간)
            double CI_f = 0.0;
            if (FR_fi > 0.0) {
                CI_f = Q_fi / FR_fi;
            }
            else {
                // 전송 속도가 0인데 큐에 데이터가 있다면 매우 혼잡함 (무한대)
                if (Q_fi > 0) CI_f = 100.0;
                else CI_f = 0.0;
            }

            // 4. Burst 및 혼잡 상태 판별
            double burstMultiplier = 1.0;
            bool isCongested = false;

            if (CI_f <= 1.0) {
                // 비혼잡 상태
                burstMultiplier = 1.0;
                isCongested = false;
            }
            else {
                // 혼잡 상태 (CI > 1.0) -> 리소스 보장 필요
                // 공식: Burst_e = [1 + (1/Nc) * Max(2, CI_f)] * Burst_d
                double maxVal = (CI_f > 2.0) ? CI_f : 2.0;
                burstMultiplier = 1.0 + (1.0 / N_c) * maxVal;

                isCongested = true;
                tempCongestedCount++; // N_CI 증가
            }

            // 상태 저장
            playerCongestionMap.Set(sa, { CI_f, burstMultiplier, isCongested });
        }
    }

    // 최종 N_CI 업데이트 (Method 1에서 사용)
    currentCongestedUserCount = tempCongestedCount;
}

int MethodManager::GetCongestedUserCount() {
	return currentCongestedUserCount;
}
