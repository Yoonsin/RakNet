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
#include "NetworkIDManager.h"
#include "Replicas.h" // PlayerReplica 등을 위해 필요
#include "NetLogManager.h"
#include "CInGame.h"
#include "RakNetStatistics.h"

using namespace RakNet;
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

NetworkManager::NetworkManager() : rakPeer(nullptr), networkIDManager(nullptr), replicaManager3(nullptr), statisticsPlugin(nullptr) {}

NetworkManager::~NetworkManager() {
	// 안전한 포인터 삭제 로직
}

void NetworkManager::Initialize() { 
}

void NetworkManager::Activate()
{
	topology = isServer ? Topology::SERVER : Topology::CLIENT;
	rakPeer = RakNet::RakPeerInterface::GetInstance();
	rakPeer->SetPacketReturnDelay(INGOING_TIME_MS);
	RakNet::SocketDescriptor sd((topology == SERVER) ? SERVER_PORT : 1234, 0);
	sd.socketFamily = AF_INET; // Only IPV4 supports broadcast on 255.255.255.255

	if (topology == CLIENT) {
		while (IRNS2_Berkley::IsPortInUse(sd.port, sd.hostAddress, sd.socketFamily, SOCK_DGRAM) == true)
			sd.port++;
	}

	networkIDManager = new RakNet::NetworkIDManager;
	replicaManager3 = new ReplicaManager3Irrlicht();
	replicaManager3->SetNetworkIDManager(networkIDManager);
	replicaManager3->demo = demo; // ReplicaManager에 Demo 포인터 전달
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
		//if (demo->evalMask & METHOD_3)
			//rakPeer->ApplyNetworkSimulator(0.1f, 100, 50);
	}
	else sr = rakPeer->Startup(1, &sd, 1);
	rakPeer->SetOccasionalPing(true);

	RakAssert(sr == RakNet::RAKNET_STARTED);

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
	//scene::ISceneManager* smgr = demo->GetSceneManager();
	//collisionBoxQueue = new CollisionBoxQueueSceneNode(smgr->getRootSceneNode(), smgr);
	//collisionBoxQueue->demo = demo;

	if (topology == CLIENT) {
#if __ANDROID__
		ConnectionAttemptResult car = rakPeer->Connect(SERVER_IP, SERVER_PORT, 0, 0); //랜
#else
		ConnectionAttemptResult car = rakPeer->Connect("127.0.0.1", SERVER_PORT, 0, 0); //랜
		//ConnectionAttemptResult car = rakPeer->Connect(SERVER_IP, SERVER_PORT, 0, 0); //랜

#endif // __ANDROID__
		//loggerPlugin = PacketLogger::GetInstance();
		//rakPeer->AttachPlugin(loggerPlugin);
	}
	else if (topology == SERVER) {
		/*if(isBot) replicaManager3->Reference(playerBotReplica);
		else replicaManager3->Reference(playerReplica);*/
		if (CInGame::Instance()->IsMethodActive(3)) {
			statisticsPlugin = StatisticsHistoryPlugin::GetInstance();
			statisticsPlugin->SetTrackConnections(true, 0, true);
			rakPeer->AttachPlugin(statisticsPlugin);
		}
		//loggerPlugin = PacketLogger::GetInstance();
		//rakPeer->AttachPlugin(loggerPlugin);
	}
}

void NetworkManager::Shutdown(bool isLogged, bool isBot, const char* baseDir, int evalMask)
{
	DataStructures::List<Replica3*> replicaListOut;
	replicaManager3->GetReplicasCreatedByMe(replicaListOut);
	replicaManager3->BroadcastDestructionList(replicaListOut, RakNet::UNASSIGNED_SYSTEM_ADDRESS);

	if (isLogged) {
		if (topology == SERVER) {
			NetLogManager::Instance()->SaveStatisticsToCSV(baseDir, 0);

			if (CInGame::instance()->IsMethodActive(3)) {
				//Egress, Ingress
				NetLogManager::Instance()->SaveStatisticsToCSV(baseDir, 3);
			}
		}
		else if (topology == CLIENT) {
			if (CInGame::instance()->IsMethodActive(1))
				NetLogManager::Instance()->SaveStatisticsToCSV(baseDir, 1);
		}
	}

	rakPeer->SetOccasionalPing(false);

	// Shutdown so the server knows we stopped
	rakPeer->Shutdown(100, 0);

	RakNet::RakPeerInterface::DestroyInstance(rakPeer);
	delete networkIDManager;
	delete replicaManager3;
	
	if (evalMask & METHOD_3) {
		if (topology == SERVER) {
			StatisticsHistoryPlugin::DestroyInstance(statisticsPlugin);
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
}

bool Connection_RM3Irrlicht::QuerySerializationList(DataStructures::List< RakNet::Replica3*>& replicasToSerialize) {
	(void)replicasToSerialize;
	//우선순위에 따라 정렬
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
