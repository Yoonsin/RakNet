/*
 *  Copyright (c) 2014, Oculus VR, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant 
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "RakNetStuff.h"

#include "NetworkIDManager.h"
#include "CDemo.h"
#include "RakNetTime.h"
#include "GetTime.h"
#include "SocketLayer.h"
#include "RakNetStatistics.h"
#include "StatisticsHistory.h"
#include "PacketLogger.h"
#include <stdio.h>
#include <time.h>
#include <chrono>

#include <irrlicht.h>
#ifdef __linux__
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>   // memcpy, memset
#endif

#ifdef __ANDROID__
#include <android/log.h>
#endif // __ANDROID__


using namespace RakNet;
using namespace std;

RakPeerInterface *rakPeer;
NetworkIDManager *networkIDManager;
ReplicaManager3Irrlicht *replicaManager3;
NatPunchthroughClient *natPunchthroughClient;
CloudClient *cloudClient;
RakNet::FullyConnectedMesh2 *fullyConnectedMesh2;
PlayerReplica *playerReplica;
PlayerBotReplica* playerBotReplica;

Topology topology;
PacketLogger* loggerPlugin;
StatisticsHistoryPlugin* statisticsPlugin; // Used to track network statistics
CollisionBoxQueueSceneNode* collisionBoxQueue;
//int ping = -65555;

class DebugBoxSceneNode : public scene::ISceneNode 
{
public:
	DebugBoxSceneNode(scene::ISceneNode* parent, scene::ISceneManager* mgr, s32 id = -1);
	virtual const core::aabbox3d<f32>& getBoundingBox() const;
	virtual void OnRegisterSceneNode();
	virtual void render();
	void SetSelector(irr::scene::ITriangleSelector* selector);
	void EnableDrawTriangles(bool enable);

	CDemo *demo;
private:
	irr::scene::ITriangleSelector* triangleSelector = nullptr;
	bool drawTriangles = false;
};
DebugBoxSceneNode::DebugBoxSceneNode(scene::ISceneNode* parent, scene::ISceneManager* mgr, s32 id) : scene::ISceneNode(parent, mgr, id)
{
#ifdef _DEBUG
	setDebugName("DebugBoxSceneNode");
#endif
	setAutomaticCulling(scene::EAC_OFF);
} 
const core::aabbox3d<f32>& DebugBoxSceneNode::getBoundingBox() const
{
	return demo->GetSyndeyBoundingBox();
}
void DebugBoxSceneNode::OnRegisterSceneNode()
{
	if (IsVisible)
		demo->GetSceneManager()->registerNodeForRendering(this, scene::ESNRP_SOLID);
}
void DebugBoxSceneNode::SetSelector(scene::ITriangleSelector* selector) {
	triangleSelector = selector;
}
void DebugBoxSceneNode::EnableDrawTriangles(bool enable) {
	drawTriangles = enable;
}
void DebugBoxSceneNode::render()
{
	video::IVideoDriver* driver = SceneManager->getVideoDriver();

	if (DebugDataVisible)
	{ 
	  video::SMaterial m;
	  m.Lighting = false;
	  driver->setMaterial(m);
	  driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);
	  driver->draw3DBox(demo->GetSyndeyBoundingBox(), video::SColor(255, 0, 255, 255));
    }

	// TriangleSelector 기반 triangle 출력
	if (drawTriangles && triangleSelector != nullptr) {
		core::matrix4 worldMat;
		worldMat.makeIdentity();
		DrawBoxTriangles(triangleSelector, worldMat, driver);
	}
}
void DrawBoxTriangles(irr::scene::ITriangleSelector* selector, const irr::core::matrix4& transform, irr::video::IVideoDriver* driver)
{
	const int maxTriangles = 512;
	irr::core::triangle3df tris[maxTriangles];
	s32 outCount = 0;

	selector->getTriangles(tris, maxTriangles, outCount, &transform);  // 월드 변환 적용!

	for (int i = 0; i < outCount; ++i) {
		driver->draw3DLine(tris[i].pointA, tris[i].pointB, irr::video::SColor(255, 255, 0, 0));
		driver->draw3DLine(tris[i].pointB, tris[i].pointC, irr::video::SColor(255, 255, 0, 0));
		driver->draw3DLine(tris[i].pointC, tris[i].pointA, irr::video::SColor(255, 255, 0, 0));
	}
}

class CollisionBoxQueueSceneNode : public scene::ISceneNode
{
public:
	CollisionBoxQueueSceneNode(scene::ISceneNode* parent, scene::ISceneManager* mgr, s32 id = -1) : scene::ISceneNode(parent, mgr, id) {
#ifdef _DEBUG
		setDebugName("CollisionBoxQueueSceneNode");
#endif
		setAutomaticCulling(scene::EAC_OFF);
	}
	virtual void OnRegisterSceneNode() { if (IsVisible) demo->GetSceneManager()->registerNodeForRendering(this, scene::ESNRP_SOLID); }
	const core::aabbox3d<f32>& getBoundingBox() const
	{
		return demo->GetSyndeyBoundingBox();
	}
	virtual void render() {
		video::IVideoDriver* driver = SceneManager->getVideoDriver();
		video::SMaterial m;
		m.Lighting = true;
		driver->setMaterial(m);
		// 기존 transform을 유지하고, 월드 좌표계로 설정
		driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);

		RakNet::TimeMS now = RakNet::GetTimeMS();
		int idx = 0;
		while (idx < q.Size() ) {
			CollDebugState& item = q[idx];
			for (int i = 0; i < item.outCount; ++i) {
				driver->draw3DLine(item.tris[i].pointA, item.tris[i].pointB, irr::video::SColor(255, 0, 0, 255));
				driver->draw3DLine(item.tris[i].pointB, item.tris[i].pointC, irr::video::SColor(255, 0, 0, 255));
				driver->draw3DLine(item.tris[i].pointC, item.tris[i].pointA, irr::video::SColor(255, 0, 0, 255));
			}
			
			if (item.line != irr::core::line3d<irr::f32>()) driver->draw3DLine(item.line.start, item.line.end, irr::video::SColor(255, 0, 255, 0));

			if (now > item.timeStamp + item.drawTimeOut) q.RemoveAtIndex(idx); // 시간 초과된 항목 제거
			else ++idx;
		}
	}
	CDemo* demo;
	DataStructures::Queue <CollDebugState> q;
private:
};

//Collision Check 용
scene::ITriangleSelector* CreateSelectorFromTransformedBox(
	const core::aabbox3df& localBox,
	const core::matrix4& worldTransform,
	scene::ISceneManager* smgr, const RakNet::RakNetGUID& guid)
{
	// 로컬 박스의 꼭지점 8개 정의
	core::vector3df corners[8];
	localBox.getEdges(corners);

	core::vector3df transformedCorner;
	worldTransform.transformVect(transformedCorner, corners[0]);
	core::aabbox3df worldBox;
	worldBox.reset(transformedCorner);

	for (int i = 1; i < 8; ++i) {
		core::vector3df transformed;
		worldTransform.transformVect(transformed, corners[i]);
		worldBox.addInternalPoint(transformed);
	}

	// 임시 노드 (bounding box 전달용)
	class TempBoxNode : public scene::ISceneNode {
	public:
		core::aabbox3df box;
		TempBoxNode(const core::aabbox3df& b, scene::ISceneNode* parent, scene::ISceneManager* mgr)
			: scene::ISceneNode(parent, mgr), box(b) {
			setAutomaticCulling(scene::EAC_OFF);
		}
		virtual const core::aabbox3df& getBoundingBox() const override { return box; }
		virtual void render() override {}
		virtual void OnRegisterSceneNode() override {
			if (IsVisible)
				SceneManager->registerNodeForRendering(this);
		}
	};

	TempBoxNode* dummy = new TempBoxNode(worldBox, smgr->getRootSceneNode(), smgr);
	s32 id = static_cast<s32>(guid.g);
	dummy->setID(id); // RakNetGUID를 ID로 설정
	
	scene::ITriangleSelector* selector = smgr->createTriangleSelectorFromBoundingBox(dummy);
	dummy->remove(); // 노드 제거 (참조 카운트 감소)

	return selector; // drop()은 사용자가 책임
}


DataStructures::List<PlayerReplica*> PlayerReplica::playerList;
const int HISTORY_DURATION_MS = 1000;
static const float INGOING_TIME_MS = 0.0f;

void InstantiateRakNetClasses(bool isServer, bool isLogged, bool isBot, CDemo* demo)
{
	if (isServer) topology = SERVER;
	else topology = CLIENT;

	static const int MAX_PLAYERS=32;
	static const unsigned short TCP_PORT=0;
	static const RakNet::TimeMS UDP_SLEEP_TIMER=30;

	// Basis of all UDP communications
	rakPeer=RakNet::RakPeerInterface::GetInstance();
	rakPeer->SetPacketReturnDelay(INGOING_TIME_MS);
	// Using fixed port so we can use AdvertiseSystem and connect on the LAN if the server is not available.
	RakNet::SocketDescriptor sd((topology==SERVER)? SERVER_PORT : 1234, 0);
	sd.socketFamily = AF_INET; // Only IPV4 supports broadcast on 255.255.255.255
	
	if (topology == CLIENT) {
		while (IRNS2_Berkley::IsPortInUse(sd.port, sd.hostAddress, sd.socketFamily, SOCK_DGRAM) == true)
			sd.port++;
	}

	// +1 is for the connection to the NAT punchthrough server
	RakNet::StartupResult sr;
	if (topology == SERVER) {
		sr = rakPeer->Startup(MAX_PLAYERS + 1, &sd, 1);
		rakPeer->SetMaximumIncomingConnections(MAX_PLAYERS);
		if (demo->evalMask & METHOD_3)
			//rakPeer->ApplyNetworkSimulator(0.1f, 100, 50);
	}
	else sr = rakPeer->Startup(1, &sd, 1);
	rakPeer->SetOccasionalPing(true);

	RakAssert(sr==RakNet::RAKNET_STARTED);
		
	// ReplicaManager3 replies on NetworkIDManager. It assigns numbers to objects so they can be looked up over the network
	// It's a class in case you wanted to have multiple worlds, then you could have multiple instances of NetworkIDManager
	networkIDManager=new NetworkIDManager;
	
	// Automatically sends around new / deleted / changed game objects
	replicaManager3=new ReplicaManager3Irrlicht;
	//replicaManager3->SetIsLog(isLogged);
	
	replicaManager3->SetNetworkIDManager(networkIDManager);
	rakPeer->AttachPlugin(replicaManager3);
	
	// Automatically destroy connections, but don't create them so we have more control over when a system is considered ready to play
	replicaManager3->SetAutoManageConnections(false,true);
	replicaManager3->SetAutoSerializeInterval(30);
	replicaManager3->demo = demo;

	// Create and register the network object that represents the player
	// Hook RakNet stuff into this class
	if (isBot) {
		playerBotReplica = new PlayerBotReplica;
		playerBotReplica->demo = demo;
		playerBotReplica->CreateBotModel();
		playerBotReplica->gamePlatform = demo->gamePlatform;
		playerReplica = playerBotReplica;
	}
	else {
		playerReplica = new PlayerReplica;
		playerReplica->demo = demo;
		playerReplica->gamePlatform = demo->gamePlatform;
	}

	//Draw debug 
	scene::ISceneManager* smgr = demo->GetSceneManager();
	collisionBoxQueue = new CollisionBoxQueueSceneNode(smgr->getRootSceneNode(), smgr);
	collisionBoxQueue->demo = demo;

	if (topology == CLIENT) {
#if __ANDROID__
		//ConnectionAttemptResult car = rakPeer->Connect("10.0.2.2", SERVER_PORT, 0, 0); // 안드로이드 에뮬레이터의 "127.0.0.1" 주소
		ConnectionAttemptResult car = rakPeer->Connect(SERVER_IP, SERVER_PORT, 0, 0); //랜
		//ConnectionAttemptResult car = rakPeer->Connect("192.168.0.17", SERVER_PORT, 0, 0); //랜
#else
		//ConnectionAttemptResult car = rakPeer->Connect(SERVER_IP_LOCAL, SERVER_PORT, 0, 0); //로컬
		ConnectionAttemptResult car = rakPeer->Connect(SERVER_IP, SERVER_PORT, 0, 0); //랜
		//ConnectionAttemptResult car = rakPeer->Connect("192.168.0.17", SERVER_PORT, 0, 0); //랜
		
#endif // __ANDROID__
		//RakAssert(car == CONNECTION_ATTEMPT_STARTED);

		//loggerPlugin = PacketLogger::GetInstance();
		//rakPeer->AttachPlugin(loggerPlugin);
	}
	else if (topology == SERVER) {
		/*if(isBot) replicaManager3->Reference(playerBotReplica);
		else replicaManager3->Reference(playerReplica);*/

		if (demo->evalMask & METHOD_3) {
			statisticsPlugin = StatisticsHistoryPlugin::GetInstance();
			statisticsPlugin->SetTrackConnections(true, 0, true);
			rakPeer->AttachPlugin(statisticsPlugin);
		}
		
		//loggerPlugin = PacketLogger::GetInstance();
		//rakPeer->AttachPlugin(loggerPlugin);
	}
}
void DeinitializeRakNetClasses(bool isLogged, bool isBot, const char* baseDir, int evalMask)
{
	DataStructures::List<Replica3*> replicaListOut;
	replicaManager3->GetReplicasCreatedByMe(replicaListOut);
	replicaManager3->BroadcastDestructionList(replicaListOut, RakNet::UNASSIGNED_SYSTEM_ADDRESS);
	
	if (isLogged) {
		if (topology == SERVER) {
			SaveStatisticsToCSV(baseDir, 0);
			
			if (evalMask & METHOD_3) {
				//Egress, Ingress
				SaveStatisticsToCSV(baseDir, 3);
			}
		}
		else if (topology == CLIENT ){
			if (evalMask & METHOD_1)
				SaveStatisticsToCSV(baseDir,1);	
		}
	}

	rakPeer->SetOccasionalPing(false);

	// Shutdown so the server knows we stopped
	rakPeer->Shutdown(100,0);

	RakNet::RakPeerInterface::DestroyInstance(rakPeer);
	delete networkIDManager;
	delete replicaManager3;
	delete natPunchthroughClient;
	delete cloudClient;
	delete fullyConnectedMesh2;
	// ReplicaManager3 deletes all referenced objects, including this one
	
	if (evalMask & METHOD_3) {
		if (topology == SERVER) {
			RakNet::StatisticsHistoryPlugin::DestroyInstance(statisticsPlugin);
			//delete statisticsPlugin;
		}
	}
		
	if (isBot) {
		playerBotReplica->PreDestruction(0);
		delete playerBotReplica;
		//delete loggerPlugin;
	}
	else {
		playerReplica->PreDestruction(0);
		delete playerReplica;
		//delete loggerPlugin;
	}

	PrintOneLineNewline();
}

void PrintStatistics(bool isExportFile, int methodNum)
{
	unsigned short connectionCount = rakPeer->NumberOfConnections();
	RakNet::SystemAddress systems[256];
	rakPeer->GetConnectionList(systems, &connectionCount);

	for (unsigned short i = 0; i < connectionCount; ++i)
	{
		RakNet::SystemAddress addr = systems[i];
		char ipStr[64];
		addr.ToString(false, ipStr);

		RakNetGUID guid = rakPeer->GetGuidFromSystemAddress(addr);
		uint64_t objectId = guid.g;

		// 현재 시간 (밀리초)
		Time curTime = RakNet::GetTime();

		// [수정됨] Method 3: 상세 네트워크 통계 출력
		if (methodNum == 3 && statisticsPlugin) {

			// 통계 데이터를 담을 변수 초기화
			double egressThroughput = 0.0; // RN_USER_MESSAGE_BYTES_SENT
			double ingressThroughput = 0.0; // RN_USER_MESSAGE_BYTES_RECEIVED_PROCESSED
			double rtt = 0.0;               // RN_lastPing
			double packetLoss = 0.0;        // RN_packetlossLastSecond
			double resendBytes = 0.0;        // RN_USER_MESSAGE_BYTES_RESENT

			StatisticsHistory::TimeAndValueQueue* queue = 0;

			// 1. Egress Throughput (Server -> Client)
			if (statisticsPlugin->statistics.GetHistoryForKey(objectId, "RN_USER_MESSAGE_BYTES_SENT", &queue, curTime) == StatisticsHistory::SH_OK) {
				if (queue->values.Size() > 0) egressThroughput = queue->values.PeekTail().val;
			}

			// 2. Ingress Throughput (Client -> Server)
			if (statisticsPlugin->statistics.GetHistoryForKey(objectId, "RN_USER_MESSAGE_BYTES_RECEIVED_PROCESSED", &queue, curTime) == StatisticsHistory::SH_OK) {
				if (queue->values.Size() > 0) ingressThroughput = queue->values.PeekTail().val;
			}

			// 3. RTT (Round Trip Time)
			if (statisticsPlugin->statistics.GetHistoryForKey(objectId, "RN_lastPing", &queue, curTime) == StatisticsHistory::SH_OK) {
				if (queue->values.Size() > 0) rtt = queue->values.PeekTail().val;
			}

			// 4. Packet Loss (Rate)
			if (statisticsPlugin->statistics.GetHistoryForKey(objectId, "RN_packetlossLastSecond", &queue, curTime) == StatisticsHistory::SH_OK) {
				if (queue->values.Size() > 0) packetLoss = queue->values.PeekTail().val;
			}

			// 5.
			if (statisticsPlugin->statistics.GetHistoryForKey(objectId, "RN_USER_MESSAGE_BYTES_RESENT", &queue, curTime) == StatisticsHistory::SH_OK) {
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

void PrintStatistics(char* ipStr, PrintStatics id, int num)
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

void SaveStatisticsToCSV(const char* baseDir, int methodNum)
{
	if (statBufList[methodNum].Size() == 0) {
		return;
	}

	if (topology == SERVER) {
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

long long GetCurrentTimeMS()
{
	using namespace std::chrono;
	auto now = system_clock::now();
	auto duration = duration_cast<milliseconds>(now.time_since_epoch());
	return duration.count();
}

void DrawDebugFrame(const core::aabbox3df& boundingBox, core::vector3df position, float rotationAroundYAxis, scene::ISceneManager* sm, RakNet::RakNetGUID g, RakNet::TimeMS drawTimeOut)
{
	if (collisionBoxQueue == nullptr) return;
	core::matrix4 transform;
	transform.setTranslation(position);

	core::matrix4 rotation;
	rotation.setRotationDegrees(core::vector3df(0, rotationAroundYAxis, 0));

	core::matrix4 scale;
	scale.setScale(core::vector3df(1, 1, 1));

	transform *= rotation;
	transform *= scale;
	
	irr::scene::ITriangleSelector* selector = CreateSelectorFromTransformedBox(boundingBox, transform, sm, g);
	DrawDebugFrame(selector, drawTimeOut, true);
}

void DrawDebugFrame(irr::scene::ITriangleSelector* selector, RakNet::TimeMS drawTimeOut, bool isDrop)
{
	if (collisionBoxQueue == nullptr) return;
	const int maxTriangles = 512;
	std::vector<irr::core::triangle3df> tris(maxTriangles);
	s32 outCount = 0;
	selector->getTriangles(tris.data(), maxTriangles, outCount, &core::IdentityMatrix);
	CollDebugState item = { RakNet::GetTimeMS(),tris,outCount,irr::core::line3d<irr::f32>(), drawTimeOut };
	collisionBoxQueue->q.Push(item, _FILE_AND_LINE_);
	if(isDrop) selector->drop();
}

BaseIrrlichtReplica::BaseIrrlichtReplica()
{
}
BaseIrrlichtReplica::~BaseIrrlichtReplica()
{

}
void BaseIrrlichtReplica::SerializeConstruction(RakNet::BitStream *constructionBitstream, RakNet::Connection_RM3 *destinationConnection)
{
	constructionBitstream->Write(position);
}
bool BaseIrrlichtReplica::DeserializeConstruction(RakNet::BitStream *constructionBitstream, RakNet::Connection_RM3 *sourceConnection)
{
	constructionBitstream->Read(position);
	return true;
}
RM3SerializationResult BaseIrrlichtReplica::Serialize(RakNet::SerializeParameters *serializeParameters)
{
	return RM3SR_BROADCAST_IDENTICALLY;
}
void BaseIrrlichtReplica::Deserialize(RakNet::DeserializeParameters *deserializeParameters)
{
}
void BaseIrrlichtReplica::Update(RakNet::TimeMS curTime)
{
}
PlayerReplica::PlayerReplica()
{
	model=0;
	rotationDeltaPerMS=0.0f;
	isMoving=false;
	deathTimeout=0;
	lastUpdate=bulletCoolTime=RakNet::GetTimeMS();
	isDead = false;
	wasDead = false;
	isBot = false;
	isTeleport = false;
	playerList.Push(this,_FILE_AND_LINE_);
	fq = new DataStructures::Queue<FrameState>();
	killCnt = 0;
	deathCnt = 0;
	shootCnt = 0;
	nextExpectedAmCnt = 1;
}
PlayerReplica::~PlayerReplica()
{
	unsigned int index = playerList.GetIndexOf(this);
	if (index != (unsigned int) -1)
		playerList.RemoveAtIndexFast(index);

	if (topology == SERVER) delete fq;
	debugBox = nullptr;
}

RakNet::RM3ConstructionState PlayerReplica::QueryConstruction(RakNet::Connection_RM3* destinationConnection, RakNet::ReplicaManager3* replicaManager3) { 
	return QueryConstruction_ClientConstruction(destinationConnection, topology != CLIENT);
}
bool PlayerReplica::QueryRemoteConstruction(RakNet::Connection_RM3* sourceConnection) { 
	return QueryRemoteConstruction_ClientConstruction(sourceConnection, topology != CLIENT); 
}
RakNet::RM3QuerySerializationResult PlayerReplica::QuerySerialization(RakNet::Connection_RM3* destinationConnection) { 
	return QuerySerialization_ClientSerializable(destinationConnection, topology != CLIENT); 
}
RakNet::RM3ActionOnPopConnection PlayerReplica::QueryActionOnPopConnection(RakNet::Connection_RM3* droppedConnection) const { return QueryActionOnPopConnection_Client(droppedConnection); }

void PlayerReplica::WriteAllocationID(RakNet::Connection_RM3 *destinationConnection, RakNet::BitStream *allocationIdBitstream) const
{
	allocationIdBitstream->Write(RakNet::RakString("PlayerReplica"));
}
void PlayerReplica::SerializeConstruction(RakNet::BitStream *constructionBitstream, RakNet::Connection_RM3 *destinationConnection)
{
	BaseIrrlichtReplica::SerializeConstruction(constructionBitstream, destinationConnection);
	constructionBitstream->Write(rotationAroundYAxis);
	constructionBitstream->Write(gamePlatform);
	constructionBitstream->Write(demo->GetDevice()->getVideoDriver()->getFPS());
}
bool PlayerReplica::DeserializeConstruction(RakNet::BitStream *constructionBitstream, RakNet::Connection_RM3 *sourceConnection)
{
	if (!BaseIrrlichtReplica::DeserializeConstruction(constructionBitstream, sourceConnection))
		return false;
	constructionBitstream->Read(rotationAroundYAxis);
	constructionBitstream->Read(gamePlatform);
	constructionBitstream->Read(fps);
	//demo->PushMessage(RakNet::RakString("Deserialize Construction"));
	return true;
}
void PlayerReplica::PostDeserializeConstruction(RakNet::BitStream *constructionBitstream, RakNet::Connection_RM3 *destinationConnection)
{
	// Object was remotely created and all data loaded. Now we can make the object visible
	scene::IAnimatedMesh* mesh = 0;
	scene::ISceneManager *sm = demo->GetSceneManager();
	mesh = sm->getMesh(IRRLICHT_MEDIA_PATH "sydney.md2");
	model = sm->addAnimatedMeshSceneNode(mesh, 0);

	//debugBox = new DebugBoxSceneNode(model,sm);
	//debugBox->demo=demo;
	//debugBox->setDebugDataVisible(true); 
	//debugBox->EnableDrawTriangles(true);
	//
	//scene::ITriangleSelector* selector = CreateSelectorFromTransformedBox(demo->GetSyndeyBoundingBox(), model->getAbsoluteTransformation(), sm, creatingSystemGUID);
	//model->setTriangleSelector(selector);
	//selector->drop();  // 참조 카운트 관리
	//debugBox->SetSelector(model->getTriangleSelector());

	model->setPosition(position);
	model->setRotation(core::vector3df(0, rotationAroundYAxis, 0));
	model->setScale(core::vector3df(2,2,2));
	model->setMD2Animation(scene::EMAT_STAND);
	
	curAnim=scene::EMAT_STAND;
	model->setMaterialTexture(0, demo->GetDevice()->getVideoDriver()->getTexture(IRRLICHT_MEDIA_PATH "sydney.bmp"));
	model->setMaterialFlag(video::EMF_LIGHTING, true);
	model->addShadowVolumeSceneNode();
	model->setAutomaticCulling ( scene::EAC_BOX );
	model->setVisible(true);
	model->setAnimationEndCallback(this);
	wchar_t playerNameWChar[1024];
	mbstowcs(playerNameWChar, playerName.C_String(), 1023);
	// ensure wide-character string is null terminated (i.e. if playerName length is >= 1023)
	playerNameWChar[1023] = L'\0';
	scene::IBillboardSceneNode *bb = sm->addBillboardTextSceneNode(0, playerNameWChar, model);
	bb->setSize(core::dimension2df(40,20));
	bb->setPosition(core::vector3df(0,model->getBoundingBox().MaxEdge.Y+bb->getBoundingBox().MaxEdge.Y-bb->getBoundingBox().MinEdge.Y+5.0,0));
	bb->setColor(video::SColor(255,255,128,128), video::SColor(255,255,128,128));
}
void PlayerReplica::PreDestruction(RakNet::Connection_RM3 *sourceConnection)
{
	if (model)
		model->remove();
}
RM3SerializationResult PlayerReplica::Serialize(RakNet::SerializeParameters *serializeParameters)
{
	BaseIrrlichtReplica::Serialize(serializeParameters);
	serializeParameters->outputBitstream[0].Write(position);
	serializeParameters->outputBitstream[0].Write(rotationAroundYAxis);
	serializeParameters->outputBitstream[0].Write(isMoving);
	serializeParameters->outputBitstream[0].Write(gamePlatform);

	if (topology == SERVER) {
		//서버에서는 전달만
		serializeParameters->outputBitstream[0].Write(true);
		serializeParameters->outputBitstream[0].Write(shootPosition);
		serializeParameters->outputBitstream[0].Write(shootDirection);
	}
	else {
		scene::ISceneManager* sm = demo->GetSceneManager();
		scene::ICameraSceneNode* camera = sm->getActiveCamera();
		if (camera == nullptr) {
			serializeParameters->outputBitstream[0].Write(false);
		}
		else {
			serializeParameters->outputBitstream[0].Write(true);
			core::vector3df camPosition = camera->getPosition();
			core::vector3df camAt = (camera->getTarget() - camPosition);
			camAt.normalize();
			serializeParameters->outputBitstream[0].Write(camPosition);
			serializeParameters->outputBitstream[0].Write(camAt);
		}
	}

	//서버에서는 전달만2
	serializeParameters->outputBitstream[0].Write((topology == SERVER) ? fps : demo->GetDevice()->getVideoDriver()->getFPS());
	
	//timeStamp
	//serializeParameters->messageTimestamp = RakNet::GetTimeMS();
	//return RM3SR_BROADCAST_IDENTICALLY; 
	return RM3SR_BROADCAST_IDENTICALLY_FORCE_SERIALIZATION; //값이 안바뀌어도 계속 동기화됨
}
//destination에 존재하지 않는 객체를 전송해야 하는지?

void PlayerReplica::Deserialize(RakNet::DeserializeParameters *deserializeParameters)
{
	BaseIrrlichtReplica::Deserialize(deserializeParameters);
	deserializeParameters->serializationBitstream[0].Read(position);
	deserializeParameters->serializationBitstream[0].Read(rotationAroundYAxis);
	deserializeParameters->serializationBitstream[0].Read(isMoving);
	deserializeParameters->serializationBitstream[0].Read(gamePlatform);

	deserializeParameters->serializationBitstream[0].Read(isCreatedCamera);
	if (isCreatedCamera) {
		deserializeParameters->serializationBitstream[0].Read(shootPosition);
		deserializeParameters->serializationBitstream[0].Read(shootDirection);
	}
	deserializeParameters->serializationBitstream[0].Read(fps);

	RakNet::TimeMS curTime = RakNet::GetTimeMS();
	// Is a locally created object?
	if (creatingSystemGUID == rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))
	{

	}
	else {
		core::vector3df positionOffset;
		positionOffset = position - model->getPosition();
		positionDeltaPerMS = positionOffset / INTERP_TIME_MS;

		float rotationOffset;
		rotationOffset = GetRotationDifference(rotationAroundYAxis, model->getRotation().Y);
		rotationDeltaPerMS = rotationOffset / INTERP_TIME_MS;
		interpEndTime = curTime + (RakNet::TimeMS)INTERP_TIME_MS;
	}
}
void PlayerReplica::Update(RakNet::TimeMS curTime)
{
	if (topology == SERVER) {
		//부활 확인
		bool tmp = IsDead();
		if (wasDead && !tmp) {
			RakNet::BitStream bs;
			bs.Write((RakNet::MessageID)CDemo::ID_GAME_MESSAGE_PLAYER_LIFE);
			bs.Write(creatingSystemGUID);
			bs.Write(tmp);
			wasDead = false;
			rakPeer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
			if(isBot){
				if (creatingSystemGUID == rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))
				{
					//서버 봇이면 즉시 리스폰
					demo->SetResetBot();
					demo->Respawn(respawnPos, respawnTarget);
					demo->botMoveTime = RakNet::GetTimeMS() + demo->BOT_MOVE_TIME;
				}

				//리스폰 요청 보내기
				RakNet::BitStream bs;
				bs.Write((RakNet::MessageID)CDemo::ID_GAME_MESSAGE_PLAYER_RESPAWN);
				bs.Write(creatingSystemGUID);
				bs.Write(respawnPos);
				bs.Write(respawnTarget);

				if (creatingSystemGUID == rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS)) {
					//서버 봇은 바로 브로드 캐스트
					rakPeer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
				}				
			}
		}
	}

	//Set Animation
	if (creatingSystemGUID != rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS)) {
		if ((topology == SERVER) ? IsDead() : isDead)
		{
			UpdateAnimation(scene::EMAT_DEATH_FALLBACK);
			model->setLoopMode(false);
		}
		else if (curAnim != scene::EMAT_ATTACK)
		{
			if (isMoving)
			{
				UpdateAnimation(scene::EMAT_RUN);
				model->setLoopMode(true);
			}
			else
			{
				UpdateAnimation(scene::EMAT_STAND);
				model->setLoopMode(true);
			}
		}
	}
	

	//record frame state
	if (topology == SERVER)
	{
		if (fq == nullptr) return;
		if (!isBot && !isCreatedCamera) return;
		if (IsDead()) return;

		//bot이 아니면 isCreatedCamera 필요
		//bot 이면 isCreatedCamera 필요 없음 (총을 안쏘므로)
		core::matrix4 transform;
		transform.setTranslation(position);

		core::matrix4 rotation;
		rotation.setRotationDegrees(core::vector3df(0, rotationAroundYAxis, 0));

		core::matrix4 scale;
		scale.setScale(core::vector3df(1, 1, 1));

		transform *= rotation;
		transform *= scale;

		//TODO : shootPosition, shootDirection 모두 position 처럼 보정을 해줄 필요가 있음
		//TODO : 지금은 서버 봇이 총을 안쏴서 shoot 관련 변수가 0,0,0 으로 초기화되어 있음. 다만 나중에 총을 쏜다면 serialize~deserialize 시에 값 초기화 필요

		FrameState frame{ curTime - (rakPeer->GetAveragePing(creatingSystemGUID) / 2), transform, shootPosition, shootDirection };
		fq->Push(frame, _FILE_AND_LINE_);
		//맨 앞에 남아있는 프레임부터 차례대로 검사 -> 현재 시간이랑 1초 이상 차이나면 버림
		while (!fq->IsEmpty() && frame.timeStamp - fq->Peek().timeStamp > HISTORY_DURATION_MS)
			fq->Pop();

		//만약 이 플레이어가 Holder라면
		//if (gamePlatform == Holder) {
		//	//현재 Shoooter가 Holder에게 총을 쐈을 때 
		//	//Time Wrap가 적용되는 위치를 출력한다 (Shooter ~ Server 간 딜레이가 없다고 가정)
		//	RakNet::TimeMS now = curTime - INTERP_TIME_MS;
		//	core::vector3df printPos;
		//	for (int frameIdx = 1; frameIdx < fq->Size(); frameIdx++)
		//	{
		//		if ((*fq)[frameIdx].timeStamp >= now) {
		//			//now가 frameIdx-1 과 frameIdx 사이에 있으므로 보간
		//			const FrameState& f0 = (*fq)[frameIdx - 1];
		//			const FrameState& f1 = (*fq)[frameIdx];
		//			float alpha = float(now - f0.timeStamp) / float(f1.timeStamp - f0.timeStamp);

		//			//Shot Position 보간
		//			//= f0.shotPosition.getInterpolated(f1.shotPosition, alpha);
		//			printPos = f0.collisionTransform.getTranslation().getInterpolated(f1.collisionTransform.getTranslation(), alpha);
		//			break;
		//		}
		//	}
		//	PrintHoldPosOneLine(printPos.X, printPos.Y, printPos.Z);
		//}
	}

	
	// Is a locally created object?
	// 이동 적용
	if (creatingSystemGUID==rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))
	{	
		playerReplica->position = demo->GetSceneManager()->getActiveCamera()->getPosition() - irr::core::vector3df(0, CAMERA_HEIGHT, 0);
		playerReplica->rotationAroundYAxis = demo->GetSceneManager()->getActiveCamera()->getRotation().Y - 90.0f;

		if (playerBotReplica) {
			playerBotReplica->botModel->setPosition(playerBotReplica->position);
			playerBotReplica->botModel->setRotation(core::vector3df(0, playerBotReplica->rotationAroundYAxis, 0));
		}
		
		// Local player has no mesh to interpolate
		// Input our camera position as our player position
		isMoving=demo->IsMovementKeyDown();

		// Ack, makes the screen messed up and the mouse move off the window
		// Find another way to keep the dead player from moving
		// 서버 봇 적용하면 이동시 튕기는 문제 발생
		if(topology != SERVER) demo->EnableInput((topology == SERVER) ? IsDead() == false : isDead == false);

		//DebugPrintf("Player position : %f, %f, %f / isKeyLock : %d / wasKeyLock : %d \n", position.X, position.Y, position.Z, demo->isKeyLock, demo->wasKeyLock);
		// DebugPrintf("Player target : %f, %f, %f\n", demo->GetSceneManager()->getActiveCamera()->getTarget().X, demo->GetSceneManager()->getActiveCamera()->getTarget().Y, demo->GetSceneManager()->getActiveCamera()->getTarget().Z);
	
		if (demo->gamePlatform == Holder) {
			demo->SetHolderPosText(playerReplica->position);
		}
	}

	if (creatingSystemGUID == rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS)) return;

	//Debug Frame
	//DrawDebugFrame(demo->GetSyndeyBoundingBox(),position, rotationAroundYAxis, demo->GetSceneManager(), creatingSystemGUID, 500);


	//원격에서 보는 봇 + 리스폰 명령 받았을 때 
	//딱 한번 보간없이 강제 이동함
	if (isBot)
	{
		//method 1 일 때는 봇 보간 끄기
		if (isTeleport || (demo->evalMask & METHOD_1)) {
			model->setPosition(position);
			model->setRotation(core::vector3df(0, rotationAroundYAxis, 0));
			lastUpdate = curTime;
			interpEndTime = curTime;
			isTeleport = false;
			bulletCoolTime = 0;
			//DebugPrintf("reset!\n");
			return;
		}
		

	}

	// Update interpolation at Remote
	RakNet::TimeMS elapsed = curTime-lastUpdate;
	//DebugPrintf("Update curTime : %d, lastUpdate : %d, elapsed time : %d, guid : %llu\n", curTime, lastUpdate, elapsed, creatingSystemGUID.g);
	if (elapsed<=1)
		return;
	if (elapsed>100)
		elapsed=100;

	lastUpdate=curTime;

	irr::core::vector3df curPositionDelta = position-model->getPosition();
	irr::core::vector3df interpThisTick = positionDeltaPerMS*(float) elapsed;
	if (curTime < interpEndTime && interpThisTick.getLengthSQ() < curPositionDelta.getLengthSQ())
	{
		model->setPosition(model->getPosition()+positionDeltaPerMS*(float) elapsed);
		//DebugPrintf("interPolation\n");
	}
	else
	{
		model->setPosition(position);
	}

	float curRotationDelta = GetRotationDifference(rotationAroundYAxis,model->getRotation().Y);
	float interpThisTickRotation = rotationDeltaPerMS*(float)elapsed;
	if (curTime < interpEndTime && fabs(interpThisTickRotation) < fabs(curRotationDelta))
	{
		model->setRotation(model->getRotation()+core::vector3df(0,interpThisTickRotation,0));
	}
	else
	{
		model->setRotation(core::vector3df(0,rotationAroundYAxis,0));
	}

	//Print HolderPos
	if (isBot && (demo->evalMask==0) ) {
		
		//보간된 위치
		demo->SetHolderPosText(model->getPosition());
		core::vector3df pos = model->getPosition();

		if (isDead || bulletCoolTime > RakNet::GetTimeMS() || bulletCoolTime == -1 || topology == SERVER) {
			return;
		}
		
		if (isWithinRange(pos.Y, 167.0f, 20.0f) && isWithinRange(pos.Z, -288.0f, 15.0f)) {
			bulletCoolTime = RakNet::GetTimeMS() + BULLET_COOL_TIME;
			demo->isShoot = true;
			
		}
	}
}
void PlayerReplica::UpdateAnimation(irr::scene::EMD2_ANIMATION_TYPE anim)
{
	if (anim!=curAnim && model)
		model->setMD2Animation(anim);
	curAnim=anim;
}
float PlayerReplica::GetRotationDifference(float r1, float r2)
{
	float diff = r1-r2;
	while (diff>180.0f)
		diff-=360.0f;
	while (diff<-180.0f)
		diff+=360.0f;
	return diff;
}
void PlayerReplica::OnAnimationEnd(scene::IAnimatedMeshSceneNode* node)
{
	if (curAnim==scene::EMAT_ATTACK)
	{
		if (isMoving)
		{
			UpdateAnimation(scene::EMAT_RUN);
			if(model)model->setLoopMode(true);
		}
		else
		{
			UpdateAnimation(scene::EMAT_STAND);
			if(model)model->setLoopMode(true);
		}
	}
}
void PlayerReplica::PlayAttackAnimation(void)
{
	if ((topology == SERVER) ? IsDead() == false : isDead == false)
	{
		UpdateAnimation(scene::EMAT_ATTACK);
		if(model)model->setLoopMode(false);		
	}
}
bool PlayerReplica::IsDead(void) const
{
	return deathTimeout > RakNet::GetTimeMS();
}

PlayerBotReplica::PlayerBotReplica()
{
	botModel = 0;
	isBot = true;
}

void PlayerBotReplica::PreDestruction(RakNet::Connection_RM3* sourceConnection)
{
	PlayerReplica::PreDestruction(sourceConnection);
	if (botModel)
		botModel->remove();
}

RakNet::RM3ConstructionState PlayerBotReplica::QueryConstruction(RakNet::Connection_RM3* destinationConnection, RakNet::ReplicaManager3* replicaManager3) {
	return QueryConstruction_ClientConstruction(destinationConnection, topology != CLIENT);
}
bool PlayerBotReplica::QueryRemoteConstruction(RakNet::Connection_RM3* sourceConnection) {
	return QueryRemoteConstruction_ClientConstruction(sourceConnection, topology != CLIENT);
}
RakNet::RM3QuerySerializationResult PlayerBotReplica::QuerySerialization(RakNet::Connection_RM3* destinationConnection) {
	return QuerySerialization_ClientSerializable(destinationConnection, topology != CLIENT);
}
RakNet::RM3ActionOnPopConnection PlayerBotReplica::QueryActionOnPopConnection(RakNet::Connection_RM3* droppedConnection) const { return QueryActionOnPopConnection_Client(droppedConnection); }

void PlayerBotReplica::WriteAllocationID(RakNet::Connection_RM3* destinationConnection, RakNet::BitStream* allocationIdBitstream) const
{
	allocationIdBitstream->Write(RakNet::RakString("PlayerBotReplica"));
}

void PlayerBotReplica::SerializeConstruction(RakNet::BitStream* constructionBitstream, RakNet::Connection_RM3* destinationConnection)
{
	PlayerReplica::SerializeConstruction(constructionBitstream, destinationConnection);
	constructionBitstream->Write(isBot);
}
bool PlayerBotReplica::DeserializeConstruction(RakNet::BitStream* constructionBitstream, RakNet::Connection_RM3* sourceConnection)
{
	if (!PlayerReplica::DeserializeConstruction(constructionBitstream, sourceConnection))
		return false;
	constructionBitstream->Read(isBot);
	return true;
}

RM3SerializationResult PlayerBotReplica::Serialize(RakNet::SerializeParameters* serializeParameters)
{
	BaseIrrlichtReplica::Serialize(serializeParameters);
	serializeParameters->outputBitstream[0].Write(position);
	serializeParameters->outputBitstream[0].Write(rotationAroundYAxis);
	serializeParameters->outputBitstream[0].Write(isMoving);
	serializeParameters->outputBitstream[0].Write(gamePlatform);

	if (topology == SERVER) {
		serializeParameters->outputBitstream[0].Write(++demo->um_cnt);
		demo->umTimeMap.Set(demo->um_cnt, RakNet::GetTimeMS());
	}
	
	//TODO : umTimeList size overflow check

	//timeStamp
	//serializeParameters->messageTimestamp = RakNet::GetTimeMS();
	//return RM3SR_BROADCAST_IDENTICALLY; 
	return RM3SR_BROADCAST_IDENTICALLY_FORCE_SERIALIZATION; //값이 안바뀌어도 계속 동기화됨
}
//destination에 존재하지 않는 객체를 전송해야 하는지?

void PlayerBotReplica::Deserialize(RakNet::DeserializeParameters* deserializeParameters)
{
	BaseIrrlichtReplica::Deserialize(deserializeParameters);
	deserializeParameters->serializationBitstream[0].Read(position);
	deserializeParameters->serializationBitstream[0].Read(rotationAroundYAxis);
	deserializeParameters->serializationBitstream[0].Read(isMoving);
	deserializeParameters->serializationBitstream[0].Read(gamePlatform);
	
	if (topology != SERVER) {
		deserializeParameters->serializationBitstream[0].Read(demo->um_cnt);
		demo->reactionTime = RakNet::GetTimeMS();
		demo->umReceptionTimes.Set(demo->um_cnt, RakNet::GetTimeMS());
	}

	// Is a locally created object?
	if (creatingSystemGUID == rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))
	{
	}
	else {
		core::vector3df positionOffset;
		positionOffset = position - model->getPosition();
		positionDeltaPerMS = positionOffset / INTERP_TIME_MS;

		float rotationOffset;
		rotationOffset = GetRotationDifference(rotationAroundYAxis, model->getRotation().Y);
		rotationDeltaPerMS = rotationOffset / INTERP_TIME_MS;
		interpEndTime = RakNet::GetTimeMS() + (RakNet::TimeMS)INTERP_TIME_MS;
	}
}

void PlayerBotReplica::CreateBotModel() 
{
	scene::IAnimatedMesh* mesh = 0;
	scene::ISceneManager* sm = demo->GetSceneManager();
	mesh = sm->getMesh(IRRLICHT_MEDIA_PATH "sydney.md2");
	botModel = sm->addAnimatedMeshSceneNode(mesh, 0);

	botModel->setPosition(position);
	botModel->setRotation(core::vector3df(0, rotationAroundYAxis, 0));
	botModel->setScale(core::vector3df(2, 2, 2));
	botModel->setMD2Animation(scene::EMAT_STAND);

	curAnim = scene::EMAT_STAND;
	botModel->setMaterialTexture(0, demo->GetDevice()->getVideoDriver()->getTexture(IRRLICHT_MEDIA_PATH "sydney.bmp"));
	botModel->setMaterialFlag(video::EMF_LIGHTING, true);
	botModel->addShadowVolumeSceneNode();
	botModel->setAutomaticCulling(scene::EAC_BOX);
	botModel->setVisible(true);
	botModel->setAnimationEndCallback(this);
}

BallReplica::BallReplica()
{
	creationTime=RakNet::GetTimeMS();
}
BallReplica::~BallReplica()
{
}
void BallReplica::WriteAllocationID(RakNet::Connection_RM3 *destinationConnection, RakNet::BitStream *allocationIdBitstream) const 
{
	allocationIdBitstream->Write(RakNet::RakString("BallReplica"));
}

RakNet::RM3ConstructionState BallReplica::QueryConstruction(RakNet::Connection_RM3* destinationConnection, RakNet::ReplicaManager3* replicaManager3) { 
	return QueryConstruction_ClientConstruction(destinationConnection, topology != CLIENT); 
}
bool BallReplica::QueryRemoteConstruction(RakNet::Connection_RM3* sourceConnection) { return QueryRemoteConstruction_ClientConstruction(sourceConnection, topology != CLIENT); }
RakNet::RM3QuerySerializationResult BallReplica::QuerySerialization(RakNet::Connection_RM3* destinationConnection) { return QuerySerialization_ClientSerializable(destinationConnection, topology != CLIENT); }
RakNet::RM3ActionOnPopConnection BallReplica::QueryActionOnPopConnection(RakNet::Connection_RM3* droppedConnection) const { return QueryActionOnPopConnection_Client(droppedConnection); }

void BallReplica::SerializeConstruction(RakNet::BitStream *constructionBitstream, RakNet::Connection_RM3 *destinationConnection)
{
	BaseIrrlichtReplica::SerializeConstruction(constructionBitstream, destinationConnection);
	constructionBitstream->Write(shotDirection);
	constructionBitstream->Write(bulletCount);

	if ((demo->evalMask & METHOD_2) == 0) return;
	if (topology == SERVER) return;

	RakNet::TimeMS actionTime = RakNet::GetTimeMS();
	// am_cnt는 한 번만
	constructionBitstream->Write(++demo->am_cnt); 

	// (간단한 버전) 맵의 모든 항목에 대해 (um_id, delta) 튜플을 전송
	int tupleCount = demo->umReceptionTimes.Size();
	constructionBitstream->Write(tupleCount);

	for (unsigned int i = 0; i < demo->umReceptionTimes.Size(); ++i)
	{
		int um_id = demo->umReceptionTimes.GetKeyAtIndex(i);
		RakNet::TimeMS receptionTime = demo->umReceptionTimes.Get(um_id);
		RakNet::TimeMS delta = actionTime - receptionTime;
		constructionBitstream->Write(um_id);
		constructionBitstream->Write(delta);
	}
	
	////TimeStamp
	//constructionBitstream->Write(GetCurrentTimeMS());
}

bool BallReplica::DeserializeConstruction(RakNet::BitStream* constructionBitstream, RakNet::Connection_RM3* sourceConnection)
{
	if (!BaseIrrlichtReplica::DeserializeConstruction(constructionBitstream, sourceConnection))
		return false;
	constructionBitstream->Read(shotDirection);
	constructionBitstream->Read(bulletCount);
	
	
	if (creatingSystemGUID != rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))
	{
		RakNet::TimeMS estimatedElapsed = rakPeer->GetAveragePing(sourceConnection->GetSystemAddress()) / 2;
		position = position + shotDirection * (float)estimatedElapsed * SHOT_SPEED;
	}

	if (topology != SERVER ||(demo->evalMask & METHOD_2) == 0) return true;

	// 이 변수들은 루프 밖에서 한 번만 선언합니다.
	int am_cnt; int tupleCount;
	constructionBitstream->Read(am_cnt);
	constructionBitstream->Read(tupleCount);

	RakNet::RakNetGUID guid = creatingSystemGUID;
	RakNet::TimeMS ingoingTime = RakNet::GetTimeMS();

	for (int i = 0; i < tupleCount; ++i)
	{
		int um_cnt;
		RakNet::TimeMS rt; // reactionTime
		constructionBitstream->Read(um_cnt);
		constructionBitstream->Read(rt);

		// (i, delta) 튜플마다 큐에 삽입
		uint64_t weight = ((uint64_t)um_cnt << 32) | (uint64_t)rt;
		orderData data = { um_cnt, am_cnt, 0, rt, ingoingTime, 0, guid, false };

		// --- AM 시퀀스(순서) 검사 로직 ---
		// 이 AM을 보낸 플레이어를 찾음
		PlayerReplica* senderPlayer = nullptr;
		for (unsigned int p_idx = 0; p_idx < PlayerReplica::playerList.Size(); ++p_idx) {
			if (PlayerReplica::playerList[p_idx]->creatingSystemGUID == data.playerGUID) {
				senderPlayer = PlayerReplica::playerList[p_idx];
				break;
			}
		}

		if (senderPlayer)
		{
			if (data.am_cnt == senderPlayer->nextExpectedAmCnt)
			{
				// 1. 순서가 맞음
				data.isSequenced = true;
				senderPlayer->nextExpectedAmCnt++;
				demo->orderPq.Push(weight, data, __FILE__, __LINE__); // 큐에 즉시 삽입

				// 2. (선택적) 버퍼에 있던 다음 순서의 메시지들도 처리
				while (senderPlayer->outOfOrderAmBuffer.Has(senderPlayer->nextExpectedAmCnt))
				{
					orderData bufferedData = senderPlayer->outOfOrderAmBuffer.Get(senderPlayer->nextExpectedAmCnt);
					senderPlayer->outOfOrderAmBuffer.Delete(senderPlayer->nextExpectedAmCnt);

					bufferedData.isSequenced = true;
					uint64_t bufferedWeight = ((uint64_t)bufferedData.um_cnt << 32) | (uint64_t)bufferedData.reactionTime;
					demo->orderPq.Push(bufferedWeight, bufferedData, __FILE__, __LINE__); // 버퍼 -> 큐로 이동

					senderPlayer->nextExpectedAmCnt++;
				}
			}
			else if (data.am_cnt > senderPlayer->nextExpectedAmCnt)
			{
				// 2. 순서가 어긋남 (미래의 패킷이 먼저 옴)
				data.isSequenced = false;
				// (선택적) 일단 버퍼에 보관. 큐에는 넣지 않음.
				senderPlayer->outOfOrderAmBuffer.Set(data.am_cnt, data);
			}
			else
			{
				// 3. (data.am_cnt < senderPlayer->nextExpectedAmCnt)
				// 이미 처리된 패킷 (늦게 온 중복 패킷). 무시.
			}
		}
		// --- AM 시퀀스 검사 로직 끝 ---
	}

	// (TimeStamp 로직은 주석 처리됨)
	//long long timeStampMS;
	//constructionBitstream->Read(timeStampMS);

	return true;
}

void BallReplica::PostSerializeConstruction(RakNet::BitStream* constructionBitstream, RakNet::Connection_RM3* destinationConnection)
{
}

void BallReplica::PostDeserializeConstruction(RakNet::BitStream *constructionBitstream, RakNet::Connection_RM3 *destinationConnection)
{

	// Shot visible effect and BallReplica classes are not linked, but they update the same way, such that
	// they are in the same spot all the time
	// effect
	if (topology != SERVER) {
		GamePlatform platform = GamePlatform::Server;
		for (int idx = 0; idx < PlayerReplica::playerList.Size(); ++idx)
		{
			auto* player = PlayerReplica::playerList[idx];
			if (player->creatingSystemGUID == creatingSystemGUID) {
				platform = player->gamePlatform;
				break;
			}
		}
		demo->shootFromOrigin(position, shotDirection, platform);
		return;
	}
	
	//method 2를 쓸 때는 충돌 검사 하지 않음
	if (demo->evalMask & METHOD_2)
		return;
	
	demo->BulletHitDetected(creatingSystemGUID,0);

	// Hit Scan Only
	//core::vector3df start = position;
	//core::vector3df end = start + (shotDirection * camera->getFarValue());
	//core::line3d<irr::f32> line(start, end);
	//core::triangle3df triangle;
	//core::vector3df hitPoint;
	//const scene::ISceneNode* hitNode;
	//
	//for (idx = 0; idx < PlayerReplica::playerList.Size(); ++idx)
	//{
	//	auto* player = PlayerReplica::playerList[idx];
	//	if (player->creatingSystemGUID == ownerGUID) {
	//		player->PlayAttackAnimation();
	//		continue; // 총을 쏜 본인
	//	}
	//	
	//	scene::ITriangleSelector* selector = nullptr;
	//	core::matrix4 transform;
	//	scene::ISceneNode* node = nullptr;

	//	if (PlayerBotReplica* bot = dynamic_cast<PlayerBotReplica*>(player)) {
	//		if (bot->IsDead()) continue;
	//		transform = bot->botModel->getAbsoluteTransformation();
	//		selector = CreateSelectorFromTransformedBox(demo->GetSyndeyBoundingBox(), transform, sm, bot->creatingSystemGUID);
	//		node = bot->botModel;
	//	}
	//	else {
	//		if (player->IsDead()) continue;
	//		transform = player->model->getAbsoluteTransformation();
	//		selector = CreateSelectorFromTransformedBox(demo->GetSyndeyBoundingBox(), transform, sm, player->creatingSystemGUID);
	//		node = player->model;
	//	}

	//	if (selector == nullptr)
	//		continue;

	//	// 충돌 검사
	//	// TODO : Mobile은 getCollisionPoint 인수가 HitResult 구조체
	//	bool hit = sm->getSceneCollisionManager()->getCollisionPoint(line, selector, hitPoint, triangle, hitNode);
	//	selector->drop(); // drop 꼭 해주기

	//	if (hit && hitNode && hitNode->getID() == static_cast<s32>(player->creatingSystemGUID.g))
	//	{
	//		player->deathTimeout = RakNet::GetTimeMS() + 3000;
	//		RakNet::RakString msg("%s Dead from : %s",
	//			dynamic_cast<PlayerBotReplica*>(player) ? "Bot" : "Player",
	//			rakPeer->GetSystemAddressFromGuid(creatingSystemGUID).ToString(true));
	//		demo->PushMessage(msg);
	//		//OutputDebugStringA(msg.C_String());
	//		
	//		RakNet::BitStream bs;
	//		bs.Write((RakNet::MessageID)CDemo::ID_GAME_MESSAGE_PLAYER_LIFE);
	//		bs.Write(player->creatingSystemGUID);
	//		bs.Write(player->IsDead());
	//		player->wasDead = true;

	//		rakPeer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
	//		break; // 더 검사하지 않음
	//	}
	//}
}

void BallReplica::PreDestruction(RakNet::Connection_RM3 *sourceConnection)
{
	// The system that shot this ball destroyed it, or disconnected
	// Technically we should clear out the node visible effect too, but it's not important for now
}
RM3SerializationResult BallReplica::Serialize(RakNet::SerializeParameters *serializeParameters)
{
	BaseIrrlichtReplica::Serialize(serializeParameters);
	return RM3SR_BROADCAST_IDENTICALLY;
}
void BallReplica::Deserialize(RakNet::DeserializeParameters *deserializeParameters)
{
	BaseIrrlichtReplica::Deserialize(deserializeParameters);
}
void BallReplica::Update(RakNet::TimeMS curTime)
{
	// Is a locally created object?
	if (creatingSystemGUID==rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))
	{
		// Destroy if shot expired
		if (curTime > shotLifetime)
		{
			// Destroy on network
			BroadcastDestruction();
			delete this;
			return;
		}
	}

	//// Keep at the same position as the visible effect
	//// Deterministic, so no need to actually transmit position
	//// The variable position is the origin that the ball was created at. For the player, it is their actual position
	//RakNet::TimeMS elapsedTime;
	//// Due to ping variances and timestamp miscalculations, it's possible with very low pings to get a slightly negative time, so we have to check
	//if (curTime>=creationTime)
	//	elapsedTime = curTime - creationTime;
	//else
	//	elapsedTime=0;
	//irr::core::vector3df updatedPosition = position + shotDirection * (float) elapsedTime * SHOT_SPEED;

	////See if the bullet hit us
	////외부 총알을 맞았을 때
	//if (creatingSystemGUID != rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))
	//{
	//	bool check = (topology == SERVER && playerBotReplica->IsDead() == false) || (topology == CLIENT && playerReplica->IsDead() == false);
	//	if (check) {
	//		float playerHalfHeight = demo->GetSyndeyBoundingBox().getExtent().Y / 2;
	//		irr::core::vector3df positionRelativeToCharacter = updatedPosition - ((topology == SERVER) ?playerBotReplica->position : playerReplica->position);//+core::vector3df(0,playerHalfHeight,0);
	//		if (demo->GetSyndeyBoundingBox().isPointInside(positionRelativeToCharacter))
	//			//if ((playerReplica->position+core::vector3df(0,playerHalfHeight,0)-updatedPosition).getLengthSQ() < BALL_DIAMETER*BALL_DIAMETER/4.0f)
	//		{
	//			// We're dead for 3 seconds
	//			if (topology == SERVER) {
	//				playerBotReplica->deathTimeout = curTime + 3000;
	//				demo->PushMessage(RakNet::RakString("Bot Dead from : ") + rakPeer->GetSystemAddressFromGuid(creatingSystemGUID).ToString(true));

	//			}else playerReplica->deathTimeout = curTime + 3000;
	//		}
	//	}
	//}
}
RakNet::Replica3 *Connection_RM3Irrlicht::AllocReplica(RakNet::BitStream *allocationId, ReplicaManager3 *replicaManager3)
{
	RakNet::RakString typeName; allocationId->Read(typeName);
	if (typeName=="PlayerReplica") {BaseIrrlichtReplica *r = new PlayerReplica; r->demo=demo; return r;}
	if (typeName == "PlayerBotReplica") { BaseIrrlichtReplica* r = new PlayerBotReplica; r->demo = demo; return r; }
	if (typeName=="BallReplica") {BaseIrrlichtReplica *r = new BallReplica; r->demo=demo; return r;}
	return 0;
}

void ReplicaManager3Irrlicht::PrintTimeGap(char* str) {
	statBufList[0].Push(RakNet::RakString(str), _FILE_AND_LINE_); //Log
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

//Time Wrap
			//if (topology == SERVER) {
			//	auto* q = frameHistoryMap.Peek(PlayerReplica::playerList[idx]->creatingSystemGUID);
			//	if (q == nullptr) continue; // No history for this player

			//	bool hit = false;
			//	RakNet::Time now = RakNet::GetTimeMS()-(rakPeer->GetAveragePing(creatingSystemGUID)/2); //-ping time
			//	
			//	//char buf[64];
			//	//snprintf(buf, sizeof(buf), "now : %d\n", now);
			//	//OutputDebugStringA(buf); 
			//	
			//	for (int frameIdx = 1; frameIdx < q->Size(); ++frameIdx)
			//	{
			//		//char buf2[64];
			//		//snprintf(buf2, sizeof(buf2), "frame time : %d\n", (*q)[frameIdx].timeStamp);
			//		//OutputDebugStringA(buf2);

			//		if ((*q)[frameIdx].timeStamp >= now) {
			//			const FrameState& f0 = (*q)[frameIdx - 1]; const FrameState& f1 = (*q)[frameIdx];
			//			float alpha = float(now - f0.timeStamp) / float(f1.timeStamp - f0.timeStamp);
			//			core::vector3df targetPos = f0.position.getInterpolated(f1.position, alpha);
			//			
			//			core::vector3df toTarget = targetPos - position;
			//			float distToLine = (toTarget.crossProduct(shotDirection)).getLength() / shotDirection.getLength();
			//			if (distToLine <= BALL_DIAMETER / 2.0f) {
			//				playerBotReplica->deathTimeout = now + 3000;
			//				hit = true;
			//			}

			//			if (demo) {
			//				auto* driver = demo->GetSceneManager()->getVideoDriver();
			//				core::vector3df end = position + shotDirection * 100.0f;
			//				driver->draw3DLine(position, end, video::SColor(255, 255, 0, 0));
			//				driver->draw3DBox(core::aabbox3df(targetPos - core::vector3df(1), targetPos + core::vector3df(1)),
			//					video::SColor(255, 0, hit ? 255 : 50, 0));
			//			}
			//		}
			//	}

			//	// fallback: 마지막 위치에서 판정
			//	if (q->Size() >= 1) {
			//		const core::vector3df& targetPos = q->PeekTail().position;
			//		core::vector3df toTarget = targetPos - position;
			//		float distToLine = (toTarget.crossProduct(shotDirection)).getLength() / shotDirection.getLength();
			//		if (distToLine <= BALL_DIAMETER / 2.0f) {
			//			playerBotReplica->deathTimeout = now + 3000;
			//		}
			//	}
			// }