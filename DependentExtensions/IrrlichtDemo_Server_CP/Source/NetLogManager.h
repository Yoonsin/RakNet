#pragma once
#include "RakNetTypes.h"
#include "DS_List.h"
#include "DS_Map.h"
#include "RakString.h"
#include <mutex>

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

    // 초기화 (로그 활성화 여부, 최대 저장 개수 설정)
    void Initialize(bool enableLog, int maxLogCount, const char* base);

    // [핵심] RTT 로그 기록 함수 (Method 번호, 대상 주소, RTT 값)
    void LogRTT(int methodType, const RakNet::SystemAddress& sa, int rtt, int Sequence = -1 );

    // 일반 메시지 로그 기록 (printf 처럼 사용)
    void LogMessage(int methodType, const char* format, ...);

    // 쌓인 로그를 CSV 파일로 저장
    void SaveLogsToCSV(int methodIndex);

    // 디버그 창 즉시 출력 (화면 표시용)
    void PrintDebug(const char* format, ...);

	void Shutdown();

	bool IsLogging() const { return isLoggingEnabled; }
	void SetLogging(bool enable) { isLoggingEnabled = enable; }

    void PrintServerStat( );
    
    void PrintPlayerStat();

    void Update( );

    void LogThroughput(uint32_t bytesPerSecond);
    void LogJitter(int intervalMS);
    void LogPacketLoss(int lostCount, int lostBytes, int lastSeq, int curSeq);
    void SaveNetworkStats();
    RakNet::RakString GetUniqueFilePath(const char* baseDir, const char* fileName);

    std::atomic<bool> isServerStatLogging;
    std::atomic<bool> isPerClientStatLogging; // 개별 클라이언트 통계 활성화 여부

private:
    NetLogManager();
    ~NetLogManager();
    
    static NetLogManager* instance;

    bool isLoggingEnabled;
    int maxLogEntries;
    const char* baseDir;

    // 로그 버퍼: [MethodIndex][LogStringList]
    // 예: logBuffers[1] 은 Method 1의 로그 리스트
    DataStructures::List<DataStructures::List<RakNet::RakString>> logBuffers;


    DataStructures::List<RakNet::RakString> serverStatLogs;
    DataStructures::Map<RakNet::SystemAddress, DataStructures::List<RakNet::RakString>> clientStatLogs; // 클라이언트별 통계 로그

    DataStructures::List<RakNet::RakString> throughputLogs;
    DataStructures::List<RakNet::RakString> jitterLogs;
    DataStructures::List<RakNet::RakString> packetLossLogs;

    // 동시 접근 방지용 뮤텍스
    std::mutex logMutex;

    RakNet::TimeMS lastStatLogTime;
};