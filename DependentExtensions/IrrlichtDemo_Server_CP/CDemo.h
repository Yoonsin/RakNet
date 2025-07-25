// This is a Demo of the Irrlicht Engine (c) 2006 by N.Gebhardt.
// This file is not documented.

#ifndef __C_DEMO_H_INCLUDED__
#define __C_DEMO_H_INCLUDED__

#define USE_IRRKLANG
//#define USE_SDL_MIXER

// For windows 1.6 must be used for this demo unless you recompile the dlls and replace them, 
// however it is untested on windows with later versions
// Include path in windows project by defaults assumes C:\irrlicht-1.6
// 1.6 or higher may be used in linux
// Get Irrlicht from http://irrlicht.sourceforge.net/ , it's a great engine
#include <irrlicht.h>
#ifdef __ANDROID__
#define IRRLICHT_MEDIA_PATH  "media/"
#else
#ifdef _WIN32
#define IRRLICHT_MEDIA_PATH  "C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/IrrlichtMedia/" //"../../media/"
#else
#define IRRLICHT_MEDIA_PATH  "../../../src/IrrlichtMedia/"
#endif // _WIN32


#endif // __ANDROID__

#ifdef _WIN32__
#include "WindowsIncludes.h" // Prevent 'fd_set' : 'struct' type redefinition
#include <windows.h>
#endif

using namespace irr;

// audio support

// Included in RakNet download with permission by Nikolaus Gebhardt
#ifdef USE_IRRKLANG
	#if defined(_WIN32__) || defined(WIN32) || defined(_WIN32)
		#include <irrKlang-1.1.3/irrKlang.h>
	#else
		//#include "irrKlang.h"
        #include <irrKlang-1.1.3/irrKlang.h>
	#endif

#ifdef  __ANDROID__
#include <android/native_activity.h>
#include "android_native_app_glue.h"
#else
#pragma comment (lib, "irrKlang-1.1.3/irrKlang.lib")
#endif //  __ANDROID__

#endif
#ifdef USE_SDL_MIXER
	# include <SDL/SDL.h>
	# include <SDL/SDL_mixer.h>
#endif

const int CAMERA_COUNT = 7;
const float CAMERA_HEIGHT=50.0f;
const float SHOT_SPEED=.6f;
const float BALL_DIAMETER=25.0f;

// RakNet
#include "RakNetStuff.h"
#include "DS_Multilist.h"
#include "RakString.h"
#include "RakNetTime.h"




class CDemo : public IEventReceiver
{
public:

	enum class GamePlatform {
		PC,
		Android
	};

	enum GameMessages {
		ID_GAME_MESSAGE_BALL_REQUEST = ID_USER_PACKET_ENUM + 1,
		ID_GAME_MESSAGE_PLAYER_LIFE = ID_USER_PACKET_ENUM + 2
	};

	CDemo(bool fullscreen, bool music, bool shadows, bool additive, bool vsync, bool aa, video::E_DRIVER_TYPE driver, core::stringw &_playerName, bool isServer, GamePlatform platform, bool isLogged, int logCnt, const char* base);
	~CDemo();

	void run();

	virtual bool OnEvent(const SEvent& event);
	IrrlichtDevice * GetDevice(void) const {return device;}
	scene::ISceneManager* GetSceneManager(void) const {return device->getSceneManager();}

	// RakNet: Control what animation is playing by what key is pressed on the remote system
	bool IsKeyDown(EKEY_CODE keyCode) const;
	bool IsMovementKeyDown(void) const;
	// RakNet: Decouple the origin of the shot from the camera, so the network code can use this same graphical effect
	RakNet::TimeMS shootFromOrigin(core::vector3df camPosition, core::vector3df camAt);
	const core::aabbox3df& GetSyndeyBoundingBox(void) const;
	void PlayDeathSound(core::vector3df position);
	void EnableInput(bool enabled);
	void PushMessage(RakNet::RakString rs);

	scene::ISceneNodeAnimator* fpsCamAnim = nullptr;
	bool isBulletRendering;

	scene::IMetaTriangleSelector* metaSelector;
	scene::ITriangleSelector* mapSelector;
	scene::ITriangleSelector* playerSelector;

	bool isConnected = false;
	RakNet::SystemAddress serverSystemAddress;

#ifdef __ANDROID__
	android_app* state;
#endif
private:
	void createLoadingScreen();
	void loadSceneData();
	void switchToNextScene();
	void shoot();
	void createParticleImpacts();

	void SaveStatisticsToCSV();
	
	bool fullscreen;
	bool music;
	bool shadows;
	bool additive;
	bool vsync;
	bool aa;
	const char* baseDir;

	video::E_DRIVER_TYPE driverType;
	core::stringw playerName;
	IrrlichtDevice *device;

	//로그 시작
	bool isLogged;
	bool isServer;
	int logCount;
	GamePlatform platform = GamePlatform::PC;
	irr::core::stringc mediaPath;
	int bulletCount;
	
#ifdef USE_IRRKLANG
	void startIrrKlang();
	irrklang::ISoundEngine* irrKlang;
	irrklang::ISoundSource* ballSound;
	irrklang::ISoundSource* deathSound;
	irrklang::ISoundSource* impactSound;
#endif

#ifdef USE_SDL_MIXER
	void startSound();
	void playSound(Mix_Chunk *);
	void pollSound();
	Mix_Music *stream;
	Mix_Chunk *ballSound;
	Mix_Chunk *impactSound;
#endif

	struct SParticleImpact
	{
		u32 when;
		core::vector3df pos;
		core::vector3df outVector;
	};

	int currentScene;
	video::SColor backColor;

	gui::IGUIStaticText* statusText;
	gui::IGUIInOutFader* inOutFader;

	scene::IQ3LevelMesh* quakeLevelMesh;
	scene::ISceneNode* quakeLevelNode;
	scene::ISceneNode* skyboxNode;
	scene::IAnimatedMeshSceneNode* model1;
	scene::IAnimatedMeshSceneNode* model2;
	scene::IParticleSystemSceneNode* campFire;

	s32 sceneStartTime;
	s32 timeForThisScene;

	core::array<SParticleImpact> Impacts;

	// Per-tick game update for RakNet
	void UpdateRakNet(void);
	// Holds output messages
	DataStructures::Multilist<ML_QUEUE, RakNet::RakString> outputMessages;
	RakNet::TimeMS whenOutputMessageStarted;
	
	const char *GetCurrentMessage(void);
	// We use this array to store the current state of each key
	bool KeyIsDown[KEY_KEY_CODES_COUNT];
	
	// Bounding box of syndney.md2, extended by BALL_DIAMETER/2 for collision against shots
	core::aabbox3df syndeyBoundingBox;
	void CalculateSyndeyBoundingBox(void);

	bool isConnectedToNATPunchthroughServer;

	// CDemo.h 안에 다음 멤버 변수 추가
	RakNet::TimeMS lastShootTime = 0;
	const RakNet::TimeMS shootInterval = 500; // 0.5초 (500ms)

#ifdef __ANDROID__
	s32 TouchID;
	bool isRotate;
	struct CurTouchID {
		s32 move = -1;
		s32 fire = -1;
		s32 jump = -1;
		s32 exit = -1;
		s32 viewRotate = -1;
};
	CurTouchID curTouchID;
	

	gui::IGUIElement* joy_stick;
	gui::IGUIElement* jump_button;
	gui::IGUIElement* fire_button;
	gui::IGUIElement* exit_button;

	/*
	joy_stick = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(AppSkin::REGULAR_AGGREGATION);
	jump_button = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(AppSkin::GUI_JUMP);
	fire_button = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(AppSkin::GUI_FIRE);
	exit_button = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(AppSkin::GUI_EXIT);
	*/
#endif
};

#endif

