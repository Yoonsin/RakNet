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
//#include <CSceneNodeAnimatorCameraFPS.h>
//#include <CCameraSceneNode.h>
#ifdef __linux__
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
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

Topology topology;
PacketLogger* loggerPlugin;
StatisticsHistoryPlugin* statisticsPlugin; // Used to track network statistics


/*
class DebugBoxSceneNode : public scene::ISceneNode 
{
public:
	DebugBoxSceneNode(scene::ISceneNode* parent,
		scene::ISceneManager* mgr,
		s32 id = -1);
	virtual const core::aabbox3d<f32>& getBoundingBox() const;
	virtual void OnRegisterSceneNode();
	virtual void render();

	CDemo *demo;
};
DebugBoxSceneNode::DebugBoxSceneNode(
									 scene::ISceneNode* parent,
									 scene::ISceneManager* mgr,
									 s32 id)
									 : scene::ISceneNode(parent, mgr, id)
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
void DebugBoxSceneNode::render()
{
	if (DebugDataVisible)
	{ 
		video::IVideoDriver* driver = SceneManager->getVideoDriver();
		driver->setTransform(video::ETS_WORLD, AbsoluteTransformation); 

		video::SMaterial m;
		m.Lighting = false;
		demo->GetDevice()->getVideoDriver()->setMaterial(m);
		demo->GetDevice()->getVideoDriver()->draw3DBox(demo->GetSyndeyBoundingBox());
	}
}
*/

DataStructures::List<RakNet::RakString> statBuf;
DataStructures::List<PlayerReplica*> PlayerReplica::playerList;

// Take this many milliseconds to move the visible position to the real position
static const float INTERP_TIME_MS=100.0f;

void InstantiateRakNetClasses(bool isServer, bool isLogged)
{
	if (isServer) topology = SERVER;
	else topology = CLIENT;

	static const int MAX_PLAYERS=32;
	static const unsigned short TCP_PORT=0;
	static const RakNet::TimeMS UDP_SLEEP_TIMER=30;

	// Basis of all UDP communications
	rakPeer=RakNet::RakPeerInterface::GetInstance();
	
	// Using fixed port so we can use AdvertiseSystem and connect on the LAN if the server is not available.
	RakNet::SocketDescriptor sd((topology==SERVER)? SERVER_PORT : 1234, 0);
	sd.socketFamily = AF_INET; // Only IPV4 supports broadcast on 255.255.255.255
	
#ifdef __ANDROID__
	if (topology == CLIENT) {
		while (IRNS2_Berkley::IsPortInUse(sd.port, sd.hostAddress, sd.socketFamily, SOCK_DGRAM) == true)
			sd.port++;
	}
#else
	if (topology == CLIENT) {
		while (IRNS2_Berkley::IsPortInUse(sd.port, sd.hostAddress, sd.socketFamily, SOCK_DGRAM) == true)
			sd.port++;
	}
#endif // __ANDROID__

	// +1 is for the connection to the NAT punchthrough server
	RakNet::StartupResult sr;
	if (topology == SERVER) {
		sr = rakPeer->Startup(MAX_PLAYERS + 1, &sd, 1);
		rakPeer->SetMaximumIncomingConnections(MAX_PLAYERS);
		// Fast disconnect for easier testing of host migration
		//rakPeer->SetTimeoutTime(5000, UNASSIGNED_SYSTEM_ADDRESS);
	}
	else sr = rakPeer->Startup(1, &sd, 1);
	
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
	
	// Create and register the network object that represents the player
	playerReplica = new PlayerReplica;
	//replicaManager3->Reference(playerReplica);
	
	if (topology == CLIENT) {
#if __ANDROID__
		//ConnectionAttemptResult car = rakPeer->Connect("10.0.2.2", SERVER_PORT, 0, 0); // 안드로이드 에뮬레이터의 "127.0.0.1" 주소
		ConnectionAttemptResult car = rakPeer->Connect("192.168.1.2", SERVER_PORT, 0, 0); //랜
#else
		//ConnectionAttemptResult car = rakPeer->Connect("127.0.0.1", SERVER_PORT, 0, 0); //로컬
		ConnectionAttemptResult car = rakPeer->Connect("192.168.1.2", SERVER_PORT, 0, 0); //랜
		
#endif // __ANDROID__
		RakAssert(car == CONNECTION_ATTEMPT_STARTED);

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
	
	// Shutdown so the server knows we stopped
	rakPeer->Shutdown(100,0);

	RakNet::RakPeerInterface::DestroyInstance(rakPeer);
	delete networkIDManager;
	delete replicaManager3;
	delete natPunchthroughClient;
	delete cloudClient;
	delete fullyConnectedMesh2;
	// ReplicaManager3 deletes all referenced objects, including this one
	playerReplica->PreDestruction(0);
	delete playerReplica;
	
	if (topology == SERVER) {
		//delete statisticsPlugin;
		//delete loggerPlugin;
	}
	else if (topology == CLIENT) {
		//delete loggerPlugin;
	}
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

void PrintStatistics(char* ipStr, bool isStart, int num)
{
	Time curTime = RakNet::GetTime();

	unsigned int ms = curTime % 1000;
	unsigned int totalSeconds = curTime / 1000;
	unsigned int seconds = totalSeconds % 60;
	unsigned int minutes = (totalSeconds / 60) % 60;
	unsigned int hours = (totalSeconds / 3600) % 24;  // 하루 기준

	char buffer[160];
	snprintf(buffer, sizeof(buffer), "%s:%d/%02u:%02u:%02u:%03u\n",(isStart? "Start" : "End"), num,hours, minutes, seconds, ms);

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
	playerList.Push(this,_FILE_AND_LINE_);
}
PlayerReplica::~PlayerReplica()
{
	unsigned int index = playerList.GetIndexOf(this);
	if (index != (unsigned int) -1)
		playerList.RemoveAtIndexFast(index);
}
RakNet::RM3ConstructionState PlayerReplica::QueryConstruction(RakNet::Connection_RM3* destinationConnection, RakNet::ReplicaManager3* replicaManager3) { 
	return QueryConstruction_ClientConstruction(destinationConnection, topology != CLIENT);

}
bool PlayerReplica::QueryRemoteConstruction(RakNet::Connection_RM3* sourceConnection) { 
	return QueryRemoteConstruction_ClientConstruction(sourceConnection, topology != CLIENT); 
}
RakNet::RM3QuerySerializationResult PlayerReplica::QuerySerialization(RakNet::Connection_RM3* destinationConnection) { return QuerySerialization_ClientSerializable(destinationConnection, topology != CLIENT); }
RakNet::RM3ActionOnPopConnection PlayerReplica::QueryActionOnPopConnection(RakNet::Connection_RM3* droppedConnection) const { return QueryActionOnPopConnection_Client(droppedConnection); }

void PlayerReplica::WriteAllocationID(RakNet::Connection_RM3 *destinationConnection, RakNet::BitStream *allocationIdBitstream) const
{
	allocationIdBitstream->Write(RakNet::RakString("PlayerReplica"));
}
void PlayerReplica::SerializeConstruction(RakNet::BitStream *constructionBitstream, RakNet::Connection_RM3 *destinationConnection)
{
	BaseIrrlichtReplica::SerializeConstruction(constructionBitstream, destinationConnection);
	constructionBitstream->Write(rotationAroundYAxis);
	constructionBitstream->Write(playerName);
	constructionBitstream->Write(IsDead());
}
bool PlayerReplica::DeserializeConstruction(RakNet::BitStream *constructionBitstream, RakNet::Connection_RM3 *sourceConnection)
{
	if (!BaseIrrlichtReplica::DeserializeConstruction(constructionBitstream, sourceConnection))
		return false;
	constructionBitstream->Read(rotationAroundYAxis);
	constructionBitstream->Read(playerName);
	constructionBitstream->Read(isDead);
	demo->PushMessage(RakNet::RakString("Deserialize Construction"));
	return true;
}
void PlayerReplica::PostDeserializeConstruction(RakNet::BitStream *constructionBitstream, RakNet::Connection_RM3 *destinationConnection)
{
	// Object was remotely created and all data loaded. Now we can make the object visible
	scene::IAnimatedMesh* mesh = 0;
	scene::ISceneManager *sm = demo->GetSceneManager();
	mesh = sm->getMesh(IRRLICHT_MEDIA_PATH "sydney.md2");
	model = sm->addAnimatedMeshSceneNode(mesh, 0);

//	DebugBoxSceneNode * debugBox = new DebugBoxSceneNode(model,sm);
//	debugBox->demo=demo;
//	debugBox->setDebugDataVisible(true); 

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
	serializeParameters->outputBitstream[0].Write( topology==CLIENT ? IsDead() : isDead);

	//irr::scene::CSceneNodeAnimatorCameraFPS* cam = (irr::scene::CSceneNodeAnimatorCameraFPS*)demo->fpsCamAnim;
	//irr::scene::CCameraSceneNode* camNode = (irr::scene::CCameraSceneNode*)demo->GetSceneManager()->getActiveCamera();

	//replicatedCameraPos = camNode? demo->GetSceneManager()->getActiveCamera()->getAbsolutePosition() : irr::core::vector3df(); //cam ? cam->replicationPos : irr::core::vector3df();
	//replicatedCameraRot = cam ? cam->replicationRot : irr::core::vector3df();
	//
	//serializeParameters->outputBitstream[0].Write(replicatedCameraPos);
	//serializeParameters->outputBitstream[0].Write(replicatedCameraRot);
	
	//if (cam) {
	//	//visual studio debugger
	//	char buffer[120];
	//	snprintf(buffer, sizeof(buffer), "x: %f / y: %f / z: %f\n", cam->replicationPos.X, cam->replicationPos.Y, cam->replicationPos.Z); // 또는 "\r\n" 사용 가능
	//	OutputDebugStringA(buffer);
	//}
	

	//timeStamp
	serializeParameters->messageTimestamp = RakNet::GetTimeMS();
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
	bool wasDead=isDead;
	deserializeParameters->serializationBitstream[0].Read(isDead);

	//core::vector3df tmpPos;
	//core::vector3df tmpRot;

	//deserializeParameters->serializationBitstream[0].Read(tmpPos);
	//deserializeParameters->serializationBitstream[0].Read(tmpRot);

	if (isDead==true && wasDead==false)
	{
		demo->PlayDeathSound(position);
	}

	// Is a locally created object?
	if (creatingSystemGUID == rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))
	{
		/*irr::scene::CCameraSceneNode* cam = (irr::scene::CCameraSceneNode*)demo->GetSceneManager()->getActiveCamera();
		if (!cam) return;

		cam->replicationPos = tmpPos;
		cam->replicationRot = tmpRot;*/

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
void PlayerReplica::Update(RakNet::TimeMS curTime)
{
	// Is a locally created object?
	if (creatingSystemGUID==rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))
	{
		playerReplica->position = demo->GetSceneManager()->getActiveCamera()->getPosition() - irr::core::vector3df(0, CAMERA_HEIGHT, 0);
		playerReplica->rotationAroundYAxis = demo->GetSceneManager()->getActiveCamera()->getRotation().Y - 90.0f;
	
		// Local player has no mesh to interpolate
		// Input our camera position as our player position
		isMoving=demo->IsMovementKeyDown();

		// Ack, makes the screen messed up and the mouse move off the window
		// Find another way to keep the dead player from moving
	    demo->EnableInput(IsDead()==false);

		return;
	}

	// 원격에서는 보간
	// Update interpolation
	RakNet::TimeMS elapsed = curTime-lastUpdate;
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

	if (isDead)
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
	if (isDead==false)
	{
		UpdateAnimation(scene::EMAT_ATTACK);
		if(model)model->setLoopMode(false);		
	}
}
bool PlayerReplica::IsDead(void) const
{
	return deathTimeout > RakNet::GetTimeMS();
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
	constructionBitstream->Write(ownerGUID);
	constructionBitstream->Write(bulletCount);
	////TimeStamp
	//constructionBitstream->Write(GetCurrentTimeMS());
}
bool BallReplica::DeserializeConstruction(RakNet::BitStream *constructionBitstream, RakNet::Connection_RM3 *sourceConnection)
{
	if (!BaseIrrlichtReplica::DeserializeConstruction(constructionBitstream, sourceConnection))
		return false;
	constructionBitstream->Read(shotDirection);
	constructionBitstream->Read(ownerGUID);
	constructionBitstream->Read(bulletCount);
	//TimeStamp
	//long long timeStampMS;
	//constructionBitstream->Read(timeStampMS);

//	//visual studio debugger
//	char buffer[1024];
//	snprintf(buffer, sizeof(buffer), "%lld\n", timeStampMS);
//#ifdef _WIN32
//OutputDebugStringA(buffer);
//#endif

	return true;
}
void BallReplica::PostDeserializeConstruction(RakNet::BitStream *constructionBitstream, RakNet::Connection_RM3 *destinationConnection)
{
	//time check
	if (ownerGUID == rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS)&& replicaManager3->isLog) {
		PrintStatistics(nullptr, false, bulletCount);
	}
	
	// Shot visible effect and BallReplica classes are not linked, but they update the same way, such that
	// they are in the same spot all the time
	demo->shootFromOrigin(position, shotDirection);

	// Find the owner of this ball, and make them play the attack animation
	unsigned int idx;
	for (idx=0; idx < PlayerReplica::playerList.Size(); idx++)
	{
		if (PlayerReplica::playerList[idx]->creatingSystemGUID==ownerGUID)
		{
			PlayerReplica::playerList[idx]->PlayAttackAnimation();
			break;
		}
	}
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

	// Keep at the same position as the visible effect
	// Deterministic, so no need to actually transmit position
	// The variable position is the origin that the ball was created at. For the player, it is their actual position
	RakNet::TimeMS elapsedTime;
	// Due to ping variances and timestamp miscalculations, it's possible with very low pings to get a slightly negative time, so we have to check
	if (curTime>=creationTime)
		elapsedTime = curTime - creationTime;
	else
		elapsedTime=0;
	irr::core::vector3df updatedPosition = position + shotDirection * (float) elapsedTime * SHOT_SPEED;

	// See if the bullet hit us
	//if (ownerGUID/*creatingSystemGUID*/ != rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))
	//{
	//	if (playerReplica->IsDead()==false)
	//	{
	//		float playerHalfHeight=demo->GetSyndeyBoundingBox().getExtent().Y/2;
	//		irr::core::vector3df positionRelativeToCharacter = updatedPosition-playerReplica->position;//+core::vector3df(0,playerHalfHeight,0);
	//		if (demo->GetSyndeyBoundingBox().isPointInside(positionRelativeToCharacter))
	//		//if ((playerReplica->position+core::vector3df(0,playerHalfHeight,0)-updatedPosition).getLengthSQ() < BALL_DIAMETER*BALL_DIAMETER/4.0f)
	//		{
	//			// We're dead for 3 seconds
	//			playerReplica->deathTimeout=curTime+3000;
	//		}
	//	}
	//}
}
RakNet::Replica3 *Connection_RM3Irrlicht::AllocReplica(RakNet::BitStream *allocationId, ReplicaManager3 *replicaManager3)
{
	RakNet::RakString typeName; allocationId->Read(typeName);
	if (typeName=="PlayerReplica") {BaseIrrlichtReplica *r = new PlayerReplica; r->demo=demo; return r;}
	if (typeName=="BallReplica") {BaseIrrlichtReplica *r = new BallReplica; r->demo=demo; return r;}
	return 0;
}

void ReplicaManager3Irrlicht::PrintTimeGap(char* str) {
	
	statBuf.Push(RakNet::RakString(str), _FILE_AND_LINE_); //Log
}