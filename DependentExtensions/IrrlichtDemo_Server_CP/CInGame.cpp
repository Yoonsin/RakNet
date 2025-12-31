
// This is a Demo of the Irrlicht Engine (c) 2005-2009 by N.Gebhardt.
// This file is not documented.



#include "CInGame.h"
#if QOS_SUPPORTED
#include <string>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>
#include "shared_mem.h"
using namespace std;

CSharedMemory write_shm;
CSemaphore sem;
#endif

#include "InputController.h"
#include "PacketHandler.h"
#include "HUDManager.h"
#include "SceneManager.h"
#include "MethodManager.h"

// RakNet includes
#include "GetTime.h"
#include "MessageIdentifiers.h"
#include "RakNetTypes.h"
#include "Itoa.h"
#include "RakNetSmartPtr.h"
#include "RandSync.h"
#include <chrono>
#include <set>
#ifdef __ANDROID__
#include "android_tools.h"
#include <sys/auxv.h>
#endif

RakNet::RakNetRandom gRand;

CInGame* CInGame::instance = nullptr;

CInGame* CInGame::Instance() {
	if (instance == nullptr) instance = new CInGame();
	return instance;
}

void CInGame::DestroyInstance() {
	if (instance) {
		delete instance;
		instance = nullptr;
	}
}

CInGame::CInGame(){}

CInGame::CInGame(bool f, bool m, bool s, bool a, bool v, bool fsaa, video::E_DRIVER_TYPE d, core::stringw& _playerName, bool isS, GamePlatform plat, bool isLog, int logCnt, const char* base, bool _isBot, int _winScore, int methodMask)
{
#ifdef __ANDROID__
	mediaPath = "media/"; //"irrlicht/media/";
#else
#ifdef _WIN32
	mediaPath = "C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/IrrlichtMedia/";
#else
	mediaPath = "../../../src/IrrlichtMedia/";
#endif //_WIN32
#endif //__ANDROID__
	
	device = 0;
	bulletCount = 0;
	gameStartTime = 0;
	BOT_MOVE_TIME = 5000;
	playerName = _playerName;
	gamePlatform = plat;
	baseDir = base;
	isBot = _isBot;
	winScore = _winScore;
	isBulletRendering = false;
	isShoot = false;
	isGameStart = false;
	isGameEnd = false;
	
	SceneManager::Instance()->Initialize(f,m,s,a,v,fsaa,d); 
	NetworkManager::Instance()->Initialize();
	NetLogManager::Instance()->Initialize(isLog, logCnt); 
	MethodManager::Instance()->Initialize(methodMask); 
	HUDManager::Instance()->Initialize(); 
}

CInGame::~CInGame()
{
}

void CInGame::run()
{
	auto duration = std::chrono::system_clock::now().time_since_epoch();
	auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
	InitRandom(millis);

	// RakNet startup
	NetworkManager::Instance()->Activate();
	SceneManager::Instance()->Activate();
	SceneManager::Instance()->CalculateSyndeyBoundingBox();
	while (device->run())
	{
		update();
		SceneManager::Instance()->Update(); //load next scene if necessary
		PacketHandler::Instance()->Update();
		// 1초마다 네트워크 통계 로그 출력 (Method 3가 활성화된 경우)
		if (GetGamePlatform() == Server && MethodManager::Instance()->IsMethodActive(3))MethodManager::Instance()->UpdateMethod(3);
	}

	// RakNet shutdown
	NetworkManager::Instance()->Shutdown();
	NetworkManager::DestroyInstance();
	device->drop();
}

void CInGame::update() {
	if (isGameStart && !MethodManager::Instance()->IsMethodActive(2)) MoveBot();
	if (isShoot) { shoot(); isShoot = false; }
}

void CInGame::shoot()
{
	if (NetworkManager::Instance()->GetPlayerReplica()==nullptr || NetworkManager::Instance()->GetPlayerReplica()->IsDead())
		return;

	NetworkManager::Instance()->GetPlayerReplica()->shootCnt++;
	HUDManager::Instance()->SetPlayerNameText();

	scene::ISceneManager* sm = device->getSceneManager();
	scene::ICameraSceneNode* camera = sm->getActiveCamera();
	core::vector3df camPosition = camera->getPosition();
	core::vector3df camAt = (camera->getTarget() - camPosition);
	camAt.normalize();

	BallReplica *br = new BallReplica;
	br->demo=this;
	br->position=camPosition;
	br->shotDirection=camAt;
	br->shotLifetime=RakNet::GetTimeMS() + SceneManager::Instance()->shootFromOrigin(camPosition, camAt, gamePlatform);

	NetworkManager::Instance()->GetReplicaManager()->Reference(br);
}

void CInGame::MoveBot()
{
	//Bot movement
	RakNet::TimeMS t = RakNet::GetTimeMS();
	if (isBot && NetworkManager::Instance()->GetPlayerBotReplica()) {
		if (!NetworkManager::Instance()->GetPlayerBotReplica()->IsDead() && !NetworkManager::Instance()->GetPlayerBotReplica()->wasDead && device->getSceneManager()->getActiveCamera()) {
			SEvent botKeyEvent;
			botKeyEvent.EventType = EET_KEY_INPUT_EVENT;
			botKeyEvent.KeyInput.Key = KEY_KEY_W;
			if (botMoveTime >= t) {
				if (dir != 0 && MethodManager::Instance()->isMethodZero()) {
					
					botKeyEvent.KeyInput.PressedDown =  true;
					InputController::Instance()->SetKeyDown(botKeyEvent.KeyInput.Key, true);
					device->getSceneManager()->getActiveCamera()->OnEvent(botKeyEvent);
				}
			}
			else {
				if (dir != 0 && MethodManager::Instance()->isMethodZero()) {
					botKeyEvent.KeyInput.PressedDown = false;
					InputController::Instance()->SetKeyDown(botKeyEvent.KeyInput.Key, false);
					device->getSceneManager()->getActiveCamera()->OnEvent(botKeyEvent);
				}
				
				SetResetBot();
				Respawn(NetworkManager::Instance()->GetPlayerBotReplica()->respawnPos, NetworkManager::Instance()->GetPlayerBotReplica()->respawnTarget);
				botMoveTime = t + BOT_MOVE_TIME;

				PacketHandler::Instance()->SendRespawnPacket();
			}
		}
	}
	preT = t;
}
void CInGame::SetTransformCamera(scene::ICameraSceneNode* camera, GamePlatform platform) {
	switch (platform)
	{
	case GamePlatform::Holder: {
		initPos = core::vector3df(-586.961609, 217.020020, -285.148346);
		initTarget = core::vector3df(99.891418, 183.134460, -289.131927);
	}
		break;
	case GamePlatform::Shooter: {
		initPos = core::vector3df(-586.961609, 217.020020, -285.148346);
		initTarget = core::vector3df(99.891418, 183.134460, -289.131927);
	}
		  break;
	case GamePlatform::Server: {
		initPos = core::vector3df(-118.683563, 224.552368, -493.077454);
		initTarget = core::vector3df(-118.502869, 229.367813, 61.550100);
	}
		  break;
	default:
		break;
	}

	//봇이면 봇 지정위치로 (임시)
	if (isBot) {
		initPos = core::vector3df(-118.683563, 224.552368, -493.077454);
		initTarget = core::vector3df(-118.502869, 229.367813, 61.550100);
	}

	camera->setPosition(initPos);
	camera->setTarget(initTarget);
}
void CInGame::SetResetBot() {
	if (!NetworkManager::Instance()->GetPlayerBotReplica()) return;

	//Direction
	//-1 : Left, 0 : Down , 1 : Right
	//그 외 : 고정위치
	dir = RandomInt(-1, 1);
	if (MethodManager::Instance()->IsMethodActive(1)) dir = 2;
	if (MethodManager::Instance()->IsMethodActive(2)) dir = 3;

	//Spawn Postion
	switch (dir)
	{
	case -1: {
		NetworkManager::Instance()->GetPlayerBotReplica()->respawnPos = core::vector3df(-118.683563, 224.552368, -493.077454);
		NetworkManager::Instance()->GetPlayerBotReplica()->respawnTarget = core::vector3df(-118.502869, 229.367813, 61.550100);
	}break;
	case 0: {
		NetworkManager::Instance()->GetPlayerBotReplica()->respawnPos = RandomVector3(core::vector3df(-46.856121, 363.129883, -292.425385), core::vector3df(-46.856121, 363.129883 + 700, -292.425385));
		NetworkManager::Instance()->GetPlayerBotReplica()->respawnTarget = core::vector3df(-518.377686, 332.969666, -276.827545);
	}
		  break;
	case 1: {
		NetworkManager::Instance()->GetPlayerBotReplica()->respawnPos = core::vector3df(-86.982430, 217.035385, -140.506424);
		NetworkManager::Instance()->GetPlayerBotReplica()->respawnTarget = core::vector3df(-87.819504, 224.415527, -413.191589);
	}
		  break;
	case 2: {
		if (MethodManager::Instance()->eval1bool) {
			//떨어진 곳
			NetworkManager::Instance()->GetPlayerBotReplica()->respawnPos = core::vector3df(-118.683563, 224.552368, -493.077454);
			NetworkManager::Instance()->GetPlayerBotReplica()->respawnTarget = core::vector3df(-118.502869, 229.367813, 61.550100);
		}
		else {
			//본래 위치
			NetworkManager::Instance()->GetPlayerBotReplica()->respawnPos = core::vector3df(-46.856121, 217.035385, -292.425385);
			NetworkManager::Instance()->GetPlayerBotReplica()->respawnTarget = core::vector3df(-518.377686, 332.969666, -276.827545);
		}
		MethodManager::Instance()->eval1bool = !MethodManager::Instance()->eval1bool;
	}
		break;
	case 3: {
		//원래 죽었던 곳에서 부활
		NetworkManager::Instance()->GetPlayerBotReplica()->respawnPos = NetworkManager::Instance()->GetPlayerBotReplica()->position;
		NetworkManager::Instance()->GetPlayerBotReplica()->respawnTarget = core::vector3df(-518.377686, 332.969666, -276.827545);
	}
		break;
	default:
		break;
	}

	//메소드 2같은 경우는 고정 값으로
	if (MethodManager::Instance()->IsMethodActive(2)) return;
	if (SceneManager::Instance()->fpsCamAnim) SceneManager::Instance()->fpsCamAnim->setMoveSpeed(RandomFloat(0.1f, 0.5f));

	//Gravity
	float gravity = -10.0f;
	if (dir == 0 ) gravity = RandomFloat(-100.f, -10.f);
	if (dir == 2) gravity = 0;
	if (SceneManager::Instance()->fpsCamResponse) SceneManager::Instance()->fpsCamResponse->setGravity(core::vector3df(0, gravity, 0));
}
void CInGame::Respawn(core::vector3df& pos, core::vector3df& target)
{
	if (!(GetSceneManager()->getActiveCamera())) return;

	GetSceneManager()->getActiveCamera()->setPosition(pos);
	GetSceneManager()->getActiveCamera()->setTarget(target);
	
	if(SceneManager::Instance()->fpsCamAnim) SceneManager::Instance()->fpsCamAnim->setReset(true);
	if(SceneManager::Instance()->fpsCamResponse) SceneManager::Instance()->fpsCamResponse->setReset(true);
}

#if QOS_SUPPORTED 
void CInGame::WriteQoSInfo() {
	//Port / UserListCnt / MethodMask /UserData (IP Address, Platform, RTT, FPS ) 
	sem.waitSemaphore();
	write_shm.clearSharedMemory();

	const int PORT = SERVER_PORT;
	string data = "";
	data += std::to_string(PORT) + std::string("|");
	data += std::to_string(NetworkManager::Instance()->GetPlayerReplica()::playerList.Size() - 1) + std::string("|");
	data += std::to_string(evalMask) + std::string("*"); //서버 자신 제외

	//port|userCnt*UserData*UserData*UserData...
	//UserData = IP/Platform/RTT(AveragePing)/ LastPing(LastPing)/2 /FPS
	//20123|1*192.168.1.3/PC/3/144
	for (int idx = 0; idx <NetworkManager::Instance()->GetPlayerList().Size(); ++idx)
	{
		NetworkManager::Instance()->GetPlayerReplica()* player =NetworkManager::Instance()->GetPlayerList()[idx];
		if (player->creatingSystemGUID == NetworkManager::Instance()->GetPeer()->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))continue;
		
		RakNet::SystemAddress addr = NetworkManager::Instance()->GetPeer()->GetSystemAddressFromGuid(player->creatingSystemGUID);
		data += addr.ToString(false) + std::string("/");
		data += player->gamePlatform == Shooter ? "PC" : (player->gamePlatform == Holder ? "M" : "S"); 
		data += std::string("/");
		data += std::to_string(NetworkManager::Instance()->GetPeer()->GetAveragePing(player->creatingSystemGUID)) + std::string("/");
		data += std::to_string(NetworkManager::Instance()->GetPeer()->GetLastPing(player->creatingSystemGUID)) + std::string("/");
		data += std::to_string(player->fps);
		if (idx !=NetworkManager::Instance()->GetPlayerList().Size() - 1) data += std::string("*");

		NetworkManager::Instance()->GetPeer()->Ping(addr); //RTT의 빠른 갱신을 위한 핑 요청
	}

	write_shm.copyToSharedMemory((char*)(data.c_str()));
	sem.releaseSemaphore();
}

unsigned long CInGame::get_nsecs()
{
	//unsigned long now = get_nsecs(); or uint64_t now = get_nsecs();
	//alternative to bpf_ktime_get_ns (ref : https://stackoverflow.com/questions/60970877/xdp-bpf-is-there-an-user-space-alternative-to-bpf-ktime-get-ns)

	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000000000UL + ts.tv_nsec;
}

#endif

