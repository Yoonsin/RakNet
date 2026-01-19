#pragma once
#include "DS_Heap.h"
#include "DS_Map.h"
#include "GetTime.h"
#include "RakNetTypes.h"
#include "BitStream.h" 
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <atomic>

// 비트 플래그 상수
// (1 << 0) = 1
// (1 << 1) = 2
// (1 << 2) = 4
constexpr int METHOD_1 = 1;
constexpr int METHOD_2 = 2;
constexpr int METHOD_3 = 4;

// [Method 1용] RTT 정보를 위한 키 구조체
struct RttKey {
	RakNet::SystemAddress systemAddress;
	int sequenceIndex; // 몇 번째 패킷인지 식별 (중복 방지)
	RttKey() : sequenceIndex(0) { systemAddress = RakNet::UNASSIGNED_SYSTEM_ADDRESS; }
	RttKey(RakNet::SystemAddress sa, int seq) : systemAddress(sa), sequenceIndex(seq) {}
};
int RttKeyComparison(const RttKey& key1, const RttKey& key2);

// [Method 1용] 지연 전송을 위한 스케줄링 패킷 구조체
struct ScheduledPacket {
	RakNet::TimeMS executionTime; // 전송되어야 할 절대 시간
	RakNet::SystemAddress targetAddress; // 목적지 주소
	RakNet::BitStream* dataStream; // 패킷 데이터 (사본)

	// 우선순위 큐 정렬용 (시간이 빠른 순서대로)
	bool operator>(const ScheduledPacket& other) const {
		return executionTime > other.executionTime;
	}
};

// [Method 3용] 플레이어별 패킷 큐 및 Burst 크레딧
struct PlayerPacketQueue {
	std::queue<RakNet::BitStream*> packetQ;
	double currentCredit = 0.0; // 현재 전송 가능한 누적 크레딧
};

// 플레이어별 혼잡도 상태 정보
struct PlayerCongestionState {
	double congestionIndex; // CI_f
	double burstMultiplier; // 최종 계산된 Burst 값
	bool isCongested;       // CI > 1.0 여부
};

// [Method 2용] 정렬 데이터를 위한 구조체 (기존 유지)
struct orderData {
	int um_cnt;
	int am_cnt;
	int processTime;
	RakNet::TimeMS reactionTime;
	RakNet::TimeMS ingoingTime;
	RakNet::TimeMS timeOut;
	RakNet::RakNetGUID playerGUID;
	bool isSequenced;
};

class MethodManager
{
public:
	static MethodManager* Instance();
	static void DestroyInstance();

	void Initialize(int methodMask, int methodLogMask, int scenario, bool isServer);
	void Activate();
	void UpdateMethod(int methodFlag);

	bool IsMethodActive(int methodFlag) const;
	bool IsMethodLogActive(int methodLogFlag) const;
	bool isMethodZero() const { return currentMethodsBitmask == 0; }

	// 패킷 전송 인터페이스
	void SendManagedPacket(RakNet::BitStream* bs, RakNet::SystemAddress target, int methodType);
	void EnqueuePacket(RakNet::SystemAddress target, RakNet::BitStream* bs, RakNet::TimeMS delayMs);

	// 내부 로직 (Method 1: Delay / Method 3: Burst)
	void ExecuteMethod1(RakNet::BitStream* bs);
	void ExecuteMethod3(RakNet::BitStream* bs, RakNet::SystemAddress target);

	// 알고리즘 로직
	int CalculateDelayForPlayer(RakNet::SystemAddress sa); // Method 1 전용
	void UpdateCongetstionIndex(); // Method 3 전용 (Zero-Sum Burst)

	// 유틸리티 및 Getter/Setter
	void RecordSendTime(RakNet::SystemAddress sa, int sequenceIndex, RakNet::TimeMS time, int methodType);
	RakNet::TimeMS GetAndRemoveSendTime(RakNet::SystemAddress sa, int sequenceIndex, int methodType);
	int GetCongestedUserCount();
	bool GetPlayerCongestionState(RakNet::SystemAddress sa, PlayerCongestionState& outState);
	static int ConvertMethodToIndex(int method);
	int GetSequenceIndexForMethod(int method);

	// Public 멤버 변수
	bool method1bool;
	int method1cnt;
	int method3cnt;
	int currentCongestedUserCount;

	DataStructures::Map<RttKey, RakNet::TimeMS, RttKeyComparison> method1RttMap;
	DataStructures::Map<RttKey, RakNet::TimeMS, RttKeyComparison> method3RttMap;
	std::mutex mapMutex;
	int scenarioNum;

	// Method 2 관련 변수 (기존 유지)
	int um_cnt;
	int am_cnt;
	int sumScore;
	RakNet::TimeMS reactionTime;
	DataStructures::Heap<uint64_t, orderData, false> orderPq;
	DataStructures::Map<int, RakNet::TimeMS> umTimeMap;
	DataStructures::Map<int, RakNet::TimeMS> umReceptionTimes;

private:
	MethodManager();
	~MethodManager();
	void ThreadLoop();

	static MethodManager* instance;
	int currentMethodsBitmask;
	int currentMethodsLogBitmask;
	RakNet::TimeMS CongestionLogTime;
	bool isServer;

	// [Method 1] 시간 기반 우선순위 큐
	std::priority_queue<ScheduledPacket, std::vector<ScheduledPacket>, std::greater<ScheduledPacket>> taskQueue;

	// [Method 3] 플레이어별 라운드 로빈 큐
	DataStructures::Map<RakNet::SystemAddress, PlayerPacketQueue*> playerQueues;

	std::mutex queueMutex;
	std::condition_variable queueCondVar;
	std::thread* schedulerThread;
	std::atomic<bool> isRunning;

	std::mutex congestionMutex;
	DataStructures::Map<RakNet::SystemAddress, PlayerCongestionState> playerCongestionMap;

	const double DEFAULT_BURST_D = 1.0;
};