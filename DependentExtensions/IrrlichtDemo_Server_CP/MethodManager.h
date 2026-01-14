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
// 각 메소드를 나타내는 비트 플래그 정의
// (1 << 0) = 1 (001)
// (1 << 1) = 2 (010)
// (1 << 2) = 4 (100)
constexpr int METHOD_1 = 1;
constexpr int METHOD_2 = 2;
constexpr int METHOD_3 = 4;

struct ScheduledPacket {
	RakNet::TimeMS executionTime; // 전송되어야 할 절대 시간
	RakNet::SystemAddress targetAddress; // 받을 대상
	RakNet::BitStream* dataStream; // 패킷 데이터 (복사본)

	// 우선순위 큐 정렬을 위한 연산자 (시간이 작은 것이 위로 오게)
	bool operator>(const ScheduledPacket& other) const {
		return executionTime > other.executionTime;
	}
};

struct PlayerCongestionState {
	double congestionIndex; // CI_f
	double burstMultiplier; // Burst 배율
	bool isCongested;       // CI > 1.0 여부
};

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

struct RttKey {
	RakNet::SystemAddress systemAddress;
	int sequenceIndex; // 몇 번째 패킷인지 식별 (중복 방지)

	// 기본 생성자
	RttKey() : sequenceIndex(0) {
		systemAddress = RakNet::UNASSIGNED_SYSTEM_ADDRESS;
	}
	RttKey(RakNet::SystemAddress sa, int seq) : systemAddress(sa), sequenceIndex(seq) {}
};

int RttKeyComparison(const RttKey& key1, const RttKey& key2);

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
	void ExecuteMethod1(RakNet::BitStream* bs);
	void ExecuteMethod3(RakNet::BitStream* bs, RakNet::SystemAddress target);
	void SendManagedPacket(RakNet::BitStream* bs, RakNet::SystemAddress target, int methodType);
	int CalculateDelayForPlayer(RakNet::SystemAddress sa);
	void EnqueuePacket(RakNet::SystemAddress target, RakNet::BitStream* bs, RakNet::TimeMS delayMs);
	void RecordSendTime(RakNet::SystemAddress sa, int sequenceIndex, RakNet::TimeMS time, int methodType);
	RakNet::TimeMS GetAndRemoveSendTime(RakNet::SystemAddress sa, int sequenceIndex, int methodType);
	void UpdateMethod3();
	int GetCongestedUserCount();
	static int ConvertMethodToIndex(int method);
	int GetSequenceIndexForMethod(int method);
	
	bool method1bool;
	int method1cnt;
	int currentCongestedUserCount;
	int method3cnt;

	int um_cnt;
	int am_cnt;
	int sumScore;
	RakNet::TimeMS reactionTime;
  
	DataStructures::Heap<uint64_t, orderData, false> orderPq;
	DataStructures::Map<int, RakNet::TimeMS> umTimeMap;
	DataStructures::Map<int, RakNet::TimeMS> umReceptionTimes;
	DataStructures::Map<RttKey, RakNet::TimeMS, RttKeyComparison> method1RttMap;
	DataStructures::Map<RttKey, RakNet::TimeMS, RttKeyComparison> method3RttMap;
	std::mutex mapMutex;  // 스레드 안전성을 위해 뮤텍스 권장
	int scenarioNum;      // 현재 실험 시나리오 번호 (0은 자유 1~3은 시나리오와 제일 관련이 깊은 메소드 번호, )
private:
	MethodManager();
	~MethodManager();
	void ThreadLoop();
	static MethodManager* instance;
	int currentMethodsBitmask;		// 활성화된 메소드 비트마스크
	int currentMethodsLogBitmask;	// 메소드 로그 기록 비트마스크
	
	RakNet::TimeMS method3LogTime;
	bool isServer;

	std::priority_queue<ScheduledPacket, std::vector<ScheduledPacket>, std::greater<ScheduledPacket>> taskQueue;
	std::mutex queueMutex;
	std::condition_variable queueCondVar;
	std::thread* schedulerThread;
	std::atomic<bool> isRunning;
	
	std::mutex congestionMutex;
	DataStructures::Map<RakNet::SystemAddress, PlayerCongestionState> playerCongestionMap;
	const double DEFAULT_BURST_D = 1.0;
};

