#pragma once
#include "NetLogManager.h"
#include "NetworkManager.h" 
#include "GetTime.h"
#include <cstdio>
#include <ctime>
#include <chrono>
#include "StatisticsHistory.h"

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

void NetLogManager::Initialize(bool isLog, int logCnt) {

	isLogged = isLog; logCount = logCnt;
	statBufList.Preallocate(5, _FILE_AND_LINE_);
	for (int i = 0; i < 5; i++)
		statBufList.Push(DataStructures::List<RakNet::RakString>(), _FILE_AND_LINE_);
}
void NetLogManager::SaveStatisticsToCSV(const char* baseDir, int methodNum)
{
	if (statBufList.Size() <= 0) {
		return;
	}

	if (statBufList[methodNum].Size() == 0) {
		return;
	}

	if (NetworkManager::Instance()->GetTopology() == SERVER) {
#ifdef __linux__
		// 현재 시간
		time_t now = time(nullptr);
		struct tm* t = localtime(&now);

		// 타임스탬프 문자열 생성
		char timeStr[64];
		strftime(timeStr, sizeof(timeStr), "%m%d_%H%M", t);

		// 절대 디렉토리
		const char* outputDir = "/home/parts/stats";

		mkdir(outputDir, 0777);  // 이미 있으면 실패하지만 무시됨
		// 경로 + 파일명 조합
		char fullpath[512];
		snprintf(fullpath, sizeof(fullpath), "%s/kill_log_%s.csv", outputDir, timeStr);

		// 파일 열기
		FILE* f = fopen(fullpath, "w");
		if (!f) {
			perror("파일 열기 실패");
			return;
		}

		for (unsigned int i = 0; i < statBufList[methodNum].Size(); i++)
		{
			fprintf(f, "%s", statBufList[methodNum][i].C_String());
		}

		fclose(f);
		statBufList[methodNum].Clear(false, _FILE_AND_LINE_);

#endif // __linux__
	}
	else {
		// 클라이언트는 baseDir 사용
		// 현재 시간
		time_t now = time(nullptr);
		struct tm* t = localtime(&now);
		// 타임스탬프 문자열 생성
		char timeStr[64];
		strftime(timeStr, sizeof(timeStr), "%m%d_%H%M", t);
		// 경로 + 파일명 조합
		char fullpath[512];
		snprintf(fullpath, sizeof(fullpath), "%s/[Method %d]stats_client_%s.csv", baseDir, methodNum, timeStr);
		// 파일 열기
		FILE* f = fopen(fullpath, "w");
		if (!f) {
			perror("파일 열기 실패");
			return;
		}
		for (unsigned int i = 0; i < statBufList[methodNum].Size(); i++)
		{
			fprintf(f, "%s", statBufList[methodNum][i].C_String());
		}
		fclose(f);
		statBufList[methodNum].Clear(false, _FILE_AND_LINE_);
	}
}
void NetLogManager::PrintStatistics(bool isExportFile, int methodNum)
{
	if (statBufList.Size() <= 0)return;
	unsigned short connectionCount = NetworkManager::Instance()->GetPeer()->NumberOfConnections();
	RakNet::SystemAddress systems[256];
	NetworkManager::Instance()->GetPeer()->GetConnectionList(systems, &connectionCount);

	for (unsigned short i = 0; i < connectionCount; ++i)
	{
		RakNet::SystemAddress addr = systems[i];
		char ipStr[64];
		addr.ToString(false, ipStr);

		RakNetGUID guid = NetworkManager::Instance()->GetPeer()->GetGuidFromSystemAddress(addr);
		uint64_t objectId = guid.g;

		// 현재 시간 (밀리초)
		Time curTime = RakNet::GetTime();

		// [수정됨] Method 3: 상세 네트워크 통계 출력
		if (methodNum == 3 && NetworkManager::Instance()->GetStatisticsPlugin()) {

			// 통계 데이터를 담을 변수 초기화
			double egressThroughput = 0.0; // RN_USER_MESSAGE_BYTES_SENT
			double ingressThroughput = 0.0; // RN_USER_MESSAGE_BYTES_RECEIVED_PROCESSED
			double rtt = 0.0;               // RN_lastPing
			double packetLoss = 0.0;        // RN_packetlossLastSecond
			double resendBytes = 0.0;        // RN_USER_MESSAGE_BYTES_RESENT


			RakNet::StatisticsHistory::TimeAndValueQueue* queue = 0;

			// 1. Egress Throughput (Server -> Client)
			if (NetworkManager::Instance()->GetStatisticsPlugin()->statistics.GetHistoryForKey(objectId, "RN_USER_MESSAGE_BYTES_SENT", &queue, curTime) == StatisticsHistory::SH_OK) {
				if (queue->values.Size() > 0) egressThroughput = queue->values.PeekTail().val;
			}

			// 2. Ingress Throughput (Client -> Server)
			if (NetworkManager::Instance()->GetStatisticsPlugin()->statistics.GetHistoryForKey(objectId, "RN_USER_MESSAGE_BYTES_RECEIVED_PROCESSED", &queue, curTime) == StatisticsHistory::SH_OK) {
				if (queue->values.Size() > 0) ingressThroughput = queue->values.PeekTail().val;
			}

			// 3. RTT (Round Trip Time)
			if (NetworkManager::Instance()->GetStatisticsPlugin()->statistics.GetHistoryForKey(objectId, "RN_lastPing", &queue, curTime) == StatisticsHistory::SH_OK) {
				if (queue->values.Size() > 0) rtt = queue->values.PeekTail().val;
			}

			// 4. Packet Loss (Rate)
			if (NetworkManager::Instance()->GetStatisticsPlugin()->statistics.GetHistoryForKey(objectId, "RN_packetlossLastSecond", &queue, curTime) == StatisticsHistory::SH_OK) {
				if (queue->values.Size() > 0) packetLoss = queue->values.PeekTail().val;
			}

			// 5.
			if (NetworkManager::Instance()->GetStatisticsPlugin()->statistics.GetHistoryForKey(objectId, "RN_USER_MESSAGE_BYTES_RESENT", &queue, curTime) == StatisticsHistory::SH_OK) {
				if (queue->values.Size() > 0) resendBytes = queue->values.PeekTail().val;

				// [출력 포맷] 시간, IP, Egress(Byte/s), Ingress(Byte/s), RTT(ms), Loss(%)
				// 필요한 경우 B/s를 *8 하여 bps로 변환하거나 KB/s로 나눌 수 있습니다.

				unsigned int ms = curTime % 1000;
				unsigned int totalSeconds = curTime / 1000;
				unsigned int seconds = totalSeconds % 60;
				unsigned int minutes = (totalSeconds / 60) % 60;
				unsigned int hours = (totalSeconds / 3600) % 24;

				char buffer[512];
				snprintf(buffer, sizeof(buffer),
					"%02u:%02u:%02u:%03u,%s,EgressThroughput:%.2f,IngressThroughput:%.2f,RTT:%.0f,Loss:%.2f,ResendIngressThroughput:%.2f%%\n",
					hours, minutes, seconds, ms,
					ipStr,
					egressThroughput,
					ingressThroughput,
					rtt,
					packetLoss * 100.0f,
					resendBytes); // 0.0~1.0 이므로 백분율 변환

				statBufList[methodNum].Push(RakNet::RakString(buffer), _FILE_AND_LINE_);
			}
			else
			{
				// 기존 로직 (Method 3이 아닐 때)
				Time curTime = RakNet::GetTime();
				unsigned int ms = curTime % 1000;
				unsigned int totalSeconds = curTime / 1000;
				unsigned int seconds = totalSeconds % 60;
				unsigned int minutes = (totalSeconds / 60) % 60;
				unsigned int hours = (totalSeconds / 3600) % 24;

				char buffer[160];
				snprintf(buffer, sizeof(buffer), "%02u:%02u:%02u:%03u,%s", hours, minutes, seconds, ms, ipStr);
				statBufList[methodNum].Push(RakNet::RakString(buffer), _FILE_AND_LINE_);
			}
		}
	}
}
void NetLogManager::PrintStatistics(char* ipStr, PrintStatics id, int num)
{
	//클라 입력 - 서버 처리 - 클라 렌더링 까지 걸린 시간 출력
	RakNet::TimeUS curTime = RakNet::GetTimeUS();  // us 단위
	unsigned int us = curTime % 1000;
	unsigned int ms = (curTime / 1000) % 1000;
	unsigned int totalSeconds = curTime / 1000000;
	unsigned int seconds = totalSeconds % 60;
	unsigned int minutes = (totalSeconds / 60) % 60;
	unsigned int hours = (totalSeconds / 3600) % 24;


	char buffer[200];
	char type[30];

	switch (id)
	{
	case ID_CLIENT_POLLING_END:
		snprintf(type, sizeof(type), "CLIENT_POLLING_END");
		break;
	case ID_CLIENT_NETWORK_SEND:
		snprintf(type, sizeof(type), "CLIENT_NETWORK_SEND");
		break;
	case ID_SERVER_NETWORK_RECEIVE:
		snprintf(type, sizeof(type), "SERVER_NETWORK_RECEIVE");
		break;
	case ID_SERVER_NETWORK_SEND:
		snprintf(type, sizeof(type), "SERVER_NETWORK_SEND");
		break;
	case ID_CLIENT_NETWORK_RECEIVE:
		snprintf(type, sizeof(type), "CLIENT_NETWORK_RECEIVE");
		break;
	case ID_CLIENT_RENDERING_START:
		snprintf(type, sizeof(type), "CLIENT_RENDERING_START");
		break;
	default:
		snprintf(type, sizeof(type), "Unknown");
		break;
	}

	if (ipStr != nullptr)
		snprintf(buffer, sizeof(buffer), "%s:%s:%d/%02u:%02u:%02u:%03u.%03u\n", ipStr, type, num, hours, minutes, seconds, ms, us);
	else
		snprintf(buffer, sizeof(buffer), "%s:%d/%02u:%02u:%02u:%03u.%03u\n", type, num, hours, minutes, seconds, ms, us);

	statBufList[0].Push(RakNet::RakString(buffer), _FILE_AND_LINE_); //Log
}

long long GetCurrentTimeMS()
{
	using namespace std::chrono;
	auto now = system_clock::now();
	auto duration = duration_cast<milliseconds>(now.time_since_epoch());
	return duration.count();
}
void DebugPrintf(const char* format, ...)
{
	char buf[512];

	va_list args;
	va_start(args, format);
	vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

#if defined(_WIN32)
	// 윈도우: Visual Studio 디버그 출력창에 출력
	OutputDebugStringA(buf);
#elif defined(__ANDROID__)
	// 안드로이드: Logcat 출력
	__android_log_print(ANDROID_LOG_DEBUG, "RakNetDemo", "%s", buf);
#else
	// 리눅스/기타 플랫폼: 그냥 stdout에 출력
	printf("%s", buf);
#endif

}


#ifdef __linux__
// 한 줄만 갱신 (out: '\r' + "문자열" + 필요 시 공백패딩), syscall 1회
static inline void PrintHoldPosOneLine(float x, float y, float z)
{
	static int last_len = 0;           // 이전에 찍은 문자열 길이(잔상 지우기용)
	char msg[128];

	int msg_len = snprintf(msg, sizeof(msg),
		"Hold Pos : %.2f, %.2f, %.2f", x, y, z);
	if (msg_len < 0) return;
	if (msg_len > (int)sizeof(msg))    // (이상 방지: 잘릴 일은 사실상 없음)
		msg_len = (int)sizeof(msg);

	char out[256];
	int n = 0;

	out[n++] = '\r';                   // 커서를 현재 줄 맨 앞으로
	memcpy(out + n, msg, msg_len);     // 새 내용 복사
	n += msg_len;

	// 이전 줄이 더 길었으면 남은 꼬리 지우기(공백으로 덮어쓰기)
	int pad = last_len - msg_len;
	if (pad > 0) {
		memset(out + n, ' ', pad);
		n += pad;
	}

	// 최종: write() 1회
	write(STDOUT_FILENO, out, n);

	last_len = msg_len;
}

// (선택) 출력 마무리로 줄을 고정하고 싶을 때 호출
static inline void PrintOneLineNewline(void) {
	write(STDOUT_FILENO, "\n", 1);
}
#else
// ── Windows / 기타 플랫폼: no-op ──────────────────────────────────────
static inline void PrintHoldPosOneLine(float x, float y, float z) {
	(void)x; (void)y; (void)z; // 경고 억제
	// no-op
}
static inline void PrintOneLineNewline(void) {
	// no-op
}

#endif // __linux__