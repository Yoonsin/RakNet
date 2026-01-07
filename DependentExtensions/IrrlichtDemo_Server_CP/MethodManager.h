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
const int METHOD_1 = 1;
const int METHOD_2 = 2;
const int METHOD_3 = 4;

struct ScheduledPacket {
	RakNet::TimeMS executionTime; // 전송되어야 할 절대 시간
	RakNet::SystemAddress targetAddress; // 받을 대상
	RakNet::BitStream* dataStream; // 패킷 데이터 (복사본)

	// 우선순위 큐 정렬을 위한 연산자 (시간이 작은 것이 위로 오게)
	bool operator>(const ScheduledPacket& other) const {
		return executionTime > other.executionTime;
	}
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

int SystemAddressComparison(const RakNet::SystemAddress& key1, const RakNet::SystemAddress& key2);

class MethodManager
{
public:
	static MethodManager* Instance();
	static void DestroyInstance();
	void Initialize(int methodMask, bool isServer);
	void Activate();
	void UpdateMethods(int activeMethodsBitmask);
	void UpdateMethod(int methodFlag);
	bool IsMethodActive(int methodFlag) const;
	bool isMethodZero() const { return currentMethodsBitmask == 0; }
	void ExecuteMethod1(RakNet::BitStream* bs);
	int CalculateDelayForPlayer(RakNet::SystemAddress sa);
	void EnqueuePacket(RakNet::SystemAddress target, RakNet::BitStream* bs, RakNet::TimeMS delayMs);
	void RecordSendTime(RakNet::SystemAddress sa, RakNet::TimeMS time);
	RakNet::TimeMS GetAndRemoveSendTime(RakNet::SystemAddress sa);

	bool method1bool;
	int eval1cnt;

	int um_cnt;
	int am_cnt;
	int sumScore;
	RakNet::TimeMS reactionTime = 0;
	DataStructures::Heap<uint64_t, orderData, false> orderPq;
	DataStructures::Map<int, RakNet::TimeMS> umTimeMap;
	DataStructures::Map<int, RakNet::TimeMS> umReceptionTimes;
	DataStructures::Map<RakNet::SystemAddress, RakNet::TimeMS, SystemAddressComparison> packetSendTimeMap;  //Key: 플레이어 주소, Value: 보낸 시간(ms)
	std::mutex mapMutex; // 스레드 안전성을 위해 뮤텍스 권장

private:
	MethodManager();
	~MethodManager();
	void ThreadLoop();
	static MethodManager* instance;
	int currentMethodsBitmask;
	RakNet::TimeMS eval3LogTime;
	bool isServer;

	std::priority_queue<ScheduledPacket, std::vector<ScheduledPacket>, std::greater<ScheduledPacket>> taskQueue;
	std::mutex queueMutex;
	std::condition_variable queueCondVar;
	std::thread* schedulerThread;
	std::atomic<bool> isRunning;
	
};

