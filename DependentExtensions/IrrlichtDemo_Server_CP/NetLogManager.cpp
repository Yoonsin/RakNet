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
	baseDir = base;
}

void NetLogManager::LogRTT(int methodType, const RakNet::SystemAddress& sa, int rtt, int Sequence) {
	if (!isLoggingEnabled) return;

	// CSV 포맷: Timestamp, IP, RTT, Sequence
	RakNet::RakString logStr;
	logStr.Set("%u,%s,%d,%d", RakNet::GetTimeMS(), sa.ToString(false), rtt, Sequence);
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
}

void NetLogManager::SaveLogsToCSV(int methodType) {
	int methodIndex = MethodManager::ConvertMethodToIndex(methodType);
	std::lock_guard<std::mutex> lock(logMutex);

	if (methodIndex < 0 || methodIndex >= logBuffers.Size()) return;
	if (logBuffers[methodIndex].Size() == 0) return;

	// 파일명 생성: baseDir/Method_X_Logs.csv
	RakNet::RakString fileName;
	fileName.Set("%s/Method_%d_RTT.csv", baseDir, methodIndex);

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
