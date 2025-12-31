#pragma once

// 각 메소드를 나타내는 비트 플래그 정의
// (1 << 0) = 1 (001)
// (1 << 1) = 2 (010)
// (1 << 2) = 4 (100)
const int METHOD_1 = 1;
const int METHOD_2 = 2;
const int METHOD_3 = 4;

class MethodManager
{
public:
	static MethodManager* Instance();
	static void DestroyInstance();
	void Initialize(int methodMask);
	void Activate();
	void UpdateMethods(int activeMethodsBitmask);
	void UpdateMethod(int methodFlag);
	bool IsMethodActive(int methodFlag) const;
	bool isMethodZero() const { return currentMethodsBitmask == 0; }
	bool eval1bool;
	int eval1cnt;
private:
	MethodManager();
	~MethodManager();
	static MethodManager* instance;
	int currentMethodsBitmask;
	int methodMask;
	RakNet::TimeMS eval3LogTime;
	

	int um_cnt;
	int am_cnt;
	int sumScore;
	RakNet::TimeMS reactionTime = 0;
	DataStructures::Heap<uint64_t, orderData, false> orderPq;
	DataStructures::Map<int, RakNet::TimeMS> umTimeMap;
	DataStructures::Map<int, RakNet::TimeMS> umReceptionTimes;

};

