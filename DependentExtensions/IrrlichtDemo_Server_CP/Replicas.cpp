#include "Replicas.h"
#include "NetworkManager.h"
#include "CDemo.h"
#include "GetTime.h"

BaseIrrlichtReplica::BaseIrrlichtReplica()
{
}
BaseIrrlichtReplica::~BaseIrrlichtReplica()
{

}
void BaseIrrlichtReplica::SerializeConstruction(RakNet::BitStream* constructionBitstream, RakNet::Connection_RM3* destinationConnection)
{
	constructionBitstream->Write(position);
}
bool BaseIrrlichtReplica::DeserializeConstruction(RakNet::BitStream* constructionBitstream, RakNet::Connection_RM3* sourceConnection)
{
	constructionBitstream->Read(position);
	return true;
}
RM3SerializationResult BaseIrrlichtReplica::Serialize(RakNet::SerializeParameters* serializeParameters)
{
	return RM3SR_BROADCAST_IDENTICALLY;
}
void BaseIrrlichtReplica::Deserialize(RakNet::DeserializeParameters* deserializeParameters)
{
}
void BaseIrrlichtReplica::Update(RakNet::TimeMS curTime)
{
}

PlayerReplica::PlayerReplica()
{
	model = 0;
	rotationDeltaPerMS = 0.0f;
	isMoving = false;
	deathTimeout = 0;
	lastUpdate = bulletCoolTime = RakNet::GetTimeMS();
	isDead = false;
	wasDead = false;
	isBot = false;
	isTeleport = false;
	NetworkManager::Instance()->AddPlayer(this);
	fq = new DataStructures::Queue<FrameState>();
	killCnt = 0;
	deathCnt = 0;
	shootCnt = 0;
	nextExpectedAmCnt = 1;
}
PlayerReplica::~PlayerReplica()
{
	NetworkManager::Instance()->RemovePlayer(this);
	if (NetworkManager::Instance()->GetTopology() == SERVER) delete fq;
	debugBox = nullptr;
}
RakNet::RM3ConstructionState PlayerReplica::QueryConstruction(RakNet::Connection_RM3* destinationConnection, RakNet::ReplicaManager3* replicaManager3) {
	return QueryConstruction_ClientConstruction(destinationConnection, NetworkManager::Instance()->GetTopology() != CLIENT);
}
bool PlayerReplica::QueryRemoteConstruction(RakNet::Connection_RM3* sourceConnection) {
	return QueryRemoteConstruction_ClientConstruction(sourceConnection, NetworkManager::Instance()->GetTopology() != CLIENT);
}
RakNet::RM3QuerySerializationResult PlayerReplica::QuerySerialization(RakNet::Connection_RM3* destinationConnection) {
	return QuerySerialization_ClientSerializable(destinationConnection, NetworkManager::Instance()->GetTopology() != CLIENT);
}
RakNet::RM3ActionOnPopConnection PlayerReplica::QueryActionOnPopConnection(RakNet::Connection_RM3* droppedConnection) const { return QueryActionOnPopConnection_Client(droppedConnection); }
void PlayerReplica::WriteAllocationID(RakNet::Connection_RM3* destinationConnection, RakNet::BitStream* allocationIdBitstream) const
{
	allocationIdBitstream->Write(RakNet::RakString("PlayerReplica"));
}
void PlayerReplica::SerializeConstruction(RakNet::BitStream* constructionBitstream, RakNet::Connection_RM3* destinationConnection)
{
	BaseIrrlichtReplica::SerializeConstruction(constructionBitstream, destinationConnection);
	constructionBitstream->Write(rotationAroundYAxis);
	constructionBitstream->Write(gamePlatform);
	constructionBitstream->Write(demo->GetDevice()->getVideoDriver()->getFPS());
}
bool PlayerReplica::DeserializeConstruction(RakNet::BitStream* constructionBitstream, RakNet::Connection_RM3* sourceConnection)
{
	if (!BaseIrrlichtReplica::DeserializeConstruction(constructionBitstream, sourceConnection))
		return false;
	constructionBitstream->Read(rotationAroundYAxis);
	constructionBitstream->Read(gamePlatform);
	constructionBitstream->Read(fps);
	//demo->PushMessage(RakNet::RakString("Deserialize Construction"));
	return true;
}
void PlayerReplica::PostDeserializeConstruction(RakNet::BitStream* constructionBitstream, RakNet::Connection_RM3* destinationConnection)
{
	// Object was remotely created and all data loaded. Now we can make the object visible
	scene::IAnimatedMesh* mesh = 0;
	scene::ISceneManager* sm = demo->GetSceneManager();
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
	model->setScale(core::vector3df(2, 2, 2));
	model->setMD2Animation(scene::EMAT_STAND);

	curAnim = scene::EMAT_STAND;
	model->setMaterialTexture(0, demo->GetDevice()->getVideoDriver()->getTexture(IRRLICHT_MEDIA_PATH "sydney.bmp"));
	model->setMaterialFlag(video::EMF_LIGHTING, true);
	model->addShadowVolumeSceneNode();
	model->setAutomaticCulling(scene::EAC_BOX);
	model->setVisible(true);
	model->setAnimationEndCallback(this);
	wchar_t playerNameWChar[1024];
	mbstowcs(playerNameWChar, playerName.C_String(), 1023);
	// ensure wide-character string is null terminated (i.e. if playerName length is >= 1023)
	playerNameWChar[1023] = L'\0';
	scene::IBillboardSceneNode* bb = sm->addBillboardTextSceneNode(0, playerNameWChar, model);
	bb->setSize(core::dimension2df(40, 20));
	bb->setPosition(core::vector3df(0, model->getBoundingBox().MaxEdge.Y + bb->getBoundingBox().MaxEdge.Y - bb->getBoundingBox().MinEdge.Y + 5.0, 0));
	bb->setColor(video::SColor(255, 255, 128, 128), video::SColor(255, 255, 128, 128));
}
void PlayerReplica::PreDestruction(RakNet::Connection_RM3* sourceConnection)
{
	if (model)
		model->remove();
}
RM3SerializationResult PlayerReplica::Serialize(RakNet::SerializeParameters* serializeParameters)
{
	BaseIrrlichtReplica::Serialize(serializeParameters);
	serializeParameters->outputBitstream[0].Write(position);
	serializeParameters->outputBitstream[0].Write(rotationAroundYAxis);
	serializeParameters->outputBitstream[0].Write(isMoving);
	serializeParameters->outputBitstream[0].Write(gamePlatform);

	if (NetworkManager::Instance()->GetTopology() == SERVER) {
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
	serializeParameters->outputBitstream[0].Write((NetworkManager::Instance()->GetTopology() == SERVER) ? fps : demo->GetDevice()->getVideoDriver()->getFPS());

	//timeStamp
	//serializeParameters->messageTimestamp = RakNet::GetTimeMS();
	//return RM3SR_BROADCAST_IDENTICALLY; 
	return RM3SR_BROADCAST_IDENTICALLY_FORCE_SERIALIZATION; //값이 안바뀌어도 계속 동기화됨
}
void PlayerReplica::Deserialize(RakNet::DeserializeParameters* deserializeParameters)
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
	if (creatingSystemGUID == NetworkManager::Instance()->GetPeer()->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))
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
	if (NetworkManager::Instance()->GetTopology() == SERVER) {
		//부활 확인
		bool tmp = IsDead();
		if (wasDead && !tmp) {
			RakNet::BitStream bs;
			bs.Write((RakNet::MessageID)CDemo::ID_GAME_MESSAGE_PLAYER_LIFE);
			bs.Write(creatingSystemGUID);
			bs.Write(tmp);
			wasDead = false;
			NetworkManager::Instance()->GetPeer()->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
			if (isBot) {
				if (creatingSystemGUID == NetworkManager::Instance()->GetPeer()->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))
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

				if (creatingSystemGUID == NetworkManager::Instance()->GetPeer()->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS)) {
					//서버 봇은 바로 브로드 캐스트
					NetworkManager::Instance()->GetPeer()->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
				}
			}
		}
	}

	//Set Animation
	if (creatingSystemGUID != NetworkManager::Instance()->GetPeer()->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS)) {
		if ((NetworkManager::Instance()->GetTopology() == SERVER) ? IsDead() : isDead)
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
	if (NetworkManager::Instance()->GetTopology() == SERVER)
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

		FrameState frame{ curTime - (NetworkManager::Instance()->GetPeer()->GetAveragePing(creatingSystemGUID) / 2), transform, shootPosition, shootDirection };
		fq->Push(frame, _FILE_AND_LINE_);
		//맨 앞에 남아있는 프레임부터 차례대로 검사 -> 현재 시간이랑 1초 이상 차이나면 버림
		while (!fq->IsEmpty() && frame.timeStamp - fq->Peek().timeStamp > HISTORY_DURATION_MS)
			fq->Pop();

		// Is a locally created object?
		// 이동 적용
		if (creatingSystemGUID == NetworkManager::Instance()->GetPeer()->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))
		{
			NetworkManager::Instance()->GetPlayerReplica()->position = demo->GetSceneManager()->getActiveCamera()->getPosition() - irr::core::vector3df(0, CAMERA_HEIGHT, 0);
			NetworkManager::Instance()->GetPlayerReplica()->rotationAroundYAxis = demo->GetSceneManager()->getActiveCamera()->getRotation().Y - 90.0f;

			if (NetworkManager::Instance()->GetPlayerBotReplica()) {
				NetworkManager::Instance()->GetPlayerBotReplica()->botModel->setPosition(NetworkManager::Instance()->GetPlayerBotReplica()->position);
				NetworkManager::Instance()->GetPlayerBotReplica()->botModel->setRotation(core::vector3df(0, NetworkManager::Instance()->GetPlayerBotReplica()->rotationAroundYAxis, 0));
			}

			// Local player has no mesh to interpolate
			// Input our camera position as our player position
			isMoving = demo->IsMovementKeyDown();

			// Ack, makes the screen messed up and the mouse move off the window
			// Find another way to keep the dead player from moving
			// 서버 봇 적용하면 이동시 튕기는 문제 발생
			if (NetworkManager::Instance()->GetTopology() != SERVER) demo->EnableInput((NetworkManager::Instance()->GetTopology() == SERVER) ? IsDead() == false : isDead == false);

			//DebugPrintf("Player position : %f, %f, %f / isKeyLock : %d / wasKeyLock : %d \n", position.X, position.Y, position.Z, demo->isKeyLock, demo->wasKeyLock);
			// DebugPrintf("Player target : %f, %f, %f\n", demo->GetSceneManager()->getActiveCamera()->getTarget().X, demo->GetSceneManager()->getActiveCamera()->getTarget().Y, demo->GetSceneManager()->getActiveCamera()->getTarget().Z);

			if (demo->gamePlatform == Holder) {
				demo->SetHolderPosText(NetworkManager::Instance()->GetPlayerReplica()->position);
			}
		}

		if (creatingSystemGUID == NetworkManager::Instance()->GetPeer()->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS)) return;

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
		RakNet::TimeMS elapsed = curTime - lastUpdate;
		//DebugPrintf("Update curTime : %d, lastUpdate : %d, elapsed time : %d, guid : %llu\n", curTime, lastUpdate, elapsed, creatingSystemGUID.g);
		if (elapsed <= 1)
			return;
		if (elapsed > 100)
			elapsed = 100;

		lastUpdate = curTime;

		irr::core::vector3df curPositionDelta = position - model->getPosition();
		irr::core::vector3df interpThisTick = positionDeltaPerMS * (float)elapsed;
		if (curTime < interpEndTime && interpThisTick.getLengthSQ() < curPositionDelta.getLengthSQ())
		{
			model->setPosition(model->getPosition() + positionDeltaPerMS * (float)elapsed);
			//DebugPrintf("interPolation\n");
		}
		else
		{
			model->setPosition(position);
		}

		float curRotationDelta = GetRotationDifference(rotationAroundYAxis, model->getRotation().Y);
		float interpThisTickRotation = rotationDeltaPerMS * (float)elapsed;
		if (curTime < interpEndTime && fabs(interpThisTickRotation) < fabs(curRotationDelta))
		{
			model->setRotation(model->getRotation() + core::vector3df(0, interpThisTickRotation, 0));
		}
		else
		{
			model->setRotation(core::vector3df(0, rotationAroundYAxis, 0));
		}

		//Print HolderPos
		if (isBot && (demo->evalMask == 0)) {

			//보간된 위치
			demo->SetHolderPosText(model->getPosition());
			core::vector3df pos = model->getPosition();

			if (isDead || bulletCoolTime > RakNet::GetTimeMS() || bulletCoolTime == -1 || NetworkManager::Instance()->GetTopology() == SERVER) {
				return;
			}

			if (isWithinRange(pos.Y, 167.0f, 20.0f) && isWithinRange(pos.Z, -288.0f, 15.0f)) {
				bulletCoolTime = RakNet::GetTimeMS() + BULLET_COOL_TIME;
				demo->isShoot = true;

			}
		}
	}
}
void PlayerReplica::UpdateAnimation(irr::scene::EMD2_ANIMATION_TYPE anim)
{
	if (anim != curAnim && model)
		model->setMD2Animation(anim);
	curAnim = anim;
}
float PlayerReplica::GetRotationDifference(float r1, float r2)
{
	float diff = r1 - r2;
	while (diff > 180.0f)
		diff -= 360.0f;
	while (diff < -180.0f)
		diff += 360.0f;
	return diff;
}
void PlayerReplica::OnAnimationEnd(scene::IAnimatedMeshSceneNode* node)
{
	if (curAnim == scene::EMAT_ATTACK)
	{
		if (isMoving)
		{
			UpdateAnimation(scene::EMAT_RUN);
			if (model)model->setLoopMode(true);
		}
		else
		{
			UpdateAnimation(scene::EMAT_STAND);
			if (model)model->setLoopMode(true);
		}
	}
}
void PlayerReplica::PlayAttackAnimation(void)
{
	if ((NetworkManager::Instance()->GetTopology() == SERVER) ? IsDead() == false : isDead == false)
	{
		UpdateAnimation(scene::EMAT_ATTACK);
		if (model)model->setLoopMode(false);
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
	return QueryConstruction_ClientConstruction(destinationConnection, NetworkManager::Instance()->GetTopology() != CLIENT);
}
bool PlayerBotReplica::QueryRemoteConstruction(RakNet::Connection_RM3* sourceConnection) {
	return QueryRemoteConstruction_ClientConstruction(sourceConnection, NetworkManager::Instance()->GetTopology() != CLIENT);
}
RakNet::RM3QuerySerializationResult PlayerBotReplica::QuerySerialization(RakNet::Connection_RM3* destinationConnection) {
	return QuerySerialization_ClientSerializable(destinationConnection, NetworkManager::Instance()->GetTopology() != CLIENT);
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

	if (NetworkManager::Instance()->GetTopology() == SERVER) {
		serializeParameters->outputBitstream[0].Write(++demo->um_cnt);
		demo->umTimeMap.Set(demo->um_cnt, RakNet::GetTimeMS());
	}

	//TODO : umTimeList size overflow check

	//timeStamp
	//serializeParameters->messageTimestamp = RakNet::GetTimeMS();
	//return RM3SR_BROADCAST_IDENTICALLY; 
	return RM3SR_BROADCAST_IDENTICALLY_FORCE_SERIALIZATION; //값이 안바뀌어도 계속 동기화됨
}
void PlayerBotReplica::Deserialize(RakNet::DeserializeParameters* deserializeParameters)
{
	BaseIrrlichtReplica::Deserialize(deserializeParameters);
	deserializeParameters->serializationBitstream[0].Read(position);
	deserializeParameters->serializationBitstream[0].Read(rotationAroundYAxis);
	deserializeParameters->serializationBitstream[0].Read(isMoving);
	deserializeParameters->serializationBitstream[0].Read(gamePlatform);

	if (NetworkManager::Instance()->GetTopology() != SERVER) {
		deserializeParameters->serializationBitstream[0].Read(demo->um_cnt);
		demo->reactionTime = RakNet::GetTimeMS();
		demo->umReceptionTimes.Set(demo->um_cnt, RakNet::GetTimeMS());
	}

	// Is a locally created object?
	if (creatingSystemGUID == NetworkManager::Instance()->GetPeer()->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))
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
	creationTime = RakNet::GetTimeMS();
}
BallReplica::~BallReplica()
{
}
void BallReplica::WriteAllocationID(RakNet::Connection_RM3* destinationConnection, RakNet::BitStream* allocationIdBitstream) const
{
	allocationIdBitstream->Write(RakNet::RakString("BallReplica"));
}
RakNet::RM3ConstructionState BallReplica::QueryConstruction(RakNet::Connection_RM3* destinationConnection, RakNet::ReplicaManager3* replicaManager3) {
	return QueryConstruction_ClientConstruction(destinationConnection, NetworkManager::Instance()->GetTopology() != CLIENT);
}
bool BallReplica::QueryRemoteConstruction(RakNet::Connection_RM3* sourceConnection) { return QueryRemoteConstruction_ClientConstruction(sourceConnection, NetworkManager::Instance()->GetTopology() != CLIENT); }
RakNet::RM3QuerySerializationResult BallReplica::QuerySerialization(RakNet::Connection_RM3* destinationConnection) { return QuerySerialization_ClientSerializable(destinationConnection, NetworkManager::Instance()->GetTopology() != CLIENT); }
RakNet::RM3ActionOnPopConnection BallReplica::QueryActionOnPopConnection(RakNet::Connection_RM3* droppedConnection) const { return QueryActionOnPopConnection_Client(droppedConnection); }
void BallReplica::SerializeConstruction(RakNet::BitStream* constructionBitstream, RakNet::Connection_RM3* destinationConnection)
{
	BaseIrrlichtReplica::SerializeConstruction(constructionBitstream, destinationConnection);
	constructionBitstream->Write(shotDirection);
	constructionBitstream->Write(bulletCount);

	if ((demo->evalMask & METHOD_2) == 0) return;
	if (NetworkManager::Instance()->GetTopology() == SERVER) return;

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


	if (creatingSystemGUID != NetworkManager::Instance()->GetPeer()->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))
	{
		RakNet::TimeMS estimatedElapsed = NetworkManager::Instance()->GetPeer()->GetAveragePing(sourceConnection->GetSystemAddress()) / 2;
		position = position + shotDirection * (float)estimatedElapsed * SHOT_SPEED;
	}

	if (NetworkManager::Instance()->GetTopology() != SERVER || (demo->evalMask & METHOD_2) == 0) return true;

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
		for (unsigned int p_idx = 0; p_idx < NetworkManager::Instance()->GetPlayerList().Size(); ++p_idx) {
			if (NetworkManager::Instance()->GetPlayerList()[p_idx]->creatingSystemGUID == data.playerGUID) {
				senderPlayer = NetworkManager::Instance()->GetPlayerList()[p_idx];
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
void BallReplica::PostDeserializeConstruction(RakNet::BitStream* constructionBitstream, RakNet::Connection_RM3* destinationConnection)
{

	// Shot visible effect and BallReplica classes are not linked, but they update the same way, such that
	// they are in the same spot all the time
	// effect
	if (NetworkManager::Instance()->GetTopology() != SERVER) {
		GamePlatform platform = GamePlatform::Server;
		for (int idx = 0; idx < NetworkManager::Instance()->GetPlayerList().Size(); ++idx)
		{
			auto* player = NetworkManager::Instance()->GetPlayerList()[idx];
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

	demo->BulletHitDetected(creatingSystemGUID, 0);

	// Hit Scan Only
	//core::vector3df start = position;
	//core::vector3df end = start + (shotDirection * camera->getFarValue());
	//core::line3d<irr::f32> line(start, end);
	//core::triangle3df triangle;
	//core::vector3df hitPoint;
	//const scene::ISceneNode* hitNode;
	//
	//for (idx = 0; idx < NetworkManager::Instance()->GetPlayerList().Size(); ++idx)
	//{
	//	auto* player = NetworkManager::Instance()->GetPlayerList()[idx];
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
	//			NetworkManager::Instance()->GetPeer()->GetSystemAddressFromGuid(creatingSystemGUID).ToString(true));
	//		demo->PushMessage(msg);
	//		//OutputDebugStringA(msg.C_String());
	//		
	//		RakNet::BitStream bs;
	//		bs.Write((RakNet::MessageID)CDemo::ID_GAME_MESSAGE_PLAYER_LIFE);
	//		bs.Write(player->creatingSystemGUID);
	//		bs.Write(player->IsDead());
	//		player->wasDead = true;

	//		NetworkManager::Instance()->GetPeer()->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
	//		break; // 더 검사하지 않음
	//	}
	//}
}
void BallReplica::PreDestruction(RakNet::Connection_RM3* sourceConnection)
{
	// The system that shot this ball destroyed it, or disconnected
	// Technically we should clear out the node visible effect too, but it's not important for now
}
RM3SerializationResult BallReplica::Serialize(RakNet::SerializeParameters* serializeParameters)
{
	BaseIrrlichtReplica::Serialize(serializeParameters);
	return RM3SR_BROADCAST_IDENTICALLY;
}
void BallReplica::Deserialize(RakNet::DeserializeParameters* deserializeParameters)
{
	BaseIrrlichtReplica::Deserialize(deserializeParameters);
}
void BallReplica::Update(RakNet::TimeMS curTime)
{
	// Is a locally created object?
	if (creatingSystemGUID == NetworkManager::Instance()->GetPeer()->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))
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
	//if (creatingSystemGUID != NetworkManager::Instance()->GetPeer()->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))
	//{
	//	bool check = (NetworkManager::Instance()->GetTopology() == SERVER && NetworkManager::Instance()->GetPlayerBotReplica()->IsDead() == false) || (NetworkManager::Instance()->GetTopology() == CLIENT && NetworkManager::Instance()->GetPlayerReplica()->IsDead() == false);
	//	if (check) {
	//		float playerHalfHeight = demo->GetSyndeyBoundingBox().getExtent().Y / 2;
	//		irr::core::vector3df positionRelativeToCharacter = updatedPosition - ((NetworkManager::Instance()->GetTopology() == SERVER) ?NetworkManager::Instance()->GetPlayerBotReplica()->position : NetworkManager::Instance()->GetPlayerReplica()->position);//+core::vector3df(0,playerHalfHeight,0);
	//		if (demo->GetSyndeyBoundingBox().isPointInside(positionRelativeToCharacter))
	//			//if ((NetworkManager::Instance()->GetPlayerReplica()->position+core::vector3df(0,playerHalfHeight,0)-updatedPosition).getLengthSQ() < BALL_DIAMETER*BALL_DIAMETER/4.0f)
	//		{
	//			// We're dead for 3 seconds
	//			if (NetworkManager::Instance()->GetTopology() == SERVER) {
	//				NetworkManager::Instance()->GetPlayerBotReplica()->deathTimeout = curTime + 3000;
	//				demo->PushMessage(RakNet::RakString("Bot Dead from : ") + NetworkManager::Instance()->GetPeer()->GetSystemAddressFromGuid(creatingSystemGUID).ToString(true));

	//			}else NetworkManager::Instance()->GetPlayerReplica()->deathTimeout = curTime + 3000;
	//		}
	//	}
	//}
}

RakNet::Replica3* Connection_RM3Irrlicht::AllocReplica(RakNet::BitStream* allocationId, ReplicaManager3* replicaManager3)
{
	RakNet::RakString typeName; allocationId->Read(typeName);
	if (typeName == "PlayerReplica") { BaseIrrlichtReplica* r = new PlayerReplica; r->demo = demo; return r; }
	if (typeName == "PlayerBotReplica") { BaseIrrlichtReplica* r = new PlayerBotReplica; r->demo = demo; return r; }
	if (typeName == "BallReplica") { BaseIrrlichtReplica* r = new BallReplica; r->demo = demo; return r; }
	return 0;
}
