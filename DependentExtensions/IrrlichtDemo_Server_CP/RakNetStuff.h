/*
 *  Copyright (c) 2014, Oculus VR, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant 
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

// I tried to put most of the RakNet stuff here, but some of it had to go to CDemo.h too

#ifndef __RAKNET_ADDITIONS_FOR_IRRLICHT_DEMO_H
#define __RAKNET_ADDITIONS_FOR_IRRLICHT_DEMO_H

#include "RakPeerInterface.h"
#include "RakPeer.h"
#include "ReplicaManager3.h"
#include "NatPunchthroughClient.h"
#include "CloudClient.h"
#include "FullyConnectedMesh2.h"
#include "UDPProxyClient.h"
#include "TCPInterface.h"
#include "HTTPConnection.h"
#include "RakNetStatistics.h"
#include "StatisticsHistory.h"
//#include "../Samples/PHPDirectoryServer2/PHPDirectoryServer2.h"
#include "vector3d.h"
#include "IAnimatedMeshSceneNode.h"
#include "MessageIdentifiers.h"
#include "DS_Multilist.h"
#include "DS_OrderedList.h"
#include "DS_Heap.h"
#include "DS_Map.h"
#include "GetTime.h"
#include <vector>
#include <deque>

using namespace std;
using namespace irr;

class ReplicaManager3Irrlicht;
class CDemo;
class PlayerReplica;
class PlayerBotReplica;
class DebugBoxSceneNode;
class CollisionBoxQueueSceneNode;

enum Topology
{
	CLIENT,
	SERVER
};

enum GamePlatform {
	Shooter,
	Holder,
	Server
};

enum PrintStatics {
	ID_CLIENT_POLLING_END = 0,
	ID_CLIENT_NETWORK_SEND = 1,
	ID_SERVER_NETWORK_RECEIVE = 2,
	ID_SERVER_NETWORK_SEND = 3,
	ID_CLIENT_NETWORK_RECEIVE = 4,
	ID_CLIENT_RENDERING_START = 5,
};

enum PriorityStatics {
	PRIORITY_LOW = 0,
	PRIORITY_MEDIUM = 1,
	PRIORITY_HIGH = 2,
	PRIORITY_CRITICAL = 3,
	PRIORITY_MAX = 4
};

struct FrameState {
	RakNet::TimeMS timeStamp;
	irr::core::matrix4 collisionTransform;
	irr::core::vector3df shotPosition;
	irr::core::vector3df shotDirection;
};

struct CollDebugState {
	RakNet::TimeMS timeStamp;
	std::vector<irr::core::triangle3df> tris;
	irr::s32 outCount;
	irr::core::line3d<irr::f32> line;
	RakNet::TimeMS drawTimeOut;
};

struct orderData {
	int um_cnt;
	int am_cnt;
	int processTime;
	RakNet::TimeMS reactionTime;
	RakNet::TimeMS ingoingTime;
	RakNet::TimeMS timeOut;
	RakNet::RakNetGUID playerGUID;
	bool isSequenced;
};

// All externs defined in the corresponding CPP file
// Most of these classes has a manual entry, all of them have a demo
extern RakNet::RakPeerInterface *rakPeer; // Basic communication
extern RakNet::NetworkIDManager *networkIDManager; // Unique IDs per network object
extern ReplicaManager3Irrlicht *replicaManager3; // Autoreplicate network objects
extern RakNet::NatPunchthroughClient *natPunchthroughClient; // Connect peer to peer through routers
extern RakNet::CloudClient *cloudClient; // Used to upload game instance to the cloud
extern RakNet::FullyConnectedMesh2 *fullyConnectedMesh2; // Used to find out who is the session host
extern PlayerReplica* playerReplica; // Network object that represents the player
extern PlayerBotReplica* playerBotReplica; // Network object that represents the player
extern CollisionBoxQueueSceneNode* collisionBoxQueue;
extern DataStructures::List<DataStructures::List<RakNet::RakString>> statBufList;

// A NAT punchthrough and proxy server Jenkins Software is hosting for free, should usually be online
#define DEFAULT_NAT_PUNCHTHROUGH_FACILITATOR_PORT 61111
#define DEFAULT_NAT_PUNCHTHROUGH_FACILITATOR_IP "natpunch.slikesoft.com" //"natpunch.jenkinssoftware.com" 대체
#define SERVER_IP "192.168.1.2" 
#define SERVER_IP_LOCAL "127.0.0.1"
#define SERVER_PORT 20123

void InstantiateRakNetClasses(bool isServer, bool isLogged, bool isBot, CDemo* demo);
void DeinitializeRakNetClasses(bool isLogged, bool isBot, const char* baseDir, int evalMask);
void SaveStatisticsToCSV(const char* baseDir, int methodNum);

//시간 변화량
void PrintStatistics(bool isExportFile,int methodNum);

//서버에서 온 패킷 시간 간격
void PrintStatistics(char* ipStr, PrintStatics id,int num);

long long GetCurrentTimeMS();
//RakString FormatTime(long long milliseconds);

void DrawBoxTriangles(irr::scene::ITriangleSelector* selector, const irr::core::matrix4& transform, irr::video::IVideoDriver* driver);
void DebugPrintf(const char* format, ...);

void DrawDebugFrame(const irr::core::aabbox3df& boundingBox, irr::core::vector3df position, float rotationAroundYAxis, irr::scene::ISceneManager* sm, RakNet::RakNetGUID g, RakNet::TimeMS drawTimeOut);
void DrawDebugFrame(irr::scene::ITriangleSelector* selector, RakNet::TimeMS drawTimeOut, bool isDrop = false);

static inline void PrintHoldPosOneLine(float x, float y, float z);
static inline void PrintOneLineNewline(void);
static const float INTERP_TIME_MS = 100.0f;

irr::scene::ITriangleSelector* CreateSelectorFromTransformedBox(const core::aabbox3df& localBox, const core::matrix4& worldTransform, scene::ISceneManager* smgr, const RakNet::RakNetGUID& guid);

// Base RakNet custom classes for Replica Manager 3, setup peer to peer networking
class BaseIrrlichtReplica : public RakNet::Replica3
{
public:
	BaseIrrlichtReplica();
	virtual ~BaseIrrlichtReplica();

	virtual RakNet::RM3ConstructionState QueryConstruction(RakNet::Connection_RM3 *destinationConnection, RakNet::ReplicaManager3 *replicaManager3) {return QueryConstruction_PeerToPeer(destinationConnection);}
	virtual bool QueryRemoteConstruction(RakNet::Connection_RM3 *sourceConnection) {return QueryRemoteConstruction_PeerToPeer(sourceConnection);}
	virtual RakNet::RM3ActionOnPopConnection QueryActionOnPopConnection(RakNet::Connection_RM3* droppedConnection) const { return QueryActionOnPopConnection_PeerToPeer(droppedConnection); }
	virtual RakNet::RM3QuerySerializationResult QuerySerialization(RakNet::Connection_RM3* destinationConnection) { return QuerySerialization_PeerToPeer(destinationConnection); }
	
	virtual void DeallocReplica(RakNet::Connection_RM3 *sourceConnection) {delete this;}
	virtual void SerializeConstruction(RakNet::BitStream *constructionBitstream, RakNet::Connection_RM3 *destinationConnection);
	virtual bool DeserializeConstruction(RakNet::BitStream *constructionBitstream, RakNet::Connection_RM3 *sourceConnection);
	virtual RakNet::RM3SerializationResult Serialize(RakNet::SerializeParameters *serializeParameters);
	virtual void Deserialize(RakNet::DeserializeParameters *deserializeParameters);
	virtual void SerializeDestruction(RakNet::BitStream *destructionBitstream, RakNet::Connection_RM3 *destinationConnection) {}
	virtual bool DeserializeDestruction(RakNet::BitStream *destructionBitstream, RakNet::Connection_RM3 *sourceConnection) {return true;}
	

	virtual PriorityStatics GetPriorityStatics(void) const { return PRIORITY_MEDIUM; }

	/// This function is not derived from Replica3, it's specific to this appss
	/// Called from CDemo::UpdateRakNet
	virtual void Update(RakNet::TimeMS curTime);

	// Set when the object is constructed
	CDemo *demo;
	// real is written on the owner peer, read on the remote peer
	irr::core::vector3df position;
	RakNet::TimeMS creationTime;
};
// Game classes automatically updated by ReplicaManager3
class PlayerReplica : public BaseIrrlichtReplica, public irr::scene::IAnimationEndCallBack
{
public:
	PlayerReplica();
	virtual ~PlayerReplica();
	// Every function below, before Update overriding a function in Replica3
	virtual void WriteAllocationID(RakNet::Connection_RM3 *destinationConnection, RakNet::BitStream *allocationIdBitstream) const;
	virtual void SerializeConstruction(RakNet::BitStream *constructionBitstream, RakNet::Connection_RM3 *destinationConnection);
	virtual bool DeserializeConstruction(RakNet::BitStream *constructionBitstream, RakNet::Connection_RM3 *sourceConnection);
	virtual RakNet::RM3SerializationResult Serialize(RakNet::SerializeParameters *serializeParameters);
	virtual void Deserialize(RakNet::DeserializeParameters *deserializeParameters);
	virtual void PostDeserializeConstruction(RakNet::BitStream *constructionBitstream, RakNet::Connection_RM3 *destinationConnection);
	virtual void PreDestruction(RakNet::Connection_RM3 *sourceConnection);

	virtual RakNet::RM3ConstructionState QueryConstruction(RakNet::Connection_RM3* destinationConnection, RakNet::ReplicaManager3* replicaManager3);
	virtual bool QueryRemoteConstruction(RakNet::Connection_RM3* sourceConnection);
	virtual RakNet::RM3QuerySerializationResult QuerySerialization(RakNet::Connection_RM3* destinationConnection);
	virtual RakNet::RM3ActionOnPopConnection QueryActionOnPopConnection(RakNet::Connection_RM3* droppedConnection) const;

	virtual void Update(RakNet::TimeMS curTime);
	void UpdateAnimation(irr::scene::EMD2_ANIMATION_TYPE anim);
	float GetRotationDifference(float r1, float r2);
	virtual void OnAnimationEnd(irr::scene::IAnimatedMeshSceneNode* node);
	void PlayAttackAnimation(void);

	// playerName is only sent in SerializeConstruction, since it doesn't change
	RakNet::RakString playerName;

	// Networked rotation
	float rotationAroundYAxis;
	// Interpolation variables, not networked
	irr::core::vector3df positionDeltaPerMS;
	float rotationDeltaPerMS;
	RakNet::TimeMS interpEndTime, lastUpdate, bulletCoolTime;

	// Updated based on the keypresses, to control remote animation
	bool isMoving;

	// Only instantiated for remote systems, you never see yourself
	irr::scene::IAnimatedMeshSceneNode* model;
	irr::scene::EMD2_ANIMATION_TYPE curAnim;

	// deathTimeout is set from the Server, and is used to determine if the player is dead
	RakNet::TimeMS deathTimeout;
	bool IsDead(void) const;
	// isDead is set from network packets for remote players
	bool isDead;
	// wasDead is set from the Server, and is used to determine if the player was dead before
	bool wasDead;
	bool isBot;

	// List of all players, including our own
	static DataStructures::List<PlayerReplica*> playerList;
	// for Time Warp
	DataStructures::Queue<FrameState>* fq;

	bool firstUpdate = true;
	irr::core::vector3df replicatedCameraPos;
	irr::core::vector3df replicatedCameraRot;
	irr::core::vector3df lastPos;

	DebugBoxSceneNode* debugBox;

	bool isCreatedCamera;
	irr::core::vector3df shootPosition; // The position the player is shooting from, set by the client
	irr::core::vector3df shootDirection; // The direction the player is shooting, set by the client	
	irr::core::matrix4 collisionTransform;

	GamePlatform gamePlatform;

	//KDA
	int killCnt;
	int deathCnt;
	int shootCnt;
	bool isTeleport;

	int fps;

	irr::core::vector3df respawnPos;
	irr::core::vector3df respawnTarget;

	// In PlayerReplica class (server-side only)
	float SRTT;       // Smoothed RTT (평균 RTT)
	float RTTVAR;     // RTT Variation (RTT 변동폭)
	RakNet::TimeMS Wj_RTO;   // 최종 계산된 Wait Timeout (RTO)
	bool isRtoInitialized; // 초기화 플래그
	int nextExpectedAmCnt; // AM 시퀀스 순서
	int lastProcessedAmCnt = -1; // 마지막으로 처리된 AM 시퀀스
	DataStructures::Map<int, orderData> outOfOrderAmBuffer; // (선택적) 순서가 어긋난 AM을 임시 보관할 버퍼
};
class PlayerBotReplica : public PlayerReplica
{
public:
	PlayerBotReplica();
	virtual void WriteAllocationID(RakNet::Connection_RM3* destinationConnection, RakNet::BitStream* allocationIdBitstream) const;

	virtual RakNet::RM3ConstructionState QueryConstruction(RakNet::Connection_RM3* destinationConnection, RakNet::ReplicaManager3* replicaManager3);
	virtual bool QueryRemoteConstruction(RakNet::Connection_RM3* sourceConnection);
	virtual RakNet::RM3QuerySerializationResult QuerySerialization(RakNet::Connection_RM3* destinationConnection);
	virtual RakNet::RM3ActionOnPopConnection QueryActionOnPopConnection(RakNet::Connection_RM3* droppedConnection) const;
	virtual void PreDestruction(RakNet::Connection_RM3* sourceConnection) override;

	virtual void SerializeConstruction(RakNet::BitStream* constructionBitstream, RakNet::Connection_RM3* destinationConnection);
	virtual bool DeserializeConstruction(RakNet::BitStream* constructionBitstream, RakNet::Connection_RM3* sourceConnection);
	virtual RakNet::RM3SerializationResult Serialize(RakNet::SerializeParameters* serializeParameters);
	virtual void Deserialize(RakNet::DeserializeParameters* deserializeParameters);

	void CreateBotModel();
	irr::scene::IAnimatedMeshSceneNode* botModel; // bot 전용 Model

	virtual PriorityStatics GetPriorityStatics(void) const { return PRIORITY_LOW;} //TODO : UM 마지막 - 이건 서버만 봇 가지고 봇이 1개일 때만을 가정했을 때임
};
class BallReplica : public BaseIrrlichtReplica
{
public:
	BallReplica();
	virtual ~BallReplica();

	virtual RakNet::RM3ConstructionState QueryConstruction(RakNet::Connection_RM3* destinationConnection, RakNet::ReplicaManager3* replicaManager3);
	virtual bool QueryRemoteConstruction(RakNet::Connection_RM3* sourceConnection);
	virtual RakNet::RM3QuerySerializationResult QuerySerialization(RakNet::Connection_RM3* destinationConnection);
	virtual RakNet::RM3ActionOnPopConnection QueryActionOnPopConnection(RakNet::Connection_RM3* droppedConnection) const;
	
	
	// Every function except update is overriding a function in Replica3
	virtual void WriteAllocationID(RakNet::Connection_RM3 *destinationConnection, RakNet::BitStream *allocationIdBitstream) const;
	virtual void SerializeConstruction(RakNet::BitStream *constructionBitstream, RakNet::Connection_RM3 *destinationConnection);
	virtual bool DeserializeConstruction(RakNet::BitStream *constructionBitstream, RakNet::Connection_RM3 *sourceConnection);
	virtual RakNet::RM3SerializationResult Serialize(RakNet::SerializeParameters *serializeParameters);
	virtual void Deserialize(RakNet::DeserializeParameters *deserializeParameters);

	virtual void PostSerializeConstruction(RakNet::BitStream* constructionBitstream, RakNet::Connection_RM3* destinationConnection);

	virtual void PostDeserializeConstruction(RakNet::BitStream *constructionBitstream, RakNet::Connection_RM3 *destinationConnection);
	virtual void PreDestruction(RakNet::Connection_RM3 *sourceConnection);

	virtual void Update(RakNet::TimeMS curTime);

	// shotDirection is networked
	irr::core::vector3df shotDirection;

	// shotlifetime is calculated, not networked
	RakNet::TimeMS shotLifetime;

	int bulletCount;
};
class Connection_RM3Irrlicht : public RakNet::Connection_RM3 {
public:
	Connection_RM3Irrlicht(const RakNet::SystemAddress &_systemAddress, RakNet::RakNetGUID _guid, CDemo *_demo) : RakNet::Connection_RM3(_systemAddress, _guid) {demo=_demo;}
	virtual ~Connection_RM3Irrlicht() {}
	virtual RakNet::Replica3 *AllocReplica(RakNet::BitStream *allocationId, RakNet::ReplicaManager3 *replicaManager3);

	virtual bool QuerySerializationList(DataStructures::List< RakNet::Replica3*>& replicasToSerialize) { 
		(void)replicasToSerialize; 
		
		//우선순위에 따라 정렬
		int index = PriorityStatics::PRIORITY_MAX - 1;
		while(index >=0){
			
			int index2 = 0;
			while (index2 < this->queryToSerializeReplicaList.Size())
			{
				RakNet::LastSerializationResult* lsr = this->queryToSerializeReplicaList[index2];
				BaseIrrlichtReplica* rep = dynamic_cast<BaseIrrlichtReplica*>(lsr->replica);

				if (rep == nullptr) {
					index2++;
					continue;
				}

				if (index == rep->GetPriorityStatics())
					replicasToSerialize.Push(rep, _FILE_AND_LINE_);
				index2++;
			}
			index--;
		}
			
		int t = 0;
		if (replicasToSerialize.Size() > 2)
			t = 1;

		if (replicasToSerialize.Size() > 0) return true;
		else return false; 
	}
protected:
	CDemo *demo;
};

class ReplicaManager3Irrlicht : public RakNet::ReplicaManager3
{
public:
	virtual RakNet::Connection_RM3* AllocConnection(const RakNet::SystemAddress &systemAddress, RakNet::RakNetGUID rakNetGUID) const {
		return new Connection_RM3Irrlicht(systemAddress,rakNetGUID,demo);
	}
	virtual void DeallocConnection(RakNet::Connection_RM3 *connection) const {
		delete connection;
	}

	virtual void PrintTimeGap(char* str) override;
	virtual void SetIsLog(bool isLog) override { this->isLog = isLog; }
	
	CDemo *demo;
};

#endif
