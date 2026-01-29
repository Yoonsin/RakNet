#include "CInGame.h"
#include "NetLogManager.h"
#include "MethodManager.h"
#include "NetworkManager.h"
#include "GetTime.h"
#include <cstdio>
#include <cstdarg>
#include <string>
#include <ctime>

#if defined(_WIN32)
#include <windows.h>
#include <direct.h> // _mkdir
#define GetCurrentDir _getcwd
#else
#include <unistd.h>
#include <sys/stat.h> // mkdir
#define GetCurrentDir getcwd
#endif


using namespace RakNet;
using namespace irr;

NetLogManager* NetLogManager::instance = nullptr;
NetLogManager* NetLogManager::Instance() {
	if (instance == nullptr) instance = new NetLogManager();
	return instance;
}
void NetLogManager::DestroyInstance() {
	if (instance) {
		delete instance;
		instance = nullptr;
	}
}
NetLogManager::NetLogManager() {}
NetLogManager::~NetLogManager() {
}

void NetLogManager::Initialize(bool enableLog, int maxLogCount, const char* base) {

	isLoggingEnabled = enableLog; maxLogEntries = maxLogCount;
	// 버퍼 미리 할당 (메소드 0~4번 정도까지 커버)
	std::lock_guard<std::mutex> lock(logMutex);
	logBuffers.Clear(false, _FILE_AND_LINE_);
	for (int i = 0; i < 5; i++) {
		logBuffers.Push(DataStructures::List<RakNet::RakString>(), _FILE_AND_LINE_);
	}
	throughputLogs.Clear(false, _FILE_AND_LINE_);
	jitterLogs.Clear(false, _FILE_AND_LINE_);
	packetLossLogs.Clear(false, _FILE_AND_LINE_);
	serverStatLogs.Clear(false, _FILE_AND_LINE_);
	clientStatLogs.Clear();
	baseDir = base;
	isServerStatLogging = false; //at NetworkManager 
	isPerClientStatLogging = true;
	lastStatLogTime = 0;
}

void NetLogManager::LogRTT(int methodType, const RakNet::SystemAddress& sa, int rtt, int Sequence) {
	if (!isLoggingEnabled) return;

	// CSV 포맷: Timestamp, IP, RTT, Sequence
	RakNet::RakString logStr;
	logStr.Set("%u,%s,%d,%d", RakNet::GetTimeMS(), sa.ToString(true), rtt, Sequence);
	int methodIndex = MethodManager::ConvertMethodToIndex(methodType);

	std::lock_guard<std::mutex> lock(logMutex);
	// 인덱스 안전 검사
	if (methodIndex >= 0 && methodIndex < logBuffers.Size()) {
		// 최대 개수 넘으면 가장 오래된 것 삭제 (메모리 보호)
		if (logBuffers[methodIndex].Size() >= maxLogEntries) {
			logBuffers[methodIndex].RemoveAtIndex(0);
		}
		logBuffers[methodIndex].Push(logStr, _FILE_AND_LINE_);
	}
}

void NetLogManager::LogMessage(int methodType, const char* format, ...) {
	if (!isLoggingEnabled) return;

	char buffer[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	RakNet::RakString logStr(buffer);
	int methodIndex = MethodManager::ConvertMethodToIndex(methodType);

	std::lock_guard<std::mutex> lock(logMutex);
	if (methodIndex >= 0 && methodIndex < logBuffers.Size()) {
		if (logBuffers[methodIndex].Size() >= maxLogEntries) {
			logBuffers[methodIndex].RemoveAtIndex(0);
		}
		logBuffers[methodIndex].Push(logStr, _FILE_AND_LINE_);
	}
}

void NetLogManager::Shutdown() {
	if (NetworkManager::Instance()->IsServer()) {
		if (MethodManager::Instance()->IsMethodActive(METHOD_1) && MethodManager::Instance()->IsMethodLogActive(METHOD_1))NetLogManager::Instance()->SaveLogsToCSV(METHOD_1);
		if (MethodManager::Instance()->IsMethodActive(METHOD_3) && MethodManager::Instance()->IsMethodLogActive(METHOD_3))NetLogManager::Instance()->SaveLogsToCSV(METHOD_3);
	}
	if(MethodManager::Instance()->scenarioNum == SCENARIO_MOVE_BOT_FIXED) SaveNetworkStats();
}

void NetLogManager::LogThroughput(uint32_t bytesPerSecond) {
	if (!isLoggingEnabled) return;
	RakNet::RakString logStr;
	logStr.Set("%u,%u", RakNet::GetTimeMS(), bytesPerSecond);
	std::lock_guard<std::mutex> lock(logMutex);
	if (throughputLogs.Size() >= maxLogEntries) throughputLogs.RemoveAtIndex(0);
	throughputLogs.Push(logStr, _FILE_AND_LINE_);
}

void NetLogManager::LogJitter(int intervalMS) {
	if (!isLoggingEnabled) return;
	RakNet::RakString logStr;
	logStr.Set("%u,%d", RakNet::GetTimeMS(), intervalMS);
	std::lock_guard<std::mutex> lock(logMutex);
	if (jitterLogs.Size() >= maxLogEntries) jitterLogs.RemoveAtIndex(0);
	jitterLogs.Push(logStr, _FILE_AND_LINE_);
}

void NetLogManager::LogPacketLoss(int lostCount, int lostBytes, int lastSeq, int curSeq) {
	if (!isLoggingEnabled) return;
	RakNet::RakString logStr;
	logStr.Set("%u,%d,%d,%d,%d", RakNet::GetTimeMS(), lostCount, lostBytes, lastSeq, curSeq);
	std::lock_guard<std::mutex> lock(logMutex);
	if (packetLossLogs.Size() >= maxLogEntries) packetLossLogs.RemoveAtIndex(0);
	packetLossLogs.Push(logStr, _FILE_AND_LINE_);
}

RakNet::RakString NetLogManager::GetUniqueFilePath(const char* baseDir, const char* fileName) {
	RakNet::RakString fullPath;
	fullPath.Set("%s/%s", baseDir, fileName);

	FILE* fp = fopen(fullPath.C_String(), "r");
	if (fp == nullptr) return fullPath;
	fclose(fp);

	std::string sFileName = fileName;
	std::string namePart, extPart;
	size_t dotPos = sFileName.find_last_of('.');
	if (dotPos != std::string::npos) {
		namePart = sFileName.substr(0, dotPos);
		extPart = sFileName.substr(dotPos);
	}
	else {
		namePart = sFileName;
		extPart = "";
	}

	int count = 1;
	while (true) {
		fullPath.Set("%s/%s_%d%s", baseDir, namePart.c_str(), count, extPart.c_str());
		fp = fopen(fullPath.C_String(), "r");
		if (fp == nullptr) return fullPath;
		fclose(fp);
		count++;
	}
}

void NetLogManager::SaveNetworkStats() {
	std::lock_guard<std::mutex> lock(logMutex);
	
	if ( NetworkManager::Instance( )->IsServer( ) ) {
		if ( serverStatLogs.Size( ) > 0 ) {
			RakNet::RakString fileName = GetUniqueFilePath(baseDir, "serverStatLogs.csv");
			FILE * fp = fopen(fileName.C_String( ), "w");
			if ( fp ) {
				fprintf(fp, "Timestamp(ms),isLimited,BPSLimit,BytesPushed,BytesSent,BytesResent,ActualBytesSent,SendBuf(Imm),SendBuf(High),SendBuf(Med),SendBuf(Low),ResendBuf,packetLoss,MsgSent(Imm),MsgSent(High),MsgSent(Med),MsgSent(Low),MsgSent(Unrel),MsgSent(UnrelSeq),MsgSent(Rel),MsgSent(RelOrd),MsgSent(RelSeq),MsgSent(UnrelAck),MsgSent(RelAck),MsgSent(RelOrdAck)\n");
				for ( unsigned int i = 0; i < serverStatLogs.Size( ); i++ ) fprintf(fp, "%s\n", serverStatLogs[i].C_String( ));
				fclose(fp);
			}
		}

		if (isPerClientStatLogging) {
			for (unsigned int i = 0; i < clientStatLogs.Size(); ++i) {
				RakNet::SystemAddress sa = clientStatLogs.GetKeyAtIndex(i);
				DataStructures::List<RakNet::RakString>& logs = clientStatLogs[i];
				
				if (logs.Size() == 0) continue;

				char fileNameBuf[512];
				sprintf(fileNameBuf, "serverStatLogs_%s.csv", sa.ToString(true, '-')); // IP-Port format
				RakNet::RakString fileName = GetUniqueFilePath(baseDir, fileNameBuf);
				
				FILE* fp = fopen(fileName.C_String(), "w");
				if (fp) {
					fprintf(fp, "Timestamp(ms),isLimited,BPSLimit,BytesPushed,BytesSent,BytesResent,ActualBytesSent,SendBuf(Imm),SendBuf(High),SendBuf(Med),SendBuf(Low),ResendBuf,packetLoss,MsgSent(Imm),MsgSent(High),MsgSent(Med),MsgSent(Low),MsgSent(Unrel),MsgSent(UnrelSeq),MsgSent(Rel),MsgSent(RelOrd),MsgSent(RelSeq),MsgSent(UnrelAck),MsgSent(RelAck),MsgSent(RelOrdAck)\n");
					for (unsigned int j = 0; j < logs.Size(); j++) fprintf(fp, "%s\n", logs[j].C_String());
					fclose(fp);
				}
			}
		}
	}
	else {
		if ( throughputLogs.Size( ) > 0 ) {
			RakNet::RakString fileName = GetUniqueFilePath(baseDir, "Network_Throughput.csv");
			FILE * fp = fopen(fileName.C_String( ), "w");
			if ( fp ) {
				fprintf(fp, "Timestamp(ms),Throughput(B/s)\n");
				for ( unsigned int i = 0; i < throughputLogs.Size( ); i++ ) fprintf(fp, "%s\n", throughputLogs[i].C_String( ));
				fclose(fp);
			}
		}

		if ( jitterLogs.Size( ) > 0 ) {
			RakNet::RakString fileName = GetUniqueFilePath(baseDir, "Network_Jitter.csv");
			FILE * fp = fopen(fileName.C_String( ), "w");
			if ( fp ) {
				fprintf(fp, "Timestamp(ms),Jitter(Interval_ms)\n");
				for ( unsigned int i = 0; i < jitterLogs.Size( ); i++ ) fprintf(fp, "%s\n", jitterLogs[i].C_String( ));
				fclose(fp);
			}
		}

		if ( packetLossLogs.Size( ) > 0 ) {
			RakNet::RakString fileName = GetUniqueFilePath(baseDir, "Network_PacketLoss.csv");
			FILE * fp = fopen(fileName.C_String( ), "w");
			if ( fp ) {
				fprintf(fp, "Timestamp(ms),PacketLoss,Throughput,Jitter,MsgRecv(Unrel),MsgRecv(UnrelSeq),MsgRecv(Rel),MsgRecv(RelOrd),MsgRecv(RelSeq),MsgRecv(UnrelAck),MsgRecv(RelAck),MsgRecv(RelOrdAck),Jitter(Unrel),Jitter(UnrelSeq),Jitter(Rel),Jitter(RelOrd),Jitter(RelSeq),Jitter(UnrelAck),Jitter(RelAck),Jitter(RelOrdAck)\n");
				for ( unsigned int i = 0; i < packetLossLogs.Size( ); i++ ) fprintf(fp, "%s\n", packetLossLogs[i].C_String( ));
				fclose(fp);
			}
		}
	}
}

void NetLogManager::Update( ) {
	PrintServerStat( );
	//PrintPlayerStat( );
}

void NetLogManager::PrintServerStat( ) {
	if ( !isServerStatLogging ) return;
	RakNet::TimeMS curTime = RakNet::GetTimeMS();
	if ( curTime - lastStatLogTime < 1000 ) return;
	lastStatLogTime = curTime;

	RakNet::RakNetStatistics stats;
	RakPeerInterface* peer = NetworkManager::Instance( )->GetPeer( );
	if (peer == nullptr) return;

	if (peer->GetStatistics(UNASSIGNED_SYSTEM_ADDRESS, &stats) == nullptr) return;

	if ( NetworkManager::Instance( )->IsServer() ) {
		RakNet::RakString logStr;
		logStr.Set("%d,%d,%llu,", curTime, stats.isLimitedByCongestionControl, stats.BPSLimitByCongestionControl);
		for ( int i = 0; i <= 2; i++ ) logStr += RakString("%llu,", stats.valueOverLastSecond[i]);
		logStr += RakString("%llu,", stats.valueOverLastSecond[ACTUAL_BYTES_SENT]);
		for ( int i = 0; i < NUMBER_OF_PRIORITIES; i++ ) logStr += RakString("%.0f,", stats.bytesInSendBuffer[i]);
		logStr += RakString("%llu,",stats.bytesInResendBuffer);
		logStr += RakString("%.4f,",stats.packetlossLastSecond);

		// [Added] Priority & Reliability Stats
		for (int i = 0; i < NUMBER_OF_PRIORITIES; i++) logStr += RakString("%llu,", stats.messagesSentByPriority[i]);
		for (int i = 0; i < NUMBER_OF_RELIABILITIES; i++) logStr += RakString("%llu,", stats.messagesSentByReliability[i]);
		// Remove trailing comma
		logStr.Truncate(logStr.GetLength() - 1);
	
		std::lock_guard<std::mutex> lock(logMutex);
		if ( serverStatLogs.Size( ) >= maxLogEntries ) serverStatLogs.RemoveAtIndex(0);
		serverStatLogs.Push(logStr, _FILE_AND_LINE_);

		if (isPerClientStatLogging) {
			DataStructures::List<RakNet::SystemAddress> connections;
			DataStructures::List<RakNet::RakNetGUID> guids;
			peer->GetSystemList(connections, guids);

			for (unsigned int i = 0; i < connections.Size(); ++i) {
				RakNet::SystemAddress sa = connections[i];
				RakNet::RakNetStatistics clientStats;
				if (peer->GetStatistics(sa, &clientStats)) {
					RakNet::RakString clientLogStr;
					clientLogStr.Set("%d,%d,%llu,", curTime, clientStats.isLimitedByCongestionControl, clientStats.BPSLimitByCongestionControl);
					for (int k = 0; k <= 2; k++) clientLogStr += RakString("%llu,", clientStats.valueOverLastSecond[k]);
					clientLogStr += RakString("%llu,", clientStats.valueOverLastSecond[ACTUAL_BYTES_SENT]);
					for (int k = 0; k < NUMBER_OF_PRIORITIES; k++) clientLogStr += RakString("%.0f,", clientStats.bytesInSendBuffer[k]);
					clientLogStr += RakString("%llu,", clientStats.bytesInResendBuffer);
					clientLogStr += RakString("%.4f,", clientStats.packetlossLastSecond);

					// [Added] Priority & Reliability Stats
					for (int k = 0; k < NUMBER_OF_PRIORITIES; k++) clientLogStr += RakString("%llu,", clientStats.messagesSentByPriority[k]);
					for (int k = 0; k < NUMBER_OF_RELIABILITIES; k++) clientLogStr += RakString("%llu,", clientStats.messagesSentByReliability[k]);
					// Remove trailing comma
					clientLogStr.Truncate(clientLogStr.GetLength() - 1);

					if (!clientStatLogs.Has(sa)) {
						clientStatLogs.Set(sa, DataStructures::List<RakNet::RakString>());
					}
					
					DataStructures::List<RakNet::RakString>& logs = clientStatLogs.Get(sa);
					if (logs.Size() >= maxLogEntries) logs.RemoveAtIndex(0);
					logs.Push(clientLogStr, _FILE_AND_LINE_);
				}
			}
		}
	}
	else {
		// Client Logging: Throughput and Packet Loss from global stats
		LogThroughput((uint32_t)stats.valueOverLastSecond[ACTUAL_BYTES_RECEIVED]);
		
		RakNet::RakString logStr;
		// Timestamp, PacketLoss, Throughput, Jitter
		logStr.Set("%u,%.4f,%llu,%.2f", curTime, stats.packetlossLastSecond, stats.valueOverLastSecond[ACTUAL_BYTES_RECEIVED], stats.jitterLastSecond);
		
		// Append MsgRecv(Reliability)
		for(int k=0; k < NUMBER_OF_RELIABILITIES; k++) logStr += RakString(",%llu", stats.messagesReceivedByReliability[k]);

		// [Added] Append Jitter(Reliability)
		for(int k=0; k < NUMBER_OF_RELIABILITIES; k++) logStr += RakString(",%.2f", stats.jitterByReliability[k]);

		std::lock_guard<std::mutex> lock(logMutex);
		if ( packetLossLogs.Size( ) >= maxLogEntries ) packetLossLogs.RemoveAtIndex(0);
		packetLossLogs.Push(logStr, _FILE_AND_LINE_);
	}
}

void NetLogManager::PrintPlayerStat() {
	
	scene::ISceneManager* sm; 
	scene::ICameraSceneNode* camera;
	sm = CInGame::Instance()->GetDevice()->getSceneManager();
	if(sm != nullptr) camera = sm->getActiveCamera();
	if(camera == nullptr) return;

	PrintDebug("Player Pos : (%.2f, %.2f, %.2f)\n", camera->getPosition().X, camera->getPosition().Y, camera->getPosition().Z);
	PrintDebug("Player At : (%.2f, %.2f, %.2f)\n", camera->getTarget().X, camera->getTarget().Y, camera->getTarget().Z);
}

void NetLogManager::SaveLogsToCSV(int methodType) {
	int methodIndex = MethodManager::ConvertMethodToIndex(methodType);
	std::lock_guard<std::mutex> lock(logMutex);

	if (methodIndex < 0 || methodIndex >= logBuffers.Size()) return;
	if (logBuffers[methodIndex].Size() == 0) return;

	// 파일명 생성: baseDir/Method_X_Logs.csv
	RakNet::RakString baseName;
	baseName.Set("Method_%d_RTT.csv", methodIndex);
	RakNet::RakString fileName = GetUniqueFilePath(baseDir, baseName.C_String());

	FILE* fp = fopen(fileName.C_String(), "w");
	if (!fp) {
		PrintDebug("Failed to open file for logging: %s\n", fileName.C_String());
		return;
	}

	// CSV 헤더 작성
	fprintf(fp, "Timestamp(ms),IP_Address,RTT(ms),SequenceNum\n");

	// 데이터 작성
	for (unsigned int i = 0; i < logBuffers[methodIndex].Size(); i++) {
		fprintf(fp, "%s\n", logBuffers[methodIndex][i].C_String());
	}

	fclose(fp);
	PrintDebug("Saved %d logs to %s\n", logBuffers[methodIndex].Size(), fileName.C_String());

	// 저장 후 버퍼 비우기 (선택 사항)
	logBuffers[methodIndex].Clear(false, _FILE_AND_LINE_);
}

void NetLogManager::PrintDebug(const char* format, ...)
{
	char buf[512];
	va_list args;
	va_start(args, format);
	vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);
#if defined(_WIN32)
	// 윈도우: Visual Studio 디버그 출력창에 출력
	OutputDebugStringA(buf);
#else
	// 리눅스/기타 플랫폼: 그냥 stdout에 출력
	printf("%s", buf);
#endif
}
