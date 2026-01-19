#pragma once
#define SERVER_IP "192.168.1.2" 
#define SERVER_IP_LOCAL "127.0.0.1"
#define SERVER_PORT 20123

#include "RakPeerInterface.h"
#include "RakPeer.h"
#include "ReplicaManager3.h"
#include "irrlicht.h"
#include "StatisticsHistory.h"
#include <vector>

using namespace std;
using namespace irr;
using namespace RakNet;

class CInGame;
class ReplicaManager3Irrlicht;
class PlayerReplica;
class PlayerBotReplica;
class PacketLogger;
class Connection_RM3Irrlicht;

enum Topology {
	CLIENT,
	SERVER
};

enum GamePlatform {
	Shooter,
	Holder,
	Server,
	Android,
	Window,
};


class NetworkManager
{
public:
	// �̱��� ���� (���� ���� ���� ���ټ��� �����ϸ鼭 ĸ��ȭ)
	static NetworkManager* Instance();
	static void DestroyInstance();

	NetworkManager();
	~NetworkManager();

	// �ʱ�ȭ �� ����
	void Initialize(bool isServer, bool isLocalServer, int MaxClientCnt);
	void Activate();
	void Shutdown();
	void Update();

	// Getter
	RakPeerInterface* GetPeer() const { return rakPeer; }
	NetworkIDManager* GetNetworkIDManager() const { return networkIDManager; }
	ReplicaManager3Irrlicht* GetReplicaManager() const { return replicaManager3; }
	PlayerReplica* GetPlayerReplica() const { return playerReplica; }
	PlayerBotReplica* GetPlayerBotReplica() const { return playerBotReplica; }

	Topology GetTopology() const { return topology; }
	bool IsServer() const { return topology == Topology::SERVER; }
	void StartStressTest(int durationMS);
	void SendStressTestChunk();
	
	// ��� �÷����� ���� (NetLogManager��)
	StatisticsHistoryPlugin* GetStatisticsPlugin() const { return statisticsPlugin; }
	void AddPlayer(PlayerReplica* player) { playerList.Push(player, _FILE_AND_LINE_); }
	void RemovePlayer(PlayerReplica* player) {unsigned int idx = playerList.GetIndexOf(player);if (idx != (unsigned int)-1) playerList.RemoveAtIndex(idx);}
	DataStructures::List<PlayerReplica*>& GetPlayerList() { return playerList; };
	int GetMaxClientCnt() const { return MaxClientCnt; }
	int GetstressPacketsPerUpdate( ) const { return stressPacketsPerUpdate; }

private:
	static NetworkManager* instance;
	RakPeerInterface* rakPeer;
	NetworkIDManager* networkIDManager;
	ReplicaManager3Irrlicht* replicaManager3;
	
	StatisticsHistoryPlugin* statisticsPlugin;
	PacketLogger* loggerPlugin;

	Topology topology;
	DataStructures::List<PlayerReplica*> playerList;
	CInGame* Game; // ���� ���� ����
	PlayerReplica* playerReplica;
	PlayerBotReplica* playerBotReplica;
	int MaxClientCnt;
	bool isLocalServer = false;
	bool isStressTesting = false;
	RakNet::TimeMS stressEndTime = 0;
	int stressPacketsPerUpdate;
};

class Connection_RM3Irrlicht : public RakNet::Connection_RM3 {
public:
	Connection_RM3Irrlicht(const RakNet::SystemAddress& _systemAddress, RakNet::RakNetGUID _guid) : RakNet::Connection_RM3(_systemAddress, _guid) {  }
	virtual ~Connection_RM3Irrlicht() {}
	virtual RakNet::Replica3* AllocReplica(RakNet::BitStream* allocationId, RakNet::ReplicaManager3* replicaManager3);
	virtual bool QuerySerializationList(DataStructures::List< RakNet::Replica3*>& replicasToSerialize); 
protected:
};

class ReplicaManager3Irrlicht : public RakNet::ReplicaManager3
{
public:
	virtual RakNet::Connection_RM3* AllocConnection(const RakNet::SystemAddress& systemAddress, RakNet::RakNetGUID rakNetGUID) const { return new Connection_RM3Irrlicht(systemAddress, rakNetGUID ); }
	virtual void DeallocConnection(RakNet::Connection_RM3* connection) const { delete connection; }
	virtual void PrintTimeGap(char* str)  override { /*statBufList[0].Push(RakNet::RakString(str), _FILE_AND_LINE_); //Log*/ }
	virtual void SetIsLog(bool isLog) override { this->isLog = isLog; }
};

static const float INTERP_TIME_MS = 100.0f;

