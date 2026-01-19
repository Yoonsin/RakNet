
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

using namespace RakNet;
using namespace irr;

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

CInGame::CInGame(bool f, bool m, bool s, bool a, bool v, bool fsaa, video::E_DRIVER_TYPE d, core::stringw& _playerName, bool isS, GamePlatform plat, bool isLog, int ClientCnt, const char* base, bool _isBot, int _winScore, int methodMask, int methodLogMask, int scenario, bool isLocalS)
{
	if (instance == nullptr) instance = this;

	device = 0;
	bulletCount = 0;
	gameStartTime = 0;
	BOT_MOVE_TIME = 1000;
	playerName = _playerName;
	gamePlatform = plat;
	isBot = _isBot;
	winScore = _winScore;
	isBulletRendering = false;
	isShoot = false;
	isGameStart = false;
	isGameEnd = false;
	driverType = d;
	if (driverType == video::EDT_NULL) isDummy = true;
	
	SceneManager::Instance()->Initialize(f,m,s,a,v,fsaa,d); 
	NetworkManager::Instance()->Initialize(isS, isLocalS, ClientCnt);
	NetLogManager::Instance()->Initialize(isLog, 30000, base); 
	MethodManager::Instance()->Initialize(methodMask, methodLogMask, scenario, isS); 
	HUDManager::Instance()->Initialize(); 
}

CInGame::~CInGame()
{
}

void CInGame::Activate()
{
#ifdef __ANDROID__
	video::E_DRIVER_TYPE driverType = video::EDT_OGLES2;
	irr::android::SDisplayMetrics displayMetrics;
	memset(&displayMetrics, 0, sizeof displayMetrics);
	irr::android::getDisplayMetrics(state, displayMetrics);
	SIrrlichtCreationParameters param;
	param.DriverType = driverType;				// android:glEsVersion in AndroidManifest.xml should be "0x00020000"
	param.WindowSize = core::dimension2d<u32>(displayMetrics.widthPixels, displayMetrics.heightPixels);	// using 0,0 it will automatically set it to the maximal size
	param.PrivateData = state;
	param.Bits = 24;
	param.ZBufferBits = 16;
	param.AntiAlias = 0;
	param.EventReceiver = InputController::Instance();
	device = createDeviceEx(param);

#else
	core::dimension2d<u32> resolution(640, 480);
	irr::SIrrlichtCreationParameters params;
	params.DriverType = driverType;
	params.WindowSize = resolution;
	params.Bits = 32;
	params.Fullscreen = SceneManager::Instance()->fullscreen;
	params.Stencilbuffer = SceneManager::Instance()->shadows;
	params.Vsync = SceneManager::Instance()->vsync;
	params.AntiAlias = SceneManager::Instance()->aa;
	params.EventReceiver = InputController::Instance();
	device = createDeviceEx(params);
#endif //__ANDROID__

	//Android���� MIP_MAPS�� ���� ������ ����ũ �� ��ü�� �˰Ժ���
	device->getVideoDriver()->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, false);
	device->setWindowCaption(L"Irrlicht Engine Demo");
	device->getSceneManager()->setAmbientLight(video::SColorf(0x00c0c0c0)); // set ambient light

	//array = 0xb4000072facc9d90 "../../../IrrlichtMedia/sydney.md2"
#ifdef _WIN32
	mediaPath = "C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/IrrlichtMedia/";
#elif __ANDROID__
	mediaPath = "media/"; //"irrlicht/media/";
#else
	mediaPath = "../../../../IrrlichtMedia/";
#endif //_WIN32

#ifdef __ANDROID__
	io::IFileSystem* fs = device->getFileSystem();
    video::IVideoDriver* driver = device->getVideoDriver();
	for (u32 i = 0; i < fs->getFileArchiveCount(); ++i)
	{
		io::IFileArchive* archive = fs->getFileArchive(i);
		if (archive->getType() == io::E_FILE_ARCHIVE_TYPE::EFAT_ANDROID_ASSET)
		{
			archive->addDirectoryToFileList(mediaPath);
			break;
		}
	}

	//Android GUI Extension 
	gui::IGUIEnvironment* guienv = device->getGUIEnvironment();
	core::rect<irr::s32> winRect(0, 0, 9 * displayMetrics.widthPixels / 10, 9 * displayMetrics.heightPixels / 10);
	Drawer2D* drawer = new Drawer2D(device);
	AppSkin* skin = new AppSkin(device, drawer);
	assert(isExtendableSkin(skin));
	guienv->setSkin(skin);
	skin->drop();
	int offset = 150;
	core::rect<s32> testArea3(20, winRect.getHeight() / 2 + 20 + offset, winRect.getWidth() / 4, winRect.getHeight() + offset);
	bool scrollable = false; bool horizontal = false;
	
	AggregateGUIElement* a3 = new AggregateGUIElement(guienv, 1.f, 1.f, 1.f, 1.f, true, horizontal, scrollable, {
		new JoyStickElement(drawer, guienv, driver->getTexture("media/joy_background.png"), driver->getTexture("media/joy_handle.png"),1.f,true,  AppSkin::DEFAULT_AGGREGATABLE, video::SColor(255,255,255,255),  static_cast<void*>(&(InputController::Instance()->isKeyLock))) },
		{}, false, AppSkin::REGULAR_AGGREGATION, NULL, NULL, testArea3);
#endif //__ANDROID__

	if (device->getFileSystem()->existFile("irrlicht.dat")) device->getFileSystem()->addFileArchive("irrlicht.dat", true, true, io::EFAT_ZIP);
	else device->getFileSystem()->addFileArchive(mediaPath + "irrlicht.dat", true, true, io::EFAT_ZIP);
	if (device->getFileSystem()->existFile("map-20kdm2.pk3")) device->getFileSystem()->addFileArchive("map-20kdm2.pk3", true, true, io::EFAT_ZIP);
	else device->getFileSystem()->addFileArchive(mediaPath + "map-20kdm2.pk3", true, true, io::EFAT_ZIP);
}

void CInGame::ShutDown() {
	NetLogManager::Instance()->Shutdown();
	NetLogManager::DestroyInstance();
	MethodManager::DestroyInstance();
	NetworkManager::Instance()->Shutdown();
	NetworkManager::DestroyInstance();
	CollisionManager::DestroyInstance();
	InputController::DestroyInstance();
	PacketHandler::DestroyInstance();
	HUDManager::DestroyInstance();
	SceneManager::DestroyInstance();
	device->drop();
	device = nullptr;
}

void CInGame::Run()
{
	auto duration = std::chrono::system_clock::now().time_since_epoch();
	auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
	InitRandom(millis);

	Activate();
	HUDManager::Instance()->Activate();
	NetworkManager::Instance()->Activate(); // RakNet startup
	SceneManager::Instance()->Activate();
	SceneManager::Instance()->CalculateSyndeyBoundingBox();
	while (device->run())
	{
		Update();
		SceneManager::Instance()->Update(); //load next scene if necessary
		NetworkManager::Instance()->Update();
		PacketHandler::Instance()->Update();
		// 1�ʸ��� ��Ʈ��ũ ��� �α� ��� (Method 3�� Ȱ��ȭ�� ���)
		if (GetGamePlatform() == Server && MethodManager::Instance()->IsMethodActive(METHOD_3)) MethodManager::Instance()->UpdateMethod(METHOD_3);
	}
	ShutDown();
}

void CInGame::Update() {
	if (isGameStart && MethodManager::Instance()->scenarioNum == 1) MoveBot();
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

				RakNet::BitStream bs; 
				PacketHandler::Instance()->MakeRespawnPacket(&bs);
				if (bs.GetNumberOfBytesUsed() > 0) {
					if (NetworkManager::Instance()->IsServer() && MethodManager::Instance()->IsMethodActive(METHOD_1) == true) MethodManager::Instance()->SendManagedPacket(&bs, RakNet::UNASSIGNED_SYSTEM_ADDRESS, METHOD_1);
					else NetworkManager::Instance()->GetPeer()->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
				}
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

	//���̸� �� ������ġ�� (�ӽ�)
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
	//�� �� : ������ġ
	dir = RandomInt(-1, 1);
	if (MethodManager::Instance()->IsMethodActive(METHOD_1)) dir = 2;
	if (MethodManager::Instance()->IsMethodActive(METHOD_2)) dir = 3;

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
		if (MethodManager::Instance()->method1bool) {
			//������ ��
			NetworkManager::Instance()->GetPlayerBotReplica()->respawnPos = core::vector3df(-118.683563, 224.552368, -493.077454);
			NetworkManager::Instance()->GetPlayerBotReplica()->respawnTarget = core::vector3df(-118.502869, 229.367813, 61.550100);
		}
		else {
			//���� ��ġ
			NetworkManager::Instance()->GetPlayerBotReplica()->respawnPos = core::vector3df(-46.856121, 217.035385, -292.425385);
			NetworkManager::Instance()->GetPlayerBotReplica()->respawnTarget = core::vector3df(-518.377686, 332.969666, -276.827545);
		}
		MethodManager::Instance()->method1bool = !MethodManager::Instance()->method1bool;
	}
		break;
	case 3: {
		//���� �׾��� ������ ��Ȱ
		NetworkManager::Instance()->GetPlayerBotReplica()->respawnPos = NetworkManager::Instance()->GetPlayerBotReplica()->position;
		NetworkManager::Instance()->GetPlayerBotReplica()->respawnTarget = core::vector3df(-518.377686, 332.969666, -276.827545);
	}
		break;
	default:
		break;
	}

	//�޼ҵ� 2���� ���� ���� ������
	if (MethodManager::Instance()->IsMethodActive(METHOD_2)) return;
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
	data += std::to_string(evalMask) + std::string("*"); //���� �ڽ� ����

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
		NetworkManager::Instance()->GetPeer()->Ping(addr); //RTT�� ���� ������ ���� �� ��û
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

