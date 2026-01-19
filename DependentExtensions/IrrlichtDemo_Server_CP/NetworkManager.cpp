/*
 *  Copyright (c) 2014, Oculus VR, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

//#include "RakNetStuff.h"
//

//
//#include "RakNetTime.h"
//#include "GetTime.h"
//#include "SocketLayer.h"


//#include "PacketLogger.h"
//#include <stdio.h>
//#include <time.h>
//#include <chrono>
//
//#include <irrlicht.h>
//#ifdef __linux__
//#include <sys/types.h>
//#include <sys/stat.h>
//#include <unistd.h>
//#include <string.h>   // memcpy, memset
//#endif
//
//#ifdef __ANDROID__
//#include <android/log.h>
//#endif // __ANDROID__

#include "NetworkManager.h"
#include "HUDManager.h"
#include "NetworkIDManager.h"
#include "Replicas.h" // PlayerReplica ���� ���� �ʿ�
#include "NetLogManager.h"
#include "CInGame.h"
#include "MethodManager.h"
#include "PacketHandler.h"
#include "RakNetStatistics.h"

using namespace RakNet;
using namespace irr;
using namespace std;

static const float INGOING_TIME_MS = 0.0f;
NetworkManager* NetworkManager::instance = nullptr;
NetworkManager* NetworkManager::Instance() {
	if (instance == nullptr) instance = new NetworkManager();
	return instance;
}

void NetworkManager::DestroyInstance() {
	if (instance) {
		delete instance;
		instance = nullptr;
	}
}

NetworkManager::NetworkManager() : rakPeer(nullptr), networkIDManager(nullptr), replicaManager3(nullptr), statisticsPlugin(nullptr), playerBotReplica(nullptr), playerReplica(nullptr) {}

NetworkManager::~NetworkManager() {
	// ������ ������ ���� ����
}

void NetworkManager::Initialize(bool isServer, bool isLocalServer, int maxClientCnt) { 
	topology = isServer ? Topology::SERVER : Topology::CLIENT;
	this->isLocalServer = isLocalServer;
	this->MaxClientCnt = maxClientCnt;
	stressPacketsPerUpdate = 1300;
}

void NetworkManager::Activate()
{
	rakPeer = RakNet::RakPeerInterface::GetInstance();
	rakPeer->SetPacketReturnDelay(INGOING_TIME_MS);
	RakNet::SocketDescriptor sd((topology == SERVER) ? SERVER_PORT : 0, 0);
	sd.socketFamily = AF_INET; // Only IPV4 supports broadcast on 255.255.255.255

	networkIDManager = new RakNet::NetworkIDManager;
	replicaManager3 = new ReplicaManager3Irrlicht();
	replicaManager3->SetNetworkIDManager(networkIDManager);
	replicaManager3->SetAutoManageConnections(false, true);
	replicaManager3->SetAutoSerializeInterval(30);
	rakPeer->AttachPlugin(replicaManager3);

	static const int MAX_PLAYERS = 32;
	static const unsigned short TCP_PORT = 0;
	static const RakNet::TimeMS UDP_SLEEP_TIMER = 30;

	RakNet::StartupResult sr;
	if (topology == SERVER) {
		sr = rakPeer->Startup(MAX_PLAYERS, &sd, 1);
		rakPeer->SetMaximumIncomingConnections(MAX_PLAYERS);
		if (MethodManager::Instance()->IsMethodActive(METHOD_3) && MethodManager::Instance()->IsMethodLogActive(METHOD_3)) 
			rakPeer->SetPerConnectionOutgoingBandwidthLimit(0); // 1000000 = 1Mbps
	}
	else sr = rakPeer->Startup(1, &sd, 1);
	rakPeer->SetOccasionalPing(true);

	RakAssert(sr == RakNet::RAKNET_STARTED);

	// Create and register the network object that represents the player
	// Hook RakNet stuff into this class
	if (CInGame::Instance()->isBot) {
		playerBotReplica = new PlayerBotReplica;
		playerBotReplica->CreateBotModel();
		playerBotReplica->gamePlatform = CInGame::Instance()->gamePlatform;
		playerReplica = playerBotReplica;
	}
	else {
		playerReplica = new PlayerReplica;
		playerReplica->gamePlatform = CInGame::Instance()->gamePlatform;
	}

	//Draw debug 
	//scene::ISceneManager* smgr = CInGame::Instance()->GetSceneManager();
	//collisionBoxQueue = new CollisionBoxQueueSceneNode(smgr->getRootSceneNode(), smgr);
	//collisionBoxQueue->demo = demo;

	if (topology == CLIENT) {
#if __ANDROID__
		ConnectionAttemptResult car = rakPeer->Connect(SERVER_IP, SERVER_PORT, 0, 0); //��
#else
		if(isLocalServer)ConnectionAttemptResult car = rakPeer->Connect("127.0.0.1", SERVER_PORT, 0, 0); //��
		else ConnectionAttemptResult car = rakPeer->Connect(SERVER_IP, SERVER_PORT, 0, 0); //��
#endif // __ANDROID__
	}
	else if (topology == SERVER) {

	}
}

void NetworkManager::Shutdown()
{
	DataStructures::List<Replica3*> replicaListOut;
	replicaManager3->GetReplicasCreatedByMe(replicaListOut);
	replicaManager3->BroadcastDestructionList(replicaListOut, RakNet::UNASSIGNED_SYSTEM_ADDRESS);
	rakPeer->SetOccasionalPing(false);

	// Shutdown so the server knows we stopped
	rakPeer->Shutdown(100, 0);

	RakNet::RakPeerInterface::DestroyInstance(rakPeer);
	delete networkIDManager;
	delete replicaManager3;
	
	if (CInGame::Instance()->isBot) {
		playerBotReplica->PreDestruction(0);
		delete playerBotReplica;
	}
	else {
		playerReplica->PreDestruction(0);
		delete playerReplica;
	}
}

void NetworkManager::Update() {
	if (isStressTesting) {
		if (RakNet::GetTimeMS() > stressEndTime) {
			isStressTesting = false;
			HUDManager::Instance()->PushMessage(RakNet::RakString("[Server] Stress Test ENDED."));
			NetLogManager::Instance()->PrintDebug("[Server] Stress Test ENDED.\n");
		}
		else {
			// �� �����Ӹ��� ���ݾ� �����͸� ��ƺ��� (������ ȥ�� ����)
			MethodManager::Instance()->method3cnt++;
			SendStressTestChunk();
		}
	}
}

void NetworkManager::StartStressTest(int durationMS) {
	if (topology != SERVER) return;
	isStressTesting = true;
	stressEndTime = RakNet::GetTimeMS() + durationMS;
	HUDManager::Instance()->PushMessage(RakNet::RakString("[Server] Stress Test STARTED for %d ms (%d packets per frame)", durationMS, stressPacketsPerUpdate));
	NetLogManager::Instance()->PrintDebug("[Server] Stress Test STARTED for %d ms (%d packets per frame)\n", durationMS, stressPacketsPerUpdate);
}

void NetworkManager::SendStressTestChunk() {
	// �� �����Ӵ� ��Ŷ 100�� (100KB) -> 144 ������ ���� �� 14.4Mbps
	const int PACKET_SIZE = 1000; // 1KB
	char dummyData[PACKET_SIZE];
	memset(dummyData, 'S', PACKET_SIZE);

	unsigned short numberOfSystems = rakPeer->NumberOfConnections();
	for (unsigned short i = 0; i < numberOfSystems; i++) {
		RakNet::SystemAddress sa = rakPeer->GetSystemAddressFromIndex(i);
		if (sa == RakNet::UNASSIGNED_SYSTEM_ADDRESS) continue;
		
		RakNet::TimeMS startTime = RakNet::GetTimeMS();
		int method3cnt = MethodManager::Instance()->method3cnt; // ���� ��Ŷ �Ϸù�ȣ
		MethodManager::Instance()->RecordSendTime(sa, method3cnt, startTime, METHOD_3);
		
		for (int j = 0; j < stressPacketsPerUpdate; j++) { 
			RakNet::BitStream bs;
			bs.Write((RakNet::MessageID)ID_GAME_MESSAGE_HEAVY_PACKET);
			bs.Write(RakNet::GetTimeMS()); 
			bs.Write(PACKET_SIZE);
			bs.Write(dummyData, PACKET_SIZE);
			bs.Write((int)j);
			bs.Write(method3cnt); // ��Ŷ �Ϸù�ȣ
			GetPeer()->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, sa, false);
			//MethodManager::Instance()->SendManagedPacket(&bs, sa, METHOD_3);
		}
	}
}

bool Connection_RM3Irrlicht::QuerySerializationList(DataStructures::List< RakNet::Replica3*>& replicasToSerialize) {
	(void)replicasToSerialize;
	//�켱������ ���� ����
	int index = PriorityStatics::PRIORITY_MAX - 1;
	while (index >= 0) {

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
