#pragma once
#include "RakPeerInterface.h"
#include "RakPeer.h"
#include "ReplicaManager3.h"
#include "CollisionManager.h"
#include "DS_Map.h"
#include <irrlicht.h>
#include <vector3d.h>
using namespace std;
using namespace irr;

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

class CInGame;
class PlayerReplica;
class PlayerBotReplica;
const int HISTORY_DURATION_MS = 1000;
// Base RakNet custom classes for Replica Manager 3, setup peer to peer networking
class BaseIrrlichtReplica : public RakNet::Replica3
{
public:
	BaseIrrlichtReplica();
	virtual ~BaseIrrlichtReplica();

	virtual RakNet::RM3ConstructionState QueryConstruction(RakNet::Connection_RM3* destinationConnection, RakNet::ReplicaManager3* replicaManager3) { return QueryConstruction_PeerToPeer(destinationConnection); }
	virtual bool QueryRemoteConstruction(RakNet::Connection_RM3* sourceConnection) { return QueryRemoteConstruction_PeerToPeer(sourceConnection); }
	virtual RakNet::RM3ActionOnPopConnection QueryActionOnPopConnection(RakNet::Connection_RM3* droppedConnection) const { return QueryActionOnPopConnection_PeerToPeer(droppedConnection); }
	virtual RakNet::RM3QuerySerializationResult QuerySerialization(RakNet::Connection_RM3* destinationConnection) { return QuerySerialization_PeerToPeer(destinationConnection); }

	virtual void DeallocReplica(RakNet::Connection_RM3* sourceConnection) { delete this; }
	virtual void SerializeConstruction(RakNet::BitStream* constructionBitstream, RakNet::Connection_RM3* destinationConnection);
	virtual bool DeserializeConstruction(RakNet::BitStream* constructionBitstream, RakNet::Connection_RM3* sourceConnection);
	virtual RakNet::RM3SerializationResult Serialize(RakNet::SerializeParameters* serializeParameters);
	virtual void Deserialize(RakNet::DeserializeParameters* deserializeParameters);
	virtual void SerializeDestruction(RakNet::BitStream* destructionBitstream, RakNet::Connection_RM3* destinationConnection) {}
	virtual bool DeserializeDestruction(RakNet::BitStream* destructionBitstream, RakNet::Connection_RM3* sourceConnection) { return true; }

	virtual PriorityStatics GetPriorityStatics(void) const { return PRIORITY_MEDIUM; }

	/// This function is not derived from Replica3, it's specific to this appss
	/// Called from CInGame::UpdateRakNet
	virtual void Update(RakNet::TimeMS curTime);

	// Set when the object is constructed
	CInGame* demo;
	// real is written on the owner peer, read on the remote peer
	irr::core::vector3df position;
	RakNet::TimeMS creationTime;

	bool IsServer() const { return NetworkManager::Instance()->IsServer(); }
};
// Game classes automatically updated by ReplicaManager3
class PlayerReplica : public BaseIrrlichtReplica, public irr::scene::IAnimationEndCallBack
{
public:
	PlayerReplica();
	virtual ~PlayerReplica();
	// Every function below, before Update overriding a function in Replica3
	virtual void WriteAllocationID(RakNet::Connection_RM3* destinationConnection, RakNet::BitStream* allocationIdBitstream) const;
	virtual void SerializeConstruction(RakNet::BitStream* constructionBitstream, RakNet::Connection_RM3* destinationConnection);
	virtual bool DeserializeConstruction(RakNet::BitStream* constructionBitstream, RakNet::Connection_RM3* sourceConnection);
	virtual RakNet::RM3SerializationResult Serialize(RakNet::SerializeParameters* serializeParameters);
	virtual void Deserialize(RakNet::DeserializeParameters* deserializeParameters);
	virtual void PostDeserializeConstruction(RakNet::BitStream* constructionBitstream, RakNet::Connection_RM3* destinationConnection);
	virtual void PreDestruction(RakNet::Connection_RM3* sourceConnection);

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

	virtual PriorityStatics GetPriorityStatics(void) const { return PRIORITY_LOW; } //TODO : UM 마지막 - 이건 서버만 봇 가지고 봇이 1개일 때만을 가정했을 때임
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
	virtual void WriteAllocationID(RakNet::Connection_RM3* destinationConnection, RakNet::BitStream* allocationIdBitstream) const;
	virtual void SerializeConstruction(RakNet::BitStream* constructionBitstream, RakNet::Connection_RM3* destinationConnection);
	virtual bool DeserializeConstruction(RakNet::BitStream* constructionBitstream, RakNet::Connection_RM3* sourceConnection);
	virtual RakNet::RM3SerializationResult Serialize(RakNet::SerializeParameters* serializeParameters);
	virtual void Deserialize(RakNet::DeserializeParameters* deserializeParameters);

	virtual void PostSerializeConstruction(RakNet::BitStream* constructionBitstream, RakNet::Connection_RM3* destinationConnection);

	virtual void PostDeserializeConstruction(RakNet::BitStream* constructionBitstream, RakNet::Connection_RM3* destinationConnection);
	virtual void PreDestruction(RakNet::Connection_RM3* sourceConnection);

	virtual void Update(RakNet::TimeMS curTime);

	// shotDirection is networked
	irr::core::vector3df shotDirection;

	// shotlifetime is calculated, not networked
	RakNet::TimeMS shotLifetime;

	int bulletCount;
};

