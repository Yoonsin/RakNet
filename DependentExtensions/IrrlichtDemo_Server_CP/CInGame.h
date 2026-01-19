#pragma once
#ifndef __C_DEMO_H_INCLUDED__
#define __C_DEMO_H_INCLUDED__

#include <irrlicht.h>
#ifdef __ANDROID__
#define IRRLICHT_MEDIA_PATH  "media/"
#include <android/native_activity.h>
#include "android_native_app_glue.h"
#else
#ifdef _WIN32
#define QOS_SUPPORTED 0
#define IRRLICHT_MEDIA_PATH  "C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/IrrlichtMedia/" //"../../media/"
#include "WindowsIncludes.h" // Prevent 'fd_set' : 'struct' type redefinition
#include <windows.h>
#else
#if defined(__linux__)
#define IRRLICHT_MEDIA_PATH  "../../../src/IrrlichtMedia/"
#define QOS_SUPPORTED 0
const int QOS_WRITE_COOL_TIME = 500;
#else
#define QOS_SUPPORTED 0
#endif
#endif // _WIN32
#endif // __ANDROID__

#define RandomFloat(min, max) RandomFloatImpl(min, max)
#define RandomInt(min, max)   RandomIntImpl(min, max)
#define RandomVector3(min, max) RandomVector3Impl(min, max)

using namespace irr;
const int CAMERA_COUNT = 7;
const int BULLET_COOL_TIME = 500;
const int GAME_START_PENDING_TIME = 5000;
const float CAMERA_HEIGHT=50.0f;
const float SHOT_SPEED = 5.0f; //.6f;
const float BALL_DIAMETER=20.0f;

// RakNet
#include "NetworkManager.h"
#include "NetLogManager.h"
#include "Replicas.h"
#include "MessageIdentifiers.h"
#include "DS_OrderedList.h"
#include "DS_Heap.h"
#include "DS_Map.h"
#include "RakString.h"
#include "RakNetTime.h"
#include "Rand.h"

extern RakNet::RakNetRandom gRand;

inline void InitRandom(unsigned int t) {
	// �õ带 �ð� ������� �ָ� �Ź� �ٸ� ������
	gRand.SeedMT(t);
}

inline float RandomFloatImpl(float min, float max) {
	return min + gRand.FrandomMT() * (max - min);
}

inline int RandomIntImpl(int min, int max) {
	return min + (int)(gRand.RandomMT() % (uint32_t)(max - min + 1));
}

inline irr::core::vector3df RandomVector3Impl(
	const irr::core::vector3df& min,
	const irr::core::vector3df& max)
{
	return irr::core::vector3df(
		RandomFloatImpl(min.X, max.X),
		RandomFloatImpl(min.Y, max.Y),
		RandomFloatImpl(min.Z, max.Z)
	);
}

enum GameMatchState {
	GAME_MATCH_NONE = 0,
	GAME_MATCH_PENDING = 1,
	GAME_MATCH_START = 2,
	GAME_MATCH_END = 3,
};

class CInGame
{
public:
	static CInGame* Instance();
	static void DestroyInstance();
	CInGame();
	CInGame(bool fullscreen, bool music, bool shadows, bool additive, bool vsync, bool aa, video::E_DRIVER_TYPE driver, core::stringw &_playerName, bool isServer, GamePlatform platform, bool isLogged, int logCnt, const char* base, bool isBot, int winScore,int methodMask, int methodLogMask, int scenario, bool isLocalS);
	~CInGame();
	void Run();
	void Activate();
	void Update();
	void ShutDown();
	IrrlichtDevice * GetDevice(void) const {return device;}
	scene::ISceneManager* GetSceneManager(void) const {return device->getSceneManager();}
	GamePlatform GetGamePlatform() const { return gamePlatform; }
	void SetTransformCamera(scene::ICameraSceneNode* camera, GamePlatform platform);
	void Respawn(core::vector3df& pos, core::vector3df& target);
	void SetResetBot();
	void MoveBot();
	void shoot();

	bool isBulletRendering;
	bool isConnected = false;
	bool isPlayersNameSet = false;
	RakNet::SystemAddress serverSystemAddress;
	int serverPlayerCnt = 1; //�� + �÷��̾� ����

	GamePlatform gamePlatform = GamePlatform::Shooter;	
	core::vector3df initPos;
	core::vector3df initTarget;

	RakNet::TimeMS botMoveTime = 2000;
	RakNet::TimeMS preT = 0;
	RakNet::TimeMS gameStartTime = 0;
	int dir;
	bool isShoot;
	bool isBot;
	int  winScore;
	bool isGameStart;
	bool isGameEnd; 
	int BOT_MOVE_TIME;
	irr::core::stringc mediaPath;
	bool isDummy = false; // Dummy client mode flag

#ifdef __ANDROID__
	android_app* state;
#endif
#if QOS_SUPPORTED 
	void WriteQoSInfo();
	unsigned long get_nsecs();
	bool isQosWritten = false;
	RakNet::TimeMS QosWriteTime = 0;
#endif

private:
	static CInGame* instance;
	video::E_DRIVER_TYPE driverType;
	core::stringw playerName;
	IrrlichtDevice *device;
	int bulletCount;
	RakNet::TimeMS lastShootTime = 0;
	const RakNet::TimeMS shootInterval = 500;
};
#endif
