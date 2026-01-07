#include "MethodManager.h"
#include "NetworkManager.h"
#include "NetLogManager.h"
#include "CInGame.h"
#include <cmath>
#include <vector>
#include <numeric>

using namespace RakNet;
using namespace irr;

int SystemAddressComparison(const RakNet::SystemAddress& key1, const RakNet::SystemAddress& key2) {
    if (key1 > key2) return 1;
    if (key1 < key2) return -1;
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
    method1bool = false;
    eval1cnt = 0;
    eval3LogTime = 0;
    um_cnt = 0;
    am_cnt = 0; 
    sumScore = 0; 
	schedulerThread = nullptr;
}

MethodManager::~MethodManager() {
    RakNet::TimeMS currentTime = RakNet::GetTimeMS();
    if (IsMethodActive(METHOD_3) && currentTime - eval3LogTime >= 1000) // 1000ms = 1초
    {
        NetLogManager::Instance()->SaveLogsToCSV(3);
        eval3LogTime = currentTime;
    }
}

void MethodManager::Initialize(int methodMask, bool isServer) {
    currentMethodsBitmask = methodMask;
	this->isServer = isServer;
    if (IsMethodActive(METHOD_1)) CInGame::Instance()->BOT_MOVE_TIME = 1000;

    if (this->isServer) {
        isRunning = true;
        schedulerThread = new std::thread(&MethodManager::ThreadLoop, this);
    }
}

void MethodManager::Activate() {
}

bool MethodManager::IsMethodActive(int methodFlag) const {
	return (currentMethodsBitmask & methodFlag) != 0;
}
void MethodManager::UpdateMethod(int methodFlag) {
	
    if(methodFlag & METHOD_1) if (method1bool) eval1cnt++;
    if (methodFlag & METHOD_3) {
        RakNet::TimeMS currentTime = RakNet::GetTimeMS();
        if (currentTime - eval3LogTime >= 1000) // 1000ms = 1초
        {
			NetLogManager::Instance()->SaveLogsToCSV(3); // Method 3 로그 저장
        	eval3LogTime = currentTime;
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

void MethodManager::RecordSendTime(RakNet::SystemAddress sa, RakNet::TimeMS time) {
    std::lock_guard<std::mutex> lock(mapMutex);
    packetSendTimeMap.Set(sa, time);
}

RakNet::TimeMS MethodManager::GetAndRemoveSendTime(RakNet::SystemAddress sa) {
    std::lock_guard<std::mutex> lock(mapMutex);

    if (packetSendTimeMap.Has(sa)) {
        RakNet::TimeMS sentTime = packetSendTimeMap.Get(sa);
        packetSendTimeMap.Delete(sa); // 확인 후 삭제
        return sentTime;
    }
    return 0; // 기록 없음
}

void MethodManager::ExecuteMethod1(RakNet::BitStream* bs) {
    unsigned short numberOfSystems = NetworkManager::Instance()->GetPeer()->NumberOfConnections();
    RakNet::TimeMS now = RakNet::GetTimeMS();
    for (unsigned short i = 0; i < numberOfSystems; i++)
    {
        RakNet::SystemAddress sa = NetworkManager::Instance()->GetPeer()->GetSystemAddressFromIndex(i);
        RecordSendTime(sa, now);
        int calculatedDelay = CalculateDelayForPlayer(sa);
		//NetLogManager::Instance()->DebugPrintf("Method 1: Calculated Delay for %s is %d ms\n", sa.ToString(), calculatedDelay);
        
        // 스케줄러에 등록 (이제 1ms 단위 정밀도로 제어됨)
        MethodManager::Instance()->EnqueuePacket(sa, bs, calculatedDelay);
    }
}

int MethodManager::CalculateDelayForPlayer(RakNet::SystemAddress sa) {
    RakNet::RakPeerInterface* peer = NetworkManager::Instance()->GetPeer();
    if (!peer) return 0;

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
        // N_CI (혼잡 인덱스) 설정
        int N_CI = 1;
        if (IsMethodActive(METHOD_3)) {
            //TODO: N_CI = this->currentCongestedUserCount;
            N_CI = 1;
        }

        // 5. 공식 적용: Delay = N_CI * ((GlobalAvg - StdDev) / PlayerRTT)
        // 주의: 결과가 실수(float)로 나오므로 int로 캐스팅됩니다.
        // 공식의 결과는 '배수(Ratio)'
		
        int c = 1; // 기본 지연 단위 (예: 10ms)
		double calculatedDelayRatio = N_CI * (threshold / (double)playerRTT); //상황의 심각성 * 불공정 비율
		//double calculatedDelaydiff = N_CI * (threshold - playerRTT);

		int maxDelay = 1000; // 최대 딜레이 제한 (예: 1s)
		int finalDelay = static_cast<int>(calculatedDelayRatio * c);  //return static_cast<int>(calculatedDelaydiff);
		if (finalDelay > maxDelay) finalDelay = maxDelay;

		return finalDelay; 
    }

    // 조건에 해당하지 않으면 딜레이 없음
    return 0; 
}

