#include "MethodManager.h"
#include "NetworkManager.h"
#include "NetLogManager.h"
#include "CInGame.h"
#include <cmath>
#include <vector>
#include <numeric>
#include <utility>
#include <tuple>

using namespace RakNet;
using namespace irr;

// RTT Key Comparison
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
        instance->queueCondVar.notify_all();

        if (instance->schedulerThread && instance->schedulerThread->joinable()) {
            instance->schedulerThread->join();
            delete instance->schedulerThread;
        }

        {
            std::lock_guard<std::mutex> lock(instance->queueMutex);

            // 큐 비우기 작업
            while (!instance->taskQueue.empty()) {
                ScheduledPacket packet = instance->taskQueue.top();
                instance->taskQueue.pop();
                if (packet.dataStream) delete packet.dataStream;
            }

            for (unsigned int i = 0; i < instance->playerQueues.Size(); i++) {
                PlayerPacketQueue* pq = instance->playerQueues[i];
                if (pq) {
                    while (!pq->packetQ.empty()) {
                        RakNet::BitStream* bs = pq->packetQ.front();
                        pq->packetQ.pop();
                        if (bs) delete bs;
                    }
                    delete pq;
                }
            }
            instance->playerQueues.Clear();
        }
        delete instance;
        instance = nullptr;
    }
}

MethodManager::MethodManager() {
    if ( instance == nullptr ) instance = this;
    currentMethodsBitmask = 0;
    currentMethodsLogBitmask = 0;
    currentCongestedUserCount = 0;
    reactionTime = 0;
    scenarioNum = SCENARIO_NONE;
    botRespawnFlag = false;
    method1cnt = 0;
    method3cnt = 0;
    CongestionLogTime = 0;
    um_cnt = 0;
    am_cnt = 0;
    sumScore = 0;
    schedulerThread = nullptr;
    isServer = false;
}

MethodManager::~MethodManager() {
    RakNet::TimeMS currentTime = RakNet::GetTimeMS();
    if (IsMethodActive(METHOD_3) && currentTime - CongestionLogTime >= 1000) {
        if (IsMethodLogActive(METHOD_3)) NetLogManager::Instance()->SaveLogsToCSV(3);
        CongestionLogTime = currentTime;
    }
}

void MethodManager::Initialize(int methodMask, int methodLogMask, ScenarioNum scenario, bool isServer) {
    currentMethodsBitmask = methodMask;
    currentMethodsLogBitmask = methodLogMask;
    scenarioNum = scenario;
    this->isServer = isServer;

    if (this->isServer) {
        isRunning = true;
        schedulerThread = new std::thread(&MethodManager::ThreadLoop, this);
    }
}

void MethodManager::Activate() {}

int MethodManager::ConvertMethodToIndex(int method) {
    switch (method) {
    case METHOD_1: return 1;
    case METHOD_2: return 2;
    case METHOD_3: return 3;
    default: return 0;
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
    if (IsMethodActive(METHOD_1)) if (botRespawnFlag) method1cnt++;

    if (IsMethodActive(METHOD_3)) {
        RakNet::TimeMS currentTime = RakNet::GetTimeMS();
        if (currentTime - CongestionLogTime >= 1000) {
            UpdateCongetstionIndex(); // CI Burst (Zero-Sum)
            CongestionLogTime = currentTime;
        }
    }
}

void MethodManager::EnqueuePacket(RakNet::SystemAddress target, RakNet::BitStream* bs, RakNet::TimeMS delayMs) {
    if (!isServer || !isRunning) return;

    RakNet::BitStream* copiedStream = new RakNet::BitStream();
    bs->ResetReadPointer();
    copiedStream->Write(bs);

    std::lock_guard<std::mutex> lock(queueMutex);

    if (IsMethodActive(METHOD_3)) {
        if (!playerQueues.Has(target)) playerQueues.Set(target, new PlayerPacketQueue());
        playerQueues.Get(target)->packetQ.push(copiedStream);
    }
    else {
        ScheduledPacket packet;
        packet.executionTime = RakNet::GetTimeMS() + delayMs;
        packet.targetAddress = target;
        packet.dataStream = copiedStream;
        taskQueue.push(packet);
    }

    queueCondVar.notify_one();
}

void MethodManager::ThreadLoop() {
    while (isRunning) {
        // Temp container for outgoing packets
        std::vector<std::pair<RakNet::SystemAddress, RakNet::BitStream*>> outgoingPackets;
        ScheduledPacket method1Task;
        bool hasMethod1Task = false;

        {
            std::unique_lock<std::mutex> lock(queueMutex);

            // [Method 3] Burst Processing
            if (IsMethodActive(METHOD_3)) {
                // Keep lock while iterating map
                for (unsigned int i = 0; i < playerQueues.Size(); ++i) {
                    RakNet::SystemAddress sa = playerQueues.GetKeyAtIndex(i);
                    PlayerPacketQueue* pq = playerQueues[i];

                    if (!pq || pq->packetQ.empty()) continue;

                    PlayerCongestionState state;
                    double burstLimit = DEFAULT_BURST_D;

                    // Burst limit based on congestion state
                    if (GetPlayerCongestionState(sa, state)) {
                        burstLimit = state.burstMultiplier;
                    }

                    if (burstLimit <= 0.1) burstLimit = 0.1;

                    pq->currentCredit += burstLimit;

                    // Move packets to temp container based on credit
                    while (pq->currentCredit >= 1.0 && !pq->packetQ.empty()) {
                        RakNet::BitStream* bs = pq->packetQ.front();
                        pq->packetQ.pop();

                        outgoingPackets.push_back({ sa, bs });

                        pq->currentCredit -= 1.0;
                    }

                    // Cap credit to prevent accumulation
                    if (pq->currentCredit > 5.0) pq->currentCredit = 5.0;
                }
            }

            // [Method 1] Time-based Processing
            bool shouldWait = true;
            if (isRunning && !taskQueue.empty()) {
                RakNet::TimeMS now = RakNet::GetTimeMS();
                const ScheduledPacket& top = taskQueue.top();

                if (now >= top.executionTime) {
                    method1Task = top;
                    taskQueue.pop();
                    hasMethod1Task = true;
                    shouldWait = false;
                }
                else {
                    // Time not met yet
                    if (!outgoingPackets.empty()) {
                        shouldWait = false;
                    }
                    else if (!IsMethodActive(METHOD_3)) {
                        // Wait until next scheduled time
                        auto waitDuration = std::chrono::milliseconds(top.executionTime - now);
                        queueCondVar.wait_for(lock, waitDuration);
                        shouldWait = false;
                    }
                }
            }

            // Wait condition
            if (shouldWait && !IsMethodActive(METHOD_3) && playerQueues.Size() == 0 && outgoingPackets.empty() && !hasMethod1Task) {
                queueCondVar.wait(lock);
            }
        } // Lock released

        // [Transmission Step] - Send safely without lock
        if (isRunning) {
            // Method 3 Packet Batch Send
            for (auto& packetInfo : outgoingPackets) {
                if (NetworkManager::Instance() && NetworkManager::Instance()->GetPeer()) {
                    NetworkManager::Instance()->GetPeer()->Send(
                        packetInfo.second, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packetInfo.first, false
                    );
                }
                delete packetInfo.second;
            }

            // Method 1 Packet Send
            if (hasMethod1Task) {
                if (NetworkManager::Instance() && NetworkManager::Instance()->GetPeer()) {
                    NetworkManager::Instance()->GetPeer()->Send(
                        method1Task.dataStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, method1Task.targetAddress, false
                    );
                }
                delete method1Task.dataStream;
            }
        }
        else {
            // Cleanup on exit
            for (auto& packetInfo : outgoingPackets) delete packetInfo.second;
            if (hasMethod1Task) delete method1Task.dataStream;
        }

        // CPU yielding for Method 3
        if (IsMethodActive(METHOD_3) && isRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void MethodManager::RecordSendTime(RakNet::SystemAddress sa, int sequenceIndex, RakNet::TimeMS time, int methodType) {
    std::lock_guard<std::mutex> lock(mapMutex);
    RttKey key(sa, sequenceIndex);
    if (methodType == METHOD_1) method1RttMap.Set(key, time);
    else if (methodType == METHOD_3) method3RttMap.Set(key, time);
}

RakNet::TimeMS MethodManager::GetAndRemoveSendTime(RakNet::SystemAddress sa, int sequenceIndex, int methodType) {
    std::lock_guard<std::mutex> lock(mapMutex);
    RttKey key(sa, sequenceIndex);
    RakNet::TimeMS sentTime = 0;
    if (methodType == METHOD_1) {
        if (method1RttMap.Has(key)) { sentTime = method1RttMap.Get(key); method1RttMap.Delete(key); }
    }
    else if (methodType == METHOD_3) {
        if (method3RttMap.Has(key)) { sentTime = method3RttMap.Get(key); method3RttMap.Delete(key); }
    }
    return sentTime;
}

void MethodManager::SendManagedPacket(RakNet::BitStream* bs, RakNet::SystemAddress target, int methodType) {
    if (methodType == METHOD_1) {
        if (IsMethodActive(METHOD_1)) ExecuteMethod1(bs);
        else NetworkManager::Instance()->GetPeer()->Send(bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, UNASSIGNED_SYSTEM_ADDRESS, true);
        return;
    }

    if (methodType == METHOD_3) {
        ExecuteMethod3(bs, target);
        return;
    }

    if (target == RakNet::UNASSIGNED_SYSTEM_ADDRESS)
        NetworkManager::Instance()->GetPeer()->Send(bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, UNASSIGNED_SYSTEM_ADDRESS, true);
    else
        NetworkManager::Instance()->GetPeer()->Send(bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, target, false);
}

void MethodManager::ExecuteMethod3(RakNet::BitStream* bs, RakNet::SystemAddress target) {
    EnqueuePacket(target, bs, 0);
}

void MethodManager::ExecuteMethod1(RakNet::BitStream* bs) {
    unsigned short numberOfSystems = NetworkManager::Instance()->GetPeer()->NumberOfConnections();
    RakNet::TimeMS now = RakNet::GetTimeMS();
    for (unsigned short i = 0; i < numberOfSystems; i++) {
        RakNet::SystemAddress sa = NetworkManager::Instance()->GetPeer()->GetSystemAddressFromIndex(i);
        RecordSendTime(sa, MethodManager::Instance()->method1cnt, now, METHOD_1);
        int calculatedDelay = CalculateDelayForPlayer(sa);
        MethodManager::Instance()->EnqueuePacket(sa, bs, calculatedDelay);
    }
}

int MethodManager::CalculateDelayForPlayer(RakNet::SystemAddress sa) {
    // [Method 1] RTT Check
    RakNet::RakPeerInterface* peer = NetworkManager::Instance()->GetPeer();
    if (!peer) return 0;


    int N_CI = GetCongestedUserCount();
    if (N_CI < 1) N_CI = 1;

    int playerRTT = peer->GetAveragePing(sa);
    if (playerRTT <= 0) playerRTT = 1;


    std::vector<int> allRTTs;
    unsigned short numberOfSystems = peer->NumberOfConnections();
    double sumRTT = 0;

    for (unsigned short i = 0; i < numberOfSystems; i++) {
        RakNet::SystemAddress tempSA = peer->GetSystemAddressFromIndex(i);
        if (tempSA == RakNet::UNASSIGNED_SYSTEM_ADDRESS) continue;
        int rtt = peer->GetAveragePing(tempSA);
        if (rtt < 0) rtt = 0;
        allRTTs.push_back(rtt);
        sumRTT += rtt;
    }
    if (allRTTs.empty()) return 0;

    double globalAvg = sumRTT / allRTTs.size();
    double varianceSum = 0;
    for (int rtt : allRTTs) { varianceSum += std::pow(rtt - globalAvg, 2); }
    double stdDev = std::sqrt(varianceSum / allRTTs.size());

    double threshold = globalAvg - stdDev;


    if (playerRTT <= threshold) {
        int c = 10;
        int maxDelay = 1000;

        // RTT Delay Calculation
        double calculatedDelayRatio = N_CI * (threshold / (double)playerRTT);
        int finalDelay = static_cast<int>(calculatedDelayRatio * c);

        if (finalDelay > maxDelay) finalDelay = maxDelay;
        return finalDelay;
    }
    return 0;
}

bool MethodManager::GetPlayerCongestionState(RakNet::SystemAddress sa, PlayerCongestionState& outState) {
    std::lock_guard<std::mutex> lock(congestionMutex);
    if (playerCongestionMap.Has(sa)) {
        outState = playerCongestionMap.Get(sa);
        return true;
    }
    return false;
}

void MethodManager::UpdateCongetstionIndex() {
    RakNet::RakPeerInterface* peer = NetworkManager::Instance()->GetPeer();
    if (!peer) return;

    unsigned short numberOfSystems = peer->NumberOfConnections();
    if (numberOfSystems == 0) return;

    int tempCongestedCount = 0;

    // 2-Pass algorithm temporary storage
    struct UserStat {
        RakNet::SystemAddress sa;
        double CI_f;
    };
    std::vector<UserStat> userStats;

    // Pass 1: CI calculation and N_c count
    for (unsigned short i = 0; i < numberOfSystems; i++) {
        RakNet::SystemAddress sa = peer->GetSystemAddressFromIndex(i);
        if (sa == RakNet::UNASSIGNED_SYSTEM_ADDRESS) continue;

        RakNet::RakNetStatistics stats;
        if (peer->GetStatistics(sa, &stats)) {
            // 1. Queue Length (Q_fi)
            double Q_fi = stats.bytesInResendBuffer;
            for (int p = 0; p < NUMBER_OF_PRIORITIES; p++) Q_fi += stats.bytesInSendBuffer[p];

            // 2. Flow Rate (FR_fi)
            double FR_fi = (double)stats.valueOverLastSecond[USER_MESSAGE_BYTES_SENT];

            // 3. CI_f
            double CI_f = (FR_fi > 0.0) ? (Q_fi / FR_fi) : ((Q_fi > 0) ? 100.0 : 0.0);

            if (CI_f > 1.0) {
                tempCongestedCount++;
            }
            userStats.push_back({ sa, CI_f });
        }
    }

    // Update N_CI
    currentCongestedUserCount = tempCongestedCount;

    // Pass 2: Burst Calc and Zero-Sum
    double N_c = (tempCongestedCount > 0) ? (double)tempCongestedCount : 1.0;

    double totalExcessBurst = 0.0;
    int normalUserCount = (int)userStats.size() - tempCongestedCount;

    std::vector<std::tuple<RakNet::SystemAddress, double, double, bool>> finalUpdateList;

    for (const auto& u : userStats) {
        double calculatedBurst = DEFAULT_BURST_D;
        bool isCongested = (u.CI_f > 1.0);

        if (isCongested) {
            // Congested user: Bonus
            double maxVal = (u.CI_f > 2.0) ? u.CI_f : 2.0;
            calculatedBurst = (1.0 + (1.0 / N_c) * maxVal) * DEFAULT_BURST_D;
            totalExcessBurst += (calculatedBurst - DEFAULT_BURST_D);
        }

        finalUpdateList.push_back(std::make_tuple(u.sa, u.CI_f, calculatedBurst, isCongested));
    }

    // Pass 3: Update map
    std::lock_guard<std::mutex> lock(congestionMutex);

    double penaltyPerNormalUser = 0.0;
    if (normalUserCount > 0 && totalExcessBurst > 0.0) {
        penaltyPerNormalUser = totalExcessBurst / (double)normalUserCount;
    }
    

    for (const auto& item : finalUpdateList) {
        RakNet::SystemAddress sa = std::get<0>(item);
        double ci = std::get<1>(item);
        double burst = std::get<2>(item);
        bool congested = std::get<3>(item);

        if (!congested) {
            // Non-congested: Penalty
            burst -= penaltyPerNormalUser;
            // Min guarantee (20%)
            if (burst < 0.2 * DEFAULT_BURST_D) burst = 0.2 * DEFAULT_BURST_D;
        }

        playerCongestionMap.Set(sa, { ci, burst, congested });
    }
}

int MethodManager::GetCongestedUserCount() {
    return currentCongestedUserCount;
}