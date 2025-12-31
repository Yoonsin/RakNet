#include "PacketHandler.h"
#include "NetworkManager.h"
#include "getTime.h"
#include "MessageIdentifiers.h"


PacketHandler* PacketHandler::instance = nullptr;
PacketHandler* PacketHandler::Instance() {
	if (instance == nullptr) instance = new PacketHandler();
	return instance;
}
void PacketHandler::DestroyInstance() {
	if (instance) {
		delete instance;
		instance = nullptr;
	}
}

PacketHandler::PacketHandler() {}
PacketHandler::~PacketHandler() {
	// 안전한 포인터 삭제 로직
}

void PacketHandler::Update() {
	OnHandlePacket();
	OnUpdateReplica();
}
void PacketHandler::OnHandlePacket() {
	RakNet::Packet* packet;
	RakNet::TimeMS curTime = RakNet::GetTimeMS();
	RakNet::RakString targetName;

	for (packet = NetworkManager::Instance()->GetPeer()->Receive(); packet; NetworkManager::Instance()->GetPeer()->DeallocatePacket(packet), packet = NetworkManager::Instance()->GetPeer()->Receive())
	{
		targetName = packet->systemAddress.ToString(true);
		switch (packet->data[0])
		{
		case ID_NEW_INCOMING_CONNECTION:
		{
			PushMessage(RakNet::RakString("Sending player list to new connection"));
			RakNet::Connection_RM3* connection = NetworkManager::Instance()->GetReplicaManager()->AllocConnection(packet->systemAddress, NetworkManager::Instance()->GetPeer()->GetGuidFromSystemAddress(packet->systemAddress));
			NetworkManager::Instance()->GetReplicaManager()->PushConnection(connection);

			if (logCount == NetworkManager::Instance()->GetReplicaManager()->GetConnectionCount())
			{
				if (isBot) NetworkManager::Instance()->GetReplicaManager()->Reference(NetworkManager::Instance()->GetPlayerBotReplica());
				else NetworkManager::Instance()->GetReplicaManager()->Reference(NetworkManager::Instance()->GetPlayerReplica());

				PushMessage(RakNet::RakString("All Player Connected. Game Pending..."));
				gameStartTime = curTime + GAME_START_PENDING_TIME;
			}
		}
		break;
		case ID_CONNECTION_REQUEST_ACCEPTED:
		{
			isConnected = true;
			serverSystemAddress = packet->systemAddress;
			if (isConnected) {
				RakNet::Connection_RM3* connection = NetworkManager::Instance()->GetReplicaManager()->AllocConnection(serverSystemAddress, NetworkManager::Instance()->GetPeer()->GetGuidFromSystemAddress(serverSystemAddress));
				//NetworkManager::Instance()->GetReplicaManager()에 추적될 수 있도록 할당
				NetworkManager::Instance()->GetReplicaManager()->PushConnection(connection);

				//객체 생성
				if (isBot)NetworkManager::Instance()->GetReplicaManager()->Reference(NetworkManager::Instance()->GetPlayerBotReplica());
				else NetworkManager::Instance()->GetReplicaManager()->Reference(NetworkManager::Instance()->GetPlayerReplica());
			}

			//SwitchNextScene() 에서 연결하는 것으로 변경 -> 왜 이렇게 바꾸자고 했지?
			//그렇게 변경하면 수신 딜레이 적용시 timeout으로 연결 수립이 안됩니다..
		}
		break;
		case ID_TIMESTAMP:
		{
			RakNet::BitStream bsIn(packet->data, packet->length, false);
			bsIn.IgnoreBytes(1);
			RakNet::Time time;
			bsIn.Read(time);
			RakNet::MessageID messageId;
			bsIn.Read(messageId);

			switch (messageId)
			{
			case CDemo::ID_GAME_MESSAGE_PLAYER_RESPAWN:
			{
				RakNet::BitStream bsIn(packet->data, packet->length, false);
				bsIn.IgnoreBytes(1);

				RakNet::RakNetGUID botGuid;
				core::vector3df respawnPos;
				core::vector3df respawnTarget;
				bool eval1;

				//리스폰 요청 보낸 봇의 GUID
				bsIn.Read(botGuid);
				bsIn.Read(respawnPos);
				bsIn.Read(respawnTarget);
				bsIn.Read(eval1);

				if (isServer) {
					//보낸 이를 제외한 모두에게 다시 브로드캐스팅
					RakNet::BitStream bs;
					bs.Write((RakNet::MessageID)CDemo::ID_GAME_MESSAGE_PLAYER_RESPAWN);
					bs.Write(botGuid);
					bs.Write(respawnPos);
					bs.Write(respawnTarget);
					bs.Write(false);

					NetworkManager::Instance()->GetPeer()->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, botGuid, true);
				}
				else {
					if (eval1) {
						/*char strDisplay[100];
						sprintf(strDisplay, "Time difference is %" PRINTF_64_BIT_MODIFIER "u\n", RakNet::GetTime() - time);
						PushMessage(strDisplay);
						*/
						RakNet::Time t = RakNet::GetTime() - time;
						statBufList[1].Push(RakNet::RakString::ToString(t) + RakNet::RakString("\n"), _FILE_AND_LINE_);
					}
				}

				for (int idx = 0; idx < NetworkManager::Instance()->GetPlayerList().Size(); ++idx)
				{
					PlayerReplica* player = NetworkManager::Instance()->GetPlayerList()[idx];
					if (player->creatingSystemGUID == botGuid)
					{
						player->isTeleport = true;
						player->position = respawnPos;
						break;
					}
				}
			}
			break;
			}

		}
		break;
		case ID_GAME_MESSAGE_BALL_REQUEST:
		{
			if (isServer) {
				RakNet::BitStream bsIn(packet->data, packet->length, false);
				bsIn.IgnoreBytes(1);

				irr::core::vector3df pos, target;
				int bulletCnt;
				bsIn.Read(pos);
				bsIn.Read(target);
				bsIn.Read(bulletCnt);

				BallReplica* br = new BallReplica;
				br->demo = this;
				br->position = pos;
				br->shotDirection = target;
				// 원래는 gamePlatform이 보낸 이의 플랫폼이어야 함 (여기서는 Shooter로 가정될 수 있음)
				br->shotLifetime = RakNet::GetTimeMS() + shootFromOrigin(pos, target, gamePlatform);
				br->bulletCount = bulletCnt;
				NetworkManager::Instance()->GetReplicaManager()->Reference(br);
			}
		}
		break;
		case ID_GAME_MESSAGE_PLAYER_LIFE:
		{
			RakNet::BitStream bsIn(packet->data, packet->length, false);
			bsIn.IgnoreBytes(1);

			RakNet::RakNetGUID LifeUpdateGuid;
			RakNet::RakNetGUID ShooterGuid;
			bool isDead;
			bsIn.Read(ShooterGuid); // The player who shot
			bsIn.Read(isDead);
			if (isDead) {
				//Shooter = 죽인 사람 / Holder = 죽은 사람
				RakNet::RakNetGUID HolderGuid;
				bsIn.Read(HolderGuid);
				RakNet::RakString shooterName;
				RakNet::RakString holderName;
				bsIn.Read(shooterName);
				bsIn.Read(holderName);

				//DebugPrintf("Shooter Name : %s / Holder Name : %s\n", shooterName, holderName);
				KillLog logEntry{ shooterName + RakNet::RakString(" -> ") + holderName + RakNet::RakString("\n"), RakNet::GetTimeMS() };
				killLogMessages.Push(logEntry, _FILE_AND_LINE_); // Record the kill log message
				LifeUpdateGuid = HolderGuid;

				if (shooterName == NetworkManager::Instance()->GetPlayerReplica()->playerName) NetworkManager::Instance()->GetPlayerReplica()->killCnt++;
				else if (holderName == NetworkManager::Instance()->GetPlayerReplica()->playerName) NetworkManager::Instance()->GetPlayerReplica()->deathCnt++;
				SetPlayerNameText();

				isKeyLock = true;              // onEvent 등에서 키 처리 차단 (이미 사용중인 플래그)
				EnableInput(!isKeyLock);
			}
			else {
				//Shooter = 부활한 사람
				LifeUpdateGuid = ShooterGuid;
				isKeyLock = false;
				EnableInput(!isKeyLock);
			}

			for (int idx = 0; idx < NetworkManager::Instance()->GetPlayerList().Size(); ++idx)
			{
				PlayerReplica* player = NetworkManager::Instance()->GetPlayerList()[idx];
				if (player->creatingSystemGUID == LifeUpdateGuid)
				{
					player->isDead = isDead;
					if (isDead == false) {
						player->bulletCoolTime = -1;
					}

					if (isDead == false && LifeUpdateGuid == NetworkManager::Instance()->GetPeer()->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS) && player->isBot && isServer == false) {
						//나 서버 아님 + 봇 플레이어가 나임 => 부활했다면?
						//리스폰 후 리스폰 요청
						SEvent botKeyEvent;
						botKeyEvent.EventType = EET_KEY_INPUT_EVENT;
						botKeyEvent.KeyInput.Key = KEY_KEY_W;
						botKeyEvent.KeyInput.PressedDown = false;
						KeyIsDown[botKeyEvent.KeyInput.Key] = botKeyEvent.KeyInput.PressedDown;
						if (device->getSceneManager()->getActiveCamera()) {
							device->getSceneManager()->getActiveCamera()->OnEvent(botKeyEvent);
						}

						SetResetBot();
						Respawn(NetworkManager::Instance()->GetPlayerBotReplica()->respawnPos, NetworkManager::Instance()->GetPlayerBotReplica()->respawnTarget);
						botMoveTime = RakNet::GetTimeMS() + BOT_MOVE_TIME;

						RakNet::BitStream bs;
						bs.Write((RakNet::MessageID)CDemo::ID_GAME_MESSAGE_PLAYER_RESPAWN);
						bs.Write(NetworkManager::Instance()->GetPlayerBotReplica()->creatingSystemGUID);
						bs.Write(NetworkManager::Instance()->GetPlayerBotReplica()->respawnPos);
						bs.Write(NetworkManager::Instance()->GetPlayerBotReplica()->respawnTarget);
						bs.Write(false);
						NetworkManager::Instance()->GetPeer()->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
					}
					break;
				}
			}
		}
		break;
		case ID_GAME_MESSAGE_PLAYER_NAME:
		{
			RakNet::BitStream bsIn(packet->data, packet->length, false);
			bsIn.IgnoreBytes(1);
			int playerCnt;
			bsIn.Read(playerCnt); // Read the number of players

			for (int i = 0; i < playerCnt; i++) {
				RakNet::RakString name;
				bsIn.Read(name);
				RakNet::RakNetGUID g;
				bsIn.Read(g);

				for (int i = 0; i < NetworkManager::Instance()->GetPlayerList().Size(); i++) {
					if (NetworkManager::Instance()->GetPlayerList()[i]->creatingSystemGUID == g) {
						NetworkManager::Instance()->GetPlayerList()[i]->playerName = name;

						//만약 현재 플랫폼이 Android (Holder) 이고, 다른 플랫폼이 PC (Shooter)인 경우 (그 반대도 포함)
						//Android의 위치를 GUI에 띄울 수 있도록 한다
						if (NetworkManager::Instance()->GetPlayerReplica()->gamePlatform == Holder && NetworkManager::Instance()->GetPlayerList()[i]->gamePlatform == Shooter)
							SetHolderPosText(NetworkManager::Instance()->GetPlayerReplica()->position);
						else if (NetworkManager::Instance()->GetPlayerReplica()->gamePlatform == Shooter && NetworkManager::Instance()->GetPlayerList()[i]->gamePlatform == Holder)
							SetHolderPosText(NetworkManager::Instance()->GetPlayerList()[i]->model->getPosition());
					}
				}
			}
			SetPlayerNameText();
			PushMessage(RakNet::RakString("Client Name Update"));
		}
		break;
		case ID_GAME_MESSAGE_PLAYER_RESPAWN:
		{
			RakNet::BitStream bsIn(packet->data, packet->length, false);
			bsIn.IgnoreBytes(1);

			RakNet::RakNetGUID botGuid;
			core::vector3df respawnPos;
			core::vector3df respawnTarget;
			bool eval1;

			//리스폰 요청 보낸 봇의 GUID
			bsIn.Read(botGuid);
			bsIn.Read(respawnPos);
			bsIn.Read(respawnTarget);
			bsIn.Read(eval1);

			if (isServer) {
				//보낸 이를 제외한 모두에게 다시 브로드캐스팅
				RakNet::BitStream bs;
				bs.Write((RakNet::MessageID)CDemo::ID_GAME_MESSAGE_PLAYER_RESPAWN);
				bs.Write(botGuid);
				bs.Write(respawnPos);
				bs.Write(respawnTarget);
				bs.Write(false);
				NetworkManager::Instance()->GetPeer()->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, botGuid, true);
			}

			for (int idx = 0; idx < NetworkManager::Instance()->GetPlayerList().Size(); ++idx)
			{
				PlayerReplica* player = NetworkManager::Instance()->GetPlayerList()[idx];
				if (player->creatingSystemGUID == botGuid)
				{
					player->isTeleport = true;
					player->position = respawnPos;
					break;
				}
			}

		}
		break;
		case ID_GAME_MESSAGE_GAME_MATCH:
		{
			RakNet::BitStream bsIn(packet->data, packet->length, false);
			bsIn.IgnoreBytes(1);
			GameMatchState matchState;
			bsIn.Read(matchState);

			if (matchState == GameMatchState::GAME_MATCH_START) isGameStart = true;
			else if (matchState == GameMatchState::GAME_MATCH_END) isGameEnd = true;
		}
		break;
		}
	}
}
void PacketHandler::SendRespawnPacket() {
	RakNet::BitStream bs;
	if (MethodManager::Instance()->IsMethodActive(1)) {
		MethodManager::Instance()->UpdateMethod(1);
		if (MethodManager::Instance()->eval1cnt > 100) {
			isGameEnd = true;
			return;
		}

		bs.Write((RakNet::MessageID)ID_TIMESTAMP);
		bs.Write(RakNet::GetTime());
	}

	bs.Write((RakNet::MessageID)CInGame::ID_GAME_MESSAGE_PLAYER_RESPAWN);
	bs.Write(NetworkManager::Instance()->GetPlayerBotReplica()->creatingSystemGUID);
	bs.Write(NetworkManager::Instance()->GetPlayerBotReplica()->respawnPos);
	bs.Write(NetworkManager::Instance()->GetPlayerBotReplica()->respawnTarget);
	bs.Write(MethodManager::Instance()->eval1bool);

	NetworkManager::Instance()->GetPeer()->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
}
void PacketHandler::OnUpdateReplica() {
	if (currentScene >= 1)
	{
		//(서버 봇을 제외한) 모든 플레이어가 생성되면 서버 측에서 이름을 할당
		//클라 측에서 이름을 할당하는게 더 편하나 현재 모바일에서 이름을 할당하기 어려운 문제가 있어서 서버 측에서 할당.
		if (isServer && isPlayersNameSet == false && logCount == NetworkManager::Instance()->GetPlayerList().Size() - serverPlayerCnt) {
			isPlayersNameSet = true;
			int playerCnt = 1; int botCnt = 1;
			RakNet::RakString serverNames("My Name : ");

			RakNet::BitStream bs;
			bs.Write((RakNet::MessageID)CDemo::ID_GAME_MESSAGE_PLAYER_NAME);
			bs.Write(NetworkManager::Instance()->GetPlayerList().Size());
			for (int i = 0; i < NetworkManager::Instance()->GetPlayerList().Size(); i++) {
				PlayerReplica* player = NetworkManager::Instance()->GetPlayerList()[i];
				if (player->isBot) player->playerName = RakNet::RakString("bot%d", botCnt++);
				else player->playerName = RakNet::RakString("Player%d", playerCnt++);

				if (player->gamePlatform == Server) player->playerName += RakNet::RakString(" (S)");
				else if (player->gamePlatform == Shooter) player->playerName += RakNet::RakString(" (PC)");
				else if (player->gamePlatform == Holder) player->playerName += RakNet::RakString(" (M)");

				if (player->creatingSystemGUID == NetworkManager::Instance()->GetPeer()->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))
					serverNames += player->playerName + RakNet::RakString(" ");

				bs.Write(player->playerName);
				bs.Write(player->creatingSystemGUID);
			}

			NetworkManager::Instance()->GetPeer()->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);

			//서버 측 이름은 여기서 할당
			const char* charStr = serverNames.C_String();
			wchar_t wcharStr[128];
			mbstowcs(wcharStr, charStr, sizeof(wcharStr) / sizeof(wchar_t));
			myNameText->setText(wcharStr);
			PushMessage(RakNet::RakString("Bot Name Update"));
		}

		//게임 시작 확인
		if (isServer && gameStartTime <= curTime && isGameStart == false && gameStartTime != 0) {
			isGameStart = true;
			PushMessage(RakNet::RakString("Game Start!"));

			//모든 플레이어에게 게임 시작 알리기
			RakNet::BitStream bs;
			bs.Write((RakNet::MessageID)CDemo::ID_GAME_MESSAGE_GAME_MATCH);
			bs.Write(GameMatchState::GAME_MATCH_START);
			NetworkManager::Instance()->GetPeer()->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
			gameStartTime = curTime; //게임 시작 시간 기록

#if QOS_SUPPORTED 
			sem.setKey(888); sem.setupSemaphore(0);
			write_shm.setKey(777); write_shm.setupSharedMemory(1200); write_shm.attachSharedMemory();
			WriteQoSInfo(); isQosWritten = true; QosWriteTime = curTime + QOS_WRITE_COOL_TIME;
#endif
		}

		// 객체 업데이트 및 게임 종료 통계 저장
		bool isTimeRecorded = false;
		for (unsigned int idx = 0; idx < NetworkManager::Instance()->GetReplicaManager()->GetReplicaCount(); idx++) {
			((BaseIrrlichtReplica*)(NetworkManager::Instance()->GetReplicaManager()->GetReplicaAtIndex(idx)))->Update(curTime);

			if (isServer && isGameEnd && isLogged && (evalMask == 0)) {
				PlayerReplica* pr = dynamic_cast<PlayerReplica*>(NetworkManager::Instance()->GetReplicaManager()->GetReplicaAtIndex(idx));
				if (pr != nullptr) {
					char buffer[200];
					if (!isTimeRecorded) {
						isTimeRecorded = true;
						char timeBuf[50];
						snprintf(timeBuf, sizeof(timeBuf), "Game End! Total Time : %02u MS\n", (curTime - gameStartTime));
						statBufList[0].Push(RakNet::RakString(timeBuf), _FILE_AND_LINE_);
					}
					snprintf(buffer, sizeof(buffer), "%s/%d/%d/%d\n", pr->playerName.C_String(), pr->killCnt, pr->deathCnt, pr->shootCnt);
					statBufList[0].Push(RakNet::RakString(buffer), _FILE_AND_LINE_);
				}
			}
		}

		// -----------------------------------------------------------
		// 3. [FoS] 패킷 오더링 큐 검사 및 처리 (METHOD_2)
		// -----------------------------------------------------------
		if (isServer && (evalMask & METHOD_2))
		{
			// [Step A] 모든 플레이어의 RTO(Wj) 갱신
			for (unsigned int idx = 0; idx < NetworkManager::Instance()->GetPlayerList().Size(); idx++)
			{
				PlayerReplica* player = NetworkManager::Instance()->GetPlayerList()[idx];
				// 서버 로컬 봇은 제외.. 왜 안됨?
				if (player->creatingSystemGUID == NetworkManager::Instance()->GetPeer()->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))
					continue;

				RakNet::TimeMS SampleRTT = NetworkManager::Instance()->GetPeer()->GetLastPing(player->creatingSystemGUID);
				if (SampleRTT < 1) SampleRTT = 1;

				if (!player->isRtoInitialized) {
					player->SRTT = (float)SampleRTT;
					player->RTTVAR = (float)SampleRTT / 2.0f;
					player->isRtoInitialized = true;
				}
				else {
					float alpha = 0.125f; float beta = 0.25f;
					float Difference = fabsf((float)SampleRTT - player->SRTT);
					player->RTTVAR = (1.0f - beta) * player->RTTVAR + beta * Difference;
					player->SRTT = (1.0f - alpha) * player->SRTT + alpha * (float)SampleRTT;
				}
				player->Wj_RTO = (RakNet::TimeMS)(player->SRTT + 4.0f * player->RTTVAR);
				if (player->Wj_RTO < 200) player->Wj_RTO = 200;
				if (player->Wj_RTO > 3000) player->Wj_RTO = 3000;
			}

			// [Step B] 큐 처리 루프
			while (orderPq.Size() >= 1)
			{
				// 큐의 헤드 메시지 확인 (아직 Pop 하지 않음)
				const orderData& M_k = orderPq.Peek(0);
				RakNet::TimeMS U_i = umTimeMap[M_k.um_cnt];
				RakNet::TimeMS delta_k = M_k.reactionTime;

				// max(Wj) 계산 최적화
				// : M_k와 동일한 UM에 대해, 이미 큐에 메시지가 도착해 있는 플레이어 목록(Set)을 만듭니다.
				//   이들은 이미 "반응"했으므로(순서 보장됨), 대기 시간(Wj)을 적용할 필요가 없습니다.
				std::set<RakNet::RakNetGUID> submittedPlayers;

				// 큐 전체를 순회하며 현재 um_cnt와 같은 메시지를 보낸 플레이어 식별
				for (unsigned int q_idx = 0; q_idx < orderPq.Size(); ++q_idx) {
					// DS_Heap.h에 추가한 GetNode() 사용
					const auto& node = orderPq.GetNode(q_idx);
					if (node.data.um_cnt == M_k.um_cnt) {
						submittedPlayers.insert(node.data.playerGUID);
					}
				}

				RakNet::TimeMS max_Wj = 0;
				for (unsigned int p_idx = 0; p_idx < NetworkManager::Instance()->GetPlayerList().Size(); ++p_idx)
				{
					PlayerReplica* player_j = NetworkManager::Instance()->GetPlayerList()[p_idx];

					// 제외 대상: 서버 자신, 메시지 M_k의 발신자
					if (player_j->creatingSystemGUID == NetworkManager::Instance()->GetPeer()->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS)) continue;
					if (player_j->creatingSystemGUID == M_k.playerGUID) continue;

					// [최적화] 이미 해당 UM에 대한 메시지가 큐에 도착한 플레이어는 기다리지 않음
					if (submittedPlayers.find(player_j->creatingSystemGUID) != submittedPlayers.end())
						continue;

					if (player_j->Wj_RTO > max_Wj) {
						max_Wj = player_j->Wj_RTO;
					}
				}

				// [Step C] 배달 시간(Process Time) 계산 및 처리
				// 논문 공식: D(Mk) = Ui + max(Wj) + delta_k
				// 여기서 max(Wj)는 아직 응답하지 않은 잠재적 플레이어들의 최대 대기시간입니다.
				RakNet::TimeMS calculated_processTime = U_i + max_Wj + delta_k;

				// DataStructures::Heap은 내부 데이터의 직접 수정을 통한 재정렬을 지원하지 않으므로,
				// processTime은 계산 용도로만 쓰고 큐 내부 데이터를 수정하지 않는 것이 안전하지만,
				// 여기서는 로직상 매번 계산하여 비교하므로 굳이 M_k.processTime에 저장할 필요는 없습니다.
				// (디버깅을 위해 저장한다면 const_cast가 필요할 수 있음, 여기선 지역변수 사용)

				if (curTime >= calculated_processTime)
				{
					// 처리 시간 도달 -> 큐에서 제거 후 로직 실행
					orderData top = orderPq.Pop(0);

					// [추가] 여기서 중복 체크를 하는 것이 가장 깔끔합니다.
					PlayerReplica* sender = nullptr;

					// (플레이어 찾기 로직...)
					for (unsigned int i = 0; i < NetworkManager::Instance()->GetPlayerList().Size(); ++i) {
						if (NetworkManager::Instance()->GetPlayerList()[i]->creatingSystemGUID == top.playerGUID) {
							sender = NetworkManager::Instance()->GetPlayerList()[i];
							break;
						}
					}

					if (sender) {
						// 이미 처리된 액션이면 건너뜀 (No-Op)
						if (top.am_cnt <= sender->lastProcessedAmCnt) {
							continue;
						}
						sender->lastProcessedAmCnt = top.am_cnt; // 최신 번호 갱신
					}
					top.ingoingTime = curTime - top.ingoingTime; // 실제 처리까지 걸린 지연 시간
					BulletHitDetected(top.playerGUID, top.ingoingTime);
				}
				else
				{
					// 아직 처리 시간이 안 됨 -> 정렬된 큐이므로 뒤의 메시지도 처리 불가 -> 루프 종료
					break;
				}
			}
		}

		// -----------------------------------------------------------
		// 4. 게임 종료 및 QoS 처리
		// -----------------------------------------------------------
		if (isGameEnd) {
			if (isServer) {
				RakNet::BitStream bs;
				bs.Write((RakNet::MessageID)CDemo::ID_GAME_MESSAGE_GAME_MATCH);
				bs.Write(GameMatchState::GAME_MATCH_END);
				NetworkManager::Instance()->GetPeer()->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
#if QOS_SUPPORTED
				isQosWritten = false;
#endif
			}
			device->closeDevice();
		}

#if QOS_SUPPORTED 
		if (isServer && isQosWritten && QosWriteTime != 0 && QosWriteTime <= curTime) {
			WriteQoSInfo();
			QosWriteTime = curTime + QOS_WRITE_COOL_TIME;
		}
#endif
	}
}

