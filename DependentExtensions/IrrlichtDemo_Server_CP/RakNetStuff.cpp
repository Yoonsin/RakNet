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
int score = 0;
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

DataStructures::List<RakNet::RakString> statBuf;
DataStructures::List<PlayerReplica*> PlayerReplica::playerList;
const int HISTORY_DURATION_MS = 1000;

// Take this many milliseconds to move the visible position to the real position
static const float INTERP_TIME_MS=100.0f;
static const int INGOING_TIME_MS = 0;

void InstantiateRakNetClasses(bool isServer, bool isLogged, CDemo* demo)
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
		// Fast disconnect for easier testing of host migration
		//rakPeer->SetTimeoutTime(5000, UNASSIGNED_SYSTEM_ADDRESS);
	}
	else sr = rakPeer->Startup(1, &sd, 1);
	rakPeer->SetOccasionalPing(true);

	RakAssert(sr==RakNet::RAKNET_STARTED);
		
	// ReplicaManager3 replies on NetworkIDManager. It assigns numbers to objects so they can be looked up over the network
	// It's a class in case you wanted to have multiple worlds, then you could have multiple instances of NetworkIDManager
	networkIDManager=new NetworkIDManager;
	
	// Automatically sends around new / deleted / changed game objects
	replicaManager3=new ReplicaManager3Irrlicht;
	replicaManager3->SetIsLog(isLogged);
	
	replicaManager3->SetNetworkIDManager(networkIDManager);
	rakPeer->AttachPlugin(replicaManager3);
	
	// Automatically destroy connections, but don't create them so we have more control over when a system is considered ready to play
	replicaManager3->SetAutoManageConnections(false,true);
	replicaManager3->SetAutoSerializeInterval(30);
	replicaManager3->demo = demo;

	// Create and register the network object that represents the player
	// Hook RakNet stuff into this class
	if (topology == SERVER) {
		playerBotReplica = new PlayerBotReplica;
		playerBotReplica->demo = demo;
		playerBotReplica->CreateBotModel();
		playerBotReplica->gamePlatform = demo->gamePlatform;
	}
	else {
		playerReplica = new PlayerReplica;
		playerReplica->demo = demo;
		playerReplica->gamePlatform = demo->gamePlatform;
	}

	//Draw debug 
	//scene::ISceneManager* smgr = demo->GetSceneManager();
	//collisionBoxQueue = new CollisionBoxQueueSceneNode(smgr->getRootSceneNode(), smgr);
	//collisionBoxQueue->demo = demo;

	if (topology == CLIENT) {
#if __ANDROID__
		//ConnectionAttemptResult car = rakPeer->Connect("10.0.2.2", SERVER_PORT, 0, 0); // 안드로이드 에뮬레이터의 "127.0.0.1" 주소
		ConnectionAttemptResult car = rakPeer->Connect("192.168.1.2", SERVER_PORT, 0, 0); //랜
		//ConnectionAttemptResult car = rakPeer->Connect("192.168.0.17", SERVER_PORT, 0, 0); //랜
#else
		ConnectionAttemptResult car = rakPeer->Connect("127.0.0.1", SERVER_PORT, 0, 0); //로컬
		//ConnectionAttemptResult car = rakPeer->Connect("192.168.1.2", SERVER_PORT, 0, 0); //랜
		//ConnectionAttemptResult car = rakPeer->Connect("192.168.0.17", SERVER_PORT, 0, 0); //랜
		
#endif // __ANDROID__
		//RakAssert(car == CONNECTION_ATTEMPT_STARTED);

		//loggerPlugin = PacketLogger::GetInstance();
		//rakPeer->AttachPlugin(loggerPlugin);
	}
	else if (topology == SERVER) {
		
		//statisticsPlugin = StatisticsHistoryPlugin::GetInstance();
		//statisticsPlugin->SetTrackConnections(true, 0, true);
		//rakPeer->AttachPlugin(statisticsPlugin);

		//loggerPlugin = PacketLogger::GetInstance();
		//rakPeer->AttachPlugin(loggerPlugin);
	}
}
void DeinitializeRakNetClasses(bool isLogged, const char* baseDir)
{
	DataStructures::List<Replica3*> replicaListOut;
	replicaManager3->GetReplicasCreatedByMe(replicaListOut);
	replicaManager3->BroadcastDestructionList(replicaListOut, RakNet::UNASSIGNED_SYSTEM_ADDRESS);
	
	if (isLogged) {
		SaveStatisticsToCSV(baseDir);
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

	if (topology == SERVER) {
		playerBotReplica->PreDestruction(0);
		delete playerBotReplica;
		//delete statisticsPlugin;
		//delete loggerPlugin;
	}
	else if (topology == CLIENT) {
		playerReplica->PreDestruction(0);
		delete playerReplica;
		//delete loggerPlugin;
	}

	PrintOneLineNewline();
}

void PrintStatistics(bool isExportFile)
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

		// 현재 등록된 통계 키 가져오기
	/*	DataStructures::List<RakString> keys;
		statisticsPlugin->statistics.GetUniqueKeyList(keys);*/

		Time curTime = RakNet::GetTime();

		unsigned int ms = curTime % 1000;
		unsigned int totalSeconds = curTime / 1000;
		unsigned int seconds = totalSeconds % 60;
		unsigned int minutes = (totalSeconds / 60) % 60;
		unsigned int hours = (totalSeconds / 3600) % 24;  // 하루 기준

		char buffer[160];
		snprintf(buffer, sizeof(buffer), "%02u:%02u:%02u:%03u,%s", hours, minutes, seconds, ms, ipStr);

		statBuf.Push(RakNet::RakString(buffer), _FILE_AND_LINE_); //Log

		//for (unsigned int k = 0; k < keys.Size(); ++k)
		//{
		//	//가져올 값
		//	StatisticsHistory::TimeAndValueQueue* history; 
		//	if (statisticsPlugin->statistics.GetHistoryForKey(objectId, keys[k], &history, curTime) != StatisticsHistory::SH_OK)
		//		continue;

		//	//현재 해당하는 키에 대한 초당 변화량
		//	if (history->values.Size() > 0)
		//	{
		//		StatisticsHistory::TimeAndValue latest = history->values.PeekTail();
		//		
		//		//log format
		//		// ip / timeStemp / key / log property / value
		//		
		//		unsigned int ms = latest.time % 1000;
		//		unsigned int totalSeconds = latest.time / 1000;
		//		unsigned int seconds = totalSeconds % 60;
		//		unsigned int minutes = (totalSeconds / 60) % 60;
		//		unsigned int hours = (totalSeconds / 3600) % 24;  // 하루 기준

		//		char buffer[64];
		//		snprintf(buffer, sizeof(buffer), "%02u:%02u:%02u:%03u", hours, minutes, seconds, ms);

		//		if(isExportFile){
		//			// 마지막 누적값 및 평균 저장
		//			char buffer2[4][160];
		//			snprintf(buffer2[0], sizeof(buffer2[0]), "%s,%s,%s,CumulativeSum,%.2f\n", ipStr, buffer, keys[k].C_String(), history->GetLongTermSum());
		//			snprintf(buffer2[1], sizeof(buffer2[1]), "%s,%s,%s,CumulativeAvg,%.2f\n", ipStr, buffer, keys[k].C_String(), history->GetLongTermAverage());
		//			snprintf(buffer2[2], sizeof(buffer2[2]), "%s,%s,%s,Highest,%.2f\n", ipStr, buffer, keys[k].C_String(), history->GetLongTermHighest());
		//			snprintf(buffer2[3], sizeof(buffer2[3]), "%s,%s,%s,Lowest,%.2f\n", ipStr, buffer, keys[k].C_String(), history->GetLongTermLowest());

		//			for (int i = 0; i < 4; i++) statBuf.Push(RakNet::RakString(buffer2[i]), _FILE_AND_LINE_); //Log
		//		}
		//		else {
		//			// 초당 변화량 저장
		//			char buffer2[160];
		//			snprintf(buffer2, sizeof(buffer2), "%s,%s,%s,perSecond,%.2f\n", ipStr, buffer, keys[k].C_String(), latest.val); // 또는 "\r\n" 사용 가능

		//			statBuf.Push(RakNet::RakString(buffer2), _FILE_AND_LINE_); //Log
		//		}

		//	    // OutputDebugStringA(buffer2); //Debugger
		//	}
		//}
		
	}
}

void PrintStatistics(char* ipStr, PrintStatics id, int num)
{
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

	statBuf.Push(RakNet::RakString(buffer), _FILE_AND_LINE_); //Log
}

void SaveStatisticsToCSV(const char* baseDir)
{
#ifdef _WIN32
	//디렉토리 경로 파악
	char buffer[MAX_PATH];
	DWORD length = GetCurrentDirectoryA(MAX_PATH, buffer);
	if (length > 0) {
		printf("현재 디렉토리: %s", buffer);
	}
	else {
		printf("디렉토리 경로를 가져올 수 없습니다.");
	}
#endif // _WIN32

	unsigned short connectionCount = rakPeer->NumberOfConnections();
	RakNet::SystemAddress systems[256];
	rakPeer->GetConnectionList(systems, &connectionCount);

#ifdef __linux__
	// 현재 시간
	time_t now = time(nullptr);
	struct tm* t = localtime(&now);

	// 타임스탬프 문자열 생성
	char timeStr[64];
	strftime(timeStr, sizeof(timeStr), "%Y%m%d_%H%M%S", t);

#ifdef __ANDROID__
	// 하위 폴더명 (원하는 폴더명)
	const char* subDir = "/stats";

	// 디렉토리 경로 생성
	char outputDir[512];
	snprintf(outputDir, sizeof(outputDir), "%s%s", baseDir, subDir);
#else
	// 절대 디렉토리
	const char* outputDir = "/home/parts/stats";
#endif // __ANDROID__

	mkdir(outputDir, 0777);  // 이미 있으면 실패하지만 무시됨
	//fuck you
	// 경로 + 파일명 조합
	char fullpath[512];
	snprintf(fullpath, sizeof(fullpath), "%s/full_stats_%s.csv", outputDir, timeStr);

	// 파일 열기
	FILE* f = fopen(fullpath, "w");
	if (!f) {
		perror("파일 열기 실패");
		return;
	}
#else
	// 파일명 + 경로
	char filename[256];
	time_t now = time(nullptr);
	strftime(filename, sizeof(filename), "full_stats_%Y%m%d_%H%M%S.csv", localtime(&now));

	FILE* f = fopen(filename, "w");
	if (!f) return;
#endif // __linux__

	//log format
	// ip / timeStemp / key / log property / value
	//fprintf(f, "Ip, TimeStemp, Key, Log Property, Value\n");

	//마지막 누적값 및 평균 저장
	//PrintStatistics(true);
	if (statBuf.Size() == 0) {
		fprintf(f, "%s", "no data");
	}
	else {
		for (unsigned int i = 0; i < statBuf.Size(); i++)
		{
			fprintf(f, "%s", statBuf[i].C_String());
		}
	}
	fclose(f);
	statBuf.Clear(false, _FILE_AND_LINE_);
	
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
	lastUpdate=RakNet::GetTimeMS();
	isDead = false;
	wasDead = false;
	isBot = false;
	playerList.Push(this,_FILE_AND_LINE_);
	fq = new DataStructures::Queue<FrameState>();
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
}
bool PlayerReplica::DeserializeConstruction(RakNet::BitStream *constructionBitstream, RakNet::Connection_RM3 *sourceConnection)
{
	if (!BaseIrrlichtReplica::DeserializeConstruction(constructionBitstream, sourceConnection))
		return false;
	constructionBitstream->Read(rotationAroundYAxis);
	constructionBitstream->Read(gamePlatform);
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
		if (wasDead && !IsDead()) {
			RakNet::BitStream bs;
			bs.Write((RakNet::MessageID)CDemo::ID_GAME_MESSAGE_PLAYER_LIFE);
			bs.Write(creatingSystemGUID);
			bs.Write(IsDead());
			wasDead = false;

			rakPeer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
		}
	}

	//record frame state
	if (topology == SERVER && isCreatedCamera)
	{
		if (fq == nullptr) return;
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
		if (gamePlatform == Holder) {
			//현재 Shoooter가 Holder에게 총을 쐈을 때 
			//Time Wrap가 적용되는 위치를 출력한다 (Shooter ~ Server 간 딜레이가 없다고 가정)
			RakNet::TimeMS now = curTime - INTERP_TIME_MS;
			core::vector3df printPos;
			for (int frameIdx = 1; frameIdx < fq->Size(); frameIdx++)
			{
				if ((*fq)[frameIdx].timeStamp >= now) {
					//now가 frameIdx-1 과 frameIdx 사이에 있으므로 보간
					const FrameState& f0 = (*fq)[frameIdx - 1];
					const FrameState& f1 = (*fq)[frameIdx];
					float alpha = float(now - f0.timeStamp) / float(f1.timeStamp - f0.timeStamp);

					//Shot Position 보간
					//= f0.shotPosition.getInterpolated(f1.shotPosition, alpha);
					printPos = f0.collisionTransform.getTranslation().getInterpolated(f1.collisionTransform.getTranslation(), alpha);
					break;
				}
			}
			PrintHoldPosOneLine(printPos.X, printPos.Y, printPos.Z);
		}
	}

	// Is a locally created object?
	// 이동 적용
	if (creatingSystemGUID==rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))
	{
		
		if (topology == SERVER) {
			playerBotReplica->position = demo->GetSceneManager()->getActiveCamera()->getPosition() - irr::core::vector3df(0, CAMERA_HEIGHT, 0);
			playerBotReplica->rotationAroundYAxis = demo->GetSceneManager()->getActiveCamera()->getRotation().Y - 90.0f;
			playerBotReplica->botModel->setPosition(playerBotReplica->position);
			playerBotReplica->botModel->setRotation(core::vector3df(0, playerBotReplica->rotationAroundYAxis, 0));

			/*if (playerBotReplica->position.X <= 530.00) demo->isKeyLock = true;*/
		}
		else {
			playerReplica->position = demo->GetSceneManager()->getActiveCamera()->getPosition() - irr::core::vector3df(0, CAMERA_HEIGHT, 0);
			playerReplica->rotationAroundYAxis = demo->GetSceneManager()->getActiveCamera()->getRotation().Y - 90.0f;

			//if (playerReplica->position.X <= 530.00) {
			//	demo->isKeyLock = true;
			//}
			
		}
		
		// Local player has no mesh to interpolate
		// Input our camera position as our player position
		isMoving=demo->IsMovementKeyDown();

		// Ack, makes the screen messed up and the mouse move off the window
		// Find another way to keep the dead player from moving
	    demo->EnableInput((topology == SERVER) ? IsDead() == false : isDead == false);

		//DebugPrintf("Player position : %f, %f, %f / isKeyLock : %d / wasKeyLock : %d \n", position.X, position.Y, position.Z, demo->isKeyLock, demo->wasKeyLock);
		// DebugPrintf("Player target : %f, %f, %f\n", demo->GetSceneManager()->getActiveCamera()->getTarget().X, demo->GetSceneManager()->getActiveCamera()->getTarget().Y, demo->GetSceneManager()->getActiveCamera()->getTarget().Z);
	
		if (demo->gamePlatform == Holder) {
			demo->SetHolderPosText(playerReplica->position);
		}
	}

	if (creatingSystemGUID == rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS)) return;


	//Debug Frame
	DrawDebugFrame(demo->GetSyndeyBoundingBox(),position, rotationAroundYAxis, demo->GetSceneManager(), creatingSystemGUID, 500);

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
	if (gamePlatform == Holder ) {
		if (demo->gamePlatform == Shooter) {
			//보간된 위치
			demo->SetHolderPosText(model->getPosition());
		}
	}

	//Set Animation
	if ((topology == SERVER) ? IsDead(): isDead)
	{
		UpdateAnimation(scene::EMAT_DEATH_FALLBACK);
		model->setLoopMode(false);		
	}
	else if (curAnim!=scene::EMAT_ATTACK)
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
	return QueryConstruction_ServerConstruction(destinationConnection, topology != CLIENT);
}
bool PlayerBotReplica::QueryRemoteConstruction(RakNet::Connection_RM3* sourceConnection) {
	return QueryRemoteConstruction_ServerConstruction(sourceConnection, topology != CLIENT);
}
RakNet::RM3QuerySerializationResult PlayerBotReplica::QuerySerialization(RakNet::Connection_RM3* destinationConnection) {
	return QuerySerialization_ServerSerializable(destinationConnection, topology != CLIENT);
}
RakNet::RM3ActionOnPopConnection PlayerBotReplica::QueryActionOnPopConnection(RakNet::Connection_RM3* droppedConnection) const { return QueryActionOnPopConnection_Server(droppedConnection); }

void PlayerBotReplica::WriteAllocationID(RakNet::Connection_RM3* destinationConnection, RakNet::BitStream* allocationIdBitstream) const
{
	allocationIdBitstream->Write(RakNet::RakString("PlayerBotReplica"));
}

RM3SerializationResult PlayerBotReplica::Serialize(RakNet::SerializeParameters* serializeParameters)
{
	BaseIrrlichtReplica::Serialize(serializeParameters);
	serializeParameters->outputBitstream[0].Write(position);
	serializeParameters->outputBitstream[0].Write(rotationAroundYAxis);
	serializeParameters->outputBitstream[0].Write(isMoving);

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
	constructionBitstream->Write(shooterName);
	constructionBitstream->Write(bulletCount);
	////TimeStamp
	//constructionBitstream->Write(GetCurrentTimeMS());
}
bool BallReplica::DeserializeConstruction(RakNet::BitStream *constructionBitstream, RakNet::Connection_RM3 *sourceConnection)
{
	if (!BaseIrrlichtReplica::DeserializeConstruction(constructionBitstream, sourceConnection))
		return false;
	constructionBitstream->Read(shotDirection);
	constructionBitstream->Read(shooterName);
	constructionBitstream->Read(bulletCount);
	//TimeStamp
	//long long timeStampMS;
	//constructionBitstream->Read(timeStampMS);

	if (creatingSystemGUID != rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))
	{
		RakNet::TimeMS estimatedElapsed = rakPeer->GetAveragePing(sourceConnection->GetSystemAddress()) / 2;
		position = position + shotDirection * (float)estimatedElapsed * SHOT_SPEED;
	}

	return true;
}

void BallReplica::PostSerializeConstruction(RakNet::BitStream* constructionBitstream, RakNet::Connection_RM3* destinationConnection)
{
	if (replicaManager3->isLog&&(creatingSystemGUID==destinationConnection->GetRakNetGUID())) {
		// Start
		char ipStr[64];
		destinationConnection->GetSystemAddress().ToString(false, ipStr);
		PrintStatistics(ipStr, ID_SERVER_NETWORK_SEND, bulletCount);
	}
}

void BallReplica::PostDeserializeConstruction(RakNet::BitStream *constructionBitstream, RakNet::Connection_RM3 *destinationConnection)
{
	//time check
	if (creatingSystemGUID == rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS)) {
		if (replicaManager3->isLog) {
		PrintStatistics(nullptr, ID_CLIENT_NETWORK_RECEIVE, bulletCount);
		demo->isBulletRendering = true;
		}
	}

	// Shot visible effect and BallReplica classes are not linked, but they update the same way, such that
	// they are in the same spot all the time
	if (topology != SERVER) {
		demo->shootFromOrigin(position, shotDirection);
		return;
	}

	unsigned int idx;
	scene::ISceneManager* sm = demo->GetSceneManager();
	scene::ICameraSceneNode* camera = sm->getActiveCamera();

	// Time Warp
	// 쏜 사람 shootPosition, shootDirection 확인
	// 나머지 사람 Transform 확인
	core::vector3df start;
	core::vector3df end; 
	core::line3d<irr::f32> line;
    core::triangle3df triangle;
    core::vector3df hitPoint;
    const scene::ISceneNode* hitNode;

	RakNet::TimeMS now = RakNet::GetTimeMS() - (rakPeer->GetAveragePing(creatingSystemGUID) / 2) - INTERP_TIME_MS - INGOING_TIME_MS; //Rewind Time = Server Current Time - RTT/2 - Client View Interpolation Time - Ingoing Delay Time
	bool wallHit = false;
	core::vector3df wallHitPoint;
	for (idx = 0; idx < PlayerReplica::playerList.Size(); ++idx)
	{
		auto* player = PlayerReplica::playerList[idx];
		auto* q = player->fq;
		bool didTimeWarp = false;

		//ball position은 클라에서 camPosition으로 설정한 값
		//time warp에서는 위 값을 쓰지 않고 매번 playerReplica를 통해서 camPosition을 받아와 그 값을 쓴다
		for (int frameIdx = 1; frameIdx < q->Size(); frameIdx++) {
			if ((*q)[frameIdx].timeStamp >= now) {
				//now가 frameIdx-1 과 frameIdx 사이에 있으므로 보간
				const FrameState& f0 = (*q)[frameIdx - 1];
				const FrameState& f1 = (*q)[frameIdx];
				float alpha = float(now - f0.timeStamp) / float(f1.timeStamp - f0.timeStamp);

				if (player->creatingSystemGUID == creatingSystemGUID) {
					//Shot Position 보간
					start = f0.shotPosition.getInterpolated(f1.shotPosition, alpha);
					//Shot Direction 보간 및 정규화
					core::vector3df interpolatedShotDir = f0.shotDirection.getInterpolated(f1.shotDirection, alpha);
					interpolatedShotDir.normalize();
					end = start + (interpolatedShotDir * camera->getFarValue());
					line.setLine(start, end);

					//벽 충돌 판정 및 비주얼 애니메이션
					demo->shootFromOrigin(start,interpolatedShotDir,start,end,wallHit,wallHitPoint);
				}
				else {
					//Collision Transform 보간
					for (int i = 0; i < 16; ++i)
						player->collisionTransform.pointer()[i] = (f0.collisionTransform.pointer()[i]) * (1.0f - alpha) + (f1.collisionTransform.pointer()[i]) * alpha;
				}
				didTimeWarp = true;
				break;
			}
		}

		// fallback 처리: 보간 실패 시 가장 최신 값 사용
		if (!didTimeWarp && q->Size() >= 1) {
			const FrameState& lastFrame = q->PeekTail();
			if (player->creatingSystemGUID == creatingSystemGUID) {
				start = lastFrame.shotPosition;
				core::vector3df dir = lastFrame.shotDirection;
				dir.normalize();
				end = start + dir * camera->getFarValue();
				line.setLine(start, end);

				//벽 충돌 판정 및 비주얼 애니메이션
				demo->shootFromOrigin(lastFrame.shotPosition, lastFrame.shotDirection, start, end, wallHit, wallHitPoint);
			}
			else {
				player->collisionTransform = lastFrame.collisionTransform;
			}
		}
	}

	
	RakNet::TimeMS debugTime = RakNet::GetTimeMS();
	for (idx = 0; idx < PlayerReplica::playerList.Size(); ++idx)
	{
		auto* player = PlayerReplica::playerList[idx];
		if (player->creatingSystemGUID == creatingSystemGUID) {
			player->PlayAttackAnimation();
			continue; // 총을 쏜 본인
		}
		
		scene::ITriangleSelector* selector = nullptr;
		scene::ISceneNode* node = nullptr;

		if (player->IsDead()) continue;
		selector = CreateSelectorFromTransformedBox(demo->GetSyndeyBoundingBox(), player->collisionTransform, sm, player->creatingSystemGUID);

		if (selector == nullptr) continue;
		//DrawDebugFrame(selector, 1000);


		// 충돌 검사
#ifdef __ANDROID__
		scene::SCollisionHit hitResult;
		bool hit = sm->getSceneCollisionManager()->getCollisionPoint(hitResult, line, selector);
		hitPoint = hitResult.Intersection;
		triangle = hitResult.Triangle;
		hitNode = hitResult.Node;
#else 
		bool hit = sm->getSceneCollisionManager()->getCollisionPoint(line, selector, hitPoint, triangle, hitNode);
#endif // __ANDROID__

		selector->drop(); 

		if (hit && hitNode && hitNode->getID() == static_cast<s32>(player->creatingSystemGUID.g))
		{
			if (wallHit) {
				//벽에 부딫히면 플레이어 맞음 처리하면 안됨
				float distToPlayer = line.start.getDistanceFrom(hitPoint);
				float distToWall = line.start.getDistanceFrom(wallHitPoint);
				//DebugPrintf("distToPlayer : %f, distToWall : %f\n", distToPlayer, distToWall);
				if (distToWall < distToPlayer) continue;
			}

			player->deathTimeout = RakNet::GetTimeMS() + 200;
			printf("HIT! : %d\n", ++score);
			RakNet::RakString msg("%s Dead from : %s",
				player->isBot ? "Bot" : "Player",
				rakPeer->GetSystemAddressFromGuid(creatingSystemGUID).ToString(true));
			demo->PushMessage(msg);
			//OutputDebugStringA(msg.C_String());
			
			RakNet::BitStream bs;
			bs.Write((RakNet::MessageID)CDemo::ID_GAME_MESSAGE_PLAYER_LIFE);
			bs.Write((creatingSystemGUID));       //Shooter 
			bs.Write(player->IsDead());
			player->wasDead = true;
			bs.Write(player->creatingSystemGUID); //Holder
			bs.Write(shooterName);                //Shooter Name
			bs.Write(player->playerName);         //Holder Name
			//TODO : 점수 득점도 포함하기

			KillLog logEntry{ shooterName + RakNet::RakString(" -> ") + player->playerName + RakNet::RakString("\n"), RakNet::GetTimeMS() };
			demo->killLogMessages.Push(logEntry, _FILE_AND_LINE_); // Record the kill log message

			rakPeer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
			break; // 더 검사하지 않음
		}
	}

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
	statBuf.Push(RakNet::RakString(str), _FILE_AND_LINE_); //Log
}

void DebugPrintf(const char* format, ...)
{
#ifndef __ANDROID__
	char buf[512];

	va_list args;
	va_start(args, format);
	vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

#ifdef _WIN32
	OutputDebugStringA(buf);
#endif // _WIN32

#endif // __ANDROID__	
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