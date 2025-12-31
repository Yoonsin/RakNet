#pragma once
#include "RakNetTypes.h"
#include "DS_List.h"
#include "RakString.h"

enum PrintStatics {
    ID_CLIENT_POLLING_END = 0,
    ID_CLIENT_NETWORK_SEND = 1,
    ID_SERVER_NETWORK_RECEIVE = 2,
    ID_SERVER_NETWORK_SEND = 3,
    ID_CLIENT_NETWORK_RECEIVE = 4,
    ID_CLIENT_RENDERING_START = 5,
};

class NetLogManager
{
public:
    static NetLogManager* Instance();
    static void DestroyInstance();

    // 초기화 (버퍼 할당 등)
    void Initialize(bool isLog, int logCnt);
    void Activate();
    void SaveStatisticsToCSV(const char* baseDir, int methodNum);
    void PrintStatistics(bool isExportFile, int methodNum);
    void PrintStatistics(char* ipStr, PrintStatics id, int num);

    // 로그 버퍼에 접근하기 위한 Helper
    void PushLog(int methodNum, const RakNet::RakString& log);

private:
    NetLogManager();
    ~NetLogManager();
    static NetLogManager* instance;
    DataStructures::List<DataStructures::List<RakNet::RakString>> statBufList;

    bool isLogged;
    bool isServer;
    int logCount;
};