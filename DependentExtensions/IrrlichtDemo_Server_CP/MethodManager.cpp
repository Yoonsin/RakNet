#include "MethodManager.h"

MethodManager* MethodManager::instance = nullptr;

MethodManager* MethodManager::Instance() {
    if (instance == nullptr) instance = new MethodManager();
    return instance;
}

void MethodManager::DestroyInstance() {
    if (instance) {
        delete instance;
        instance = nullptr;
    }
}

MethodManager::MethodManager() {
    currentMethodsBitmask = 0;
    eval1bool = false;
    eval1cnt = 0;
    eval3LogTime = 0;
    
}

MethodManager::~MethodManager() {
    RakNet::TimeMS currentTime = RakNet::GetTimeMS();
    if (currentTime - eval3LogTime >= 1000) // 1000ms = 1초
    {
        NetLogManager::Instance()->PrintStatistics(false, 3); // Method 3 통계 출력
        eval3LogTime = currentTime;
    }
}

void MethodManager::Initialize(int methodMask) {
	methodMask = methodMask;
    if (IsMethodActive(1)) BOT_MOVE_TIME = 1000;
    //evalMask(0), eval1bool(false), um_cnt(0), am_cnt(0),  eval1cnt(0), sumScore(0),
}

void MethodManager::Activate() {
}
void MethodManager::UpdateMethod(int methodFlag) {
	
    if(methodFlag == 1) if (eval1bool) eval1cnt++;
    if (methodFlag == 3) {
        RakNet::TimeMS currentTime = RakNet::GetTimeMS();
        if (currentTime - eval3LogTime >= 1000) // 1000ms = 1초
        {
        	NetLogManager::Instance()->PrintStatistics(false, 3); 
        	eval3LogTime = currentTime;
        }
	}
}
