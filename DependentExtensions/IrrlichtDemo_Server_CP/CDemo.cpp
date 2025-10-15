
// This is a Demo of the Irrlicht Engine (c) 2005-2009 by N.Gebhardt.
// This file is not documented.



#include "CDemo.h"
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

// RakNet includes
#include "GetTime.h"
#include "MessageIdentifiers.h"
#include "RakNetTypes.h"
#include "Itoa.h"
#include "RakNetSmartPtr.h"
#include "RandSync.h"
#include <chrono>
#ifdef __ANDROID__
#include "android_tools.h"
#include <sys/auxv.h>
/* Irrlicht Extension stuff */
#include <IExtendableSkin.h>
#include <ScrollBarSkinExtension.h>
#include <AggregatableGUIElementAdapter.h>
#include <AggregateGUIElement.h>
#include <timing.h>
#include <utilities.h>
#include <StringHelpers.h>
#include <ScrollBar.h>
#include <Drawer2D.h>
#include <BeautifulGUIImage.h>
#include <AggregateSkinExtension.h>
#include <DraggableGUIElement.h>
#include <DragPlaceGUIElement.h>
#include <JoyStickElement.h>
#include <NotificationBox.h>
#include <mathUtils.h>

class AppSkin : public IExtendableSkin {

public:

	enum SkinIDs {
		DEFAULT_AGGREGATABLE,
		REGULAR_SCROLLBAR,
		REGULAR_AGGREGATION, //joystick
		NO_HIGHLIGHT_AGGREGATION,
		INVISIBLE_AGGREGATION,
		GUI_FIRE,
		GUI_JUMP,
		GUI_EXIT,
		ID_COUNT
	};

	AppSkin(irr::IrrlichtDevice* device, Drawer2D* drawer) :
		IExtendableSkin(device->getGUIEnvironment()->createSkin(gui::EGST_WINDOWS_CLASSIC), drawer) {
		registerExtension(new ScrollBarSkinExtension(this, { video::SColor(255,255,255,255), video::SColor(255,196,198,201) }, .1f, .3f), REGULAR_SCROLLBAR);
		registerExtension(new AggregateSkinExtension(this, false, true), REGULAR_AGGREGATION);
		registerExtension(new AggregateSkinExtension(this, false, true), NO_HIGHLIGHT_AGGREGATION);
		registerExtension(new AggregateSkinExtension(this, false, false), INVISIBLE_AGGREGATION);
		registerExtension(new DefaultAggregatableSkin(this, false), DEFAULT_AGGREGATABLE);
	}

	~AppSkin() {
		parent->drop();//drop required here since parent is created in this constructor
	}

};
#endif

RakNet::RakNetRandom gRand;
DataStructures::List<RakNet::RakString> statBuf;

CDemo::CDemo(bool f, bool m, bool s, bool a, bool v, bool fsaa, video::E_DRIVER_TYPE d, core::stringw& _playerName, bool isS, GamePlatform plat, bool isLog, int logCnt, const char* base, bool isBot, int winScore)
: fullscreen(f), music(m), shadows(s), additive(a), vsync(v), aa(fsaa),
driverType(d), device(0), playerName(_playerName), isServer(isS), gamePlatform(plat), isLogged(isLog), logCount(logCnt), baseDir(base), isBot(isBot), winScore(winScore),
#ifdef USE_IRRKLANG
	irrKlang(0), ballSound(0), impactSound(0),
#endif
#ifdef USE_SDL_MIXER
	stream(0), ballSound(0), impactSound(0),
#endif
	currentScene(-2), backColor(0), statusText(0), inOutFader(0), killLogText(0), myNameText(0), holderPosText(0),
 quakeLevelMesh(0), quakeLevelNode(0), skyboxNode(0), model1(0), model2(0),
 campFire(0), metaSelector(0), mapSelector(0), sceneStartTime(0),
 timeForThisScene(0), whenOutputMessageStarted(0), isConnectedToNATPunchthroughServer(false)
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
	for (u32 i=0; i<KEY_KEY_CODES_COUNT; ++i)
		KeyIsDown[i] = false;

	bulletCount = 0;
	isBulletRendering = false;
	isKeyLock = false;
	wasKeyLock = false;
	isShoot = false;
	isGameStart = false;
	isGameEnd = false;
	gameStartTime = 0;
}


CDemo::~CDemo()
{
	if (mapSelector)
		mapSelector->drop();

	if (metaSelector)
		metaSelector->drop();

#ifdef USE_IRRKLANG
	if (irrKlang)
		irrKlang->drop();
#endif
}


void CDemo::run()
{

#ifdef __ANDROID__
	video::E_DRIVER_TYPE driverType = video::EDT_OGLES2;
	irr::android::SDisplayMetrics displayMetrics;
	memset(&displayMetrics, 0, sizeof displayMetrics);
	irr::android::getDisplayMetrics(state, displayMetrics);
	TouchID = -1;

	SIrrlichtCreationParameters param;
	param.DriverType = driverType;				// android:glEsVersion in AndroidManifest.xml should be "0x00020000"
	param.WindowSize = core::dimension2d<u32>(displayMetrics.widthPixels, displayMetrics.heightPixels);	// using 0,0 it will automatically set it to the maximal size
	param.PrivateData = state;
	param.Bits = 24;
	param.ZBufferBits = 16;
	param.AntiAlias = 0;
	param.EventReceiver = this;
	device = createDeviceEx(param);
	//device->getTimer()->start();

	char filePath[1024] = "/media";
	char absPath[5000];
	if (realpath(filePath, absPath)) {
		DebugPrintf("Absolute path: %s", absPath);
	}
	else {
		DebugPrintf("File not found: %s", filePath);
	}

#else
	core::dimension2d<u32> resolution(640, 480);

#ifdef _WIN32
	mediaPath = "C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/IrrlichtMedia/";
#else
	mediaPath = "../../../src/IrrlichtMedia/";
#endif //_WIN32

	
	irr::SIrrlichtCreationParameters params;

	if (driverType == video::EDT_BURNINGSVIDEO || driverType == video::EDT_SOFTWARE)
	{
		resolution.Width = 640;
		resolution.Height = 480;
	}
	/*
	if (isServer) params.DriverType = video::EDT_NULL;
	else params.DriverType = driverType;
	*/

	params.DriverType = driverType;
	params.WindowSize = resolution;
	params.Bits = 32;
	params.Fullscreen = fullscreen;
	params.Stencilbuffer = shadows;
	params.Vsync = vsync;
	params.AntiAlias = aa;
	params.EventReceiver = this;
	device = createDeviceEx(params);
#endif //__ANDROID__

	video::IVideoDriver* driver = device->getVideoDriver();
	scene::ISceneManager* smgr = device->getSceneManager();
	gui::IGUIEnvironment* guienv = device->getGUIEnvironment();
	io::IFileSystem* fs = device->getFileSystem();
	ILogger* logger = device->getLogger();

	//Android에서 MIP_MAPS를 끄지 않으면 퀘이크 맵 전체가 검게보임
	driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, false);

	device->setWindowCaption(L"Irrlicht Engine Demo");

	// set ambient light
	smgr->setAmbientLight ( video::SColorf ( 0x00c0c0c0 ) );

#ifdef __ANDROID__
	ANativeWindow* nativeWindow = static_cast<ANativeWindow*>(driver->getExposedVideoData().OGLESAndroid.Window);
	int32_t windowWidth = ANativeWindow_getWidth(state->window);
	int32_t windowHeight = ANativeWindow_getHeight(state->window);
	core::dimension2d<s32> dim(driver->getScreenSize());

	char strDisplay[1000];
	sprintf(strDisplay, "!!Window size:(%d/%d)\nDisplay size:(%d/%d)\ngetScreenSize:(%d/%d)", windowWidth, windowHeight, displayMetrics.widthPixels, displayMetrics.heightPixels, dim.Width, dim.Height);
	logger->log(strDisplay);

	for (u32 i = 0; i < fs->getFileArchiveCount(); ++i)
	{
		io::IFileArchive* archive = fs->getFileArchive(i);
		if (archive->getType() == io::E_FILE_ARCHIVE_TYPE::EFAT_ANDROID_ASSET)
		{
			archive->addDirectoryToFileList(mediaPath);
			break;
		}
	}
	
	isRotate = false;

	//IrrlichtDemo Extensions
	core::rect<irr::s32> winRect(0, 0, 9 * displayMetrics.widthPixels / 10, 9 * displayMetrics.heightPixels / 10);
	Drawer2D* drawer = new Drawer2D(device);
	AppSkin* skin = new AppSkin(device, drawer);
	assert(isExtendableSkin(skin));
	guienv->setSkin(skin);
	skin->drop();

	//joystick
	int offset = 150;
	core::rect<s32> testArea3(20, winRect.getHeight() / 2 + 20 + offset, winRect.getWidth() / 4, winRect.getHeight() + offset);
	bool scrollable = false;
	bool horizontal = false;
	//JoyStickElement* joyStick = new JoyStickElement(drawer, env, driver->getTexture("media/joy_background.png"), driver->getTexture("media/joy_handle.png"),1.f,false, AppSkin::DEFAULT_AGGREGATABLE,video::SColor(255,255,255,255),NULL,NULL,testArea3);
	AggregateGUIElement* a3 = new AggregateGUIElement(guienv, 1.f, 1.f, 1.f, 1.f, true, horizontal, scrollable, {
		new JoyStickElement(drawer, guienv, driver->getTexture("media/joy_background.png"), driver->getTexture("media/joy_handle.png"),1.f,true,  AppSkin::DEFAULT_AGGREGATABLE, video::SColor(255,255,255,255),  static_cast<void*>(&isKeyLock)) },
		{}, false, AppSkin::REGULAR_AGGREGATION, NULL, NULL, testArea3);

	//AppSkin::DEFAULT_AGGREGATABLE
#endif //__ANDROID__

	if (device->getFileSystem()->existFile("irrlicht.dat"))
		device->getFileSystem()->addFileArchive("irrlicht.dat", true, true, io::EFAT_ZIP);
	else
		device->getFileSystem()->addFileArchive(mediaPath + "irrlicht.dat", true, true, io::EFAT_ZIP);
	if (device->getFileSystem()->existFile("map-20kdm2.pk3"))
		device->getFileSystem()->addFileArchive("map-20kdm2.pk3", true, true, io::EFAT_ZIP);
	else {
		device->getFileSystem()->addFileArchive(mediaPath + "map-20kdm2.pk3", true, true, io::EFAT_ZIP);
#ifdef __ANDROID__
		if (device->getFileSystem()->existFile(mediaPath + "map-20kdm2.pk3")) {
			DebugPrintf("맵 파일이 존재함!");
		}
		else {
			DebugPrintf("맵 파일이 없음!");
		}
#else 
		wchar_t tmp[255];
#endif // __ANDROID__
	}

	core::rect<int> myNameRect;
	core::rect<int> KillLogRect;
	core::rect<int> holderPosRect;
	const int lwidth = device->getVideoDriver()->getScreenSize().Width - 20;
#ifdef __ANDROID__
	myNameRect = core::rect<int>(10, 50, 1000, 100);
	holderPosRect = core::rect<int>(10, 110, 1000, 160);
	//holderPosRect = core::rect<int>(10, 110, 2000, 300);
	KillLogRect = core::rect<int>(lwidth - 550, 50, lwidth - 50, 700);
#else
	myNameRect = core::rect<int>(10, 0, 250, 30);
	holderPosRect = core::rect<int>(10, 40, 250, 70);  //Font
	//holderPosRect = core::rect<int>(0, 40, 640, 200);  //Big Font

	KillLogRect = core::rect<int>(lwidth - 100, 0, lwidth, 150);
#endif // __ANDROID__

	myNameText = device->getGUIEnvironment()->addStaticText(L"My Name : ", myNameRect);
	myNameText->setOverrideColor(video::SColor(255, 255, 255, 255));
	myNameText->setBackgroundColor(video::SColor(255, 0, 0, 0));

	wchar_t* killText = L"";
	killLogText = device->getGUIEnvironment()->addStaticText(killText, KillLogRect);
	killLogText->setOverrideColor(video::SColor(255, 255, 255, 255));
	killLogText->setBackgroundColor(video::SColor(255, 0, 0, 0));

	holderPosText = device->getGUIEnvironment()->addStaticText(L"Holder Position : ", holderPosRect);
	holderPosText->setOverrideColor(video::SColor(255, 255, 255, 255));
	holderPosText->setBackgroundColor(video::SColor(255, 0, 0, 0));

	auto duration = std::chrono::system_clock::now().time_since_epoch();
	auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
	InitRandom(millis);

	//if (isServer) {
	//	//서버 봇은 1명이라고 가정..
	//	//TODO : 나중에 서버 봇도 여러개 정할 수 있도록 하기
	//}

	// RakNet startup
	InstantiateRakNetClasses(isServer, isLogged, isBot,this);
	CalculateSyndeyBoundingBox();

	s32 now = 0;
#ifdef __ANDROID__
	//에뮬레이터에서 getTime이 0을 반환하므로 임의로 설정
	sceneStartTime = device->getTimer()->getTime();
	//sceneStartTime = device->getTimer()->getRealTime();
	RakNet::TimeMS curTime = RakNet::GetTimeMS();
#else
	sceneStartTime = device->getTimer()->getTime();
#endif 

	//서버가 게임을 측정할 시간
	s32 logTime = 1 * 1000 * 60; //1분
	//현재 시간
	s32 startTime = device->getTimer()->getTime();
	//최대 인원
	bool isLogStart = false;

	while (device->run() && driver)
	{
		// RakNet: Render even if not active, multiplayer never stops
		//if (device->isWindowActive())
		{
#ifdef USE_IRRKLANG
			// update 3D position for sound engine
			scene::ICameraSceneNode* cam = smgr->getActiveCamera();
			if (cam && irrKlang) {
				//irrKlang->setListenerPosition(cam->getAbsolutePosition(), cam->getTarget());
			}
#endif
			if (isGameStart) {
				MoveBot();
			}
			
			//Client Shoot in Range
			if (isShoot) {
				shoot();
				isShoot = false;
			}

			// load next scene if necessary
			now = device->getTimer()->getTime();

			if (now - sceneStartTime > timeForThisScene && timeForThisScene!=-1)
				switchToNextScene();

			createParticleImpacts();

			driver->beginScene(timeForThisScene != -1, true, backColor);

			smgr->drawAll();
			guienv->drawAll();

            //create crosshair
			if(crosshairTex) DrawCrosshairHUD();		
			driver->endScene();
			
#ifdef __ANDROID__
			//60frame
		/*	char k[1000];
			static s32 lastfps = 0;
			s32 nowfps = driver->getFPS();
			sprintf(k, "!!fps:(%d)\n)", nowfps);
			logger->log(k);
			if (nowfps != lastfps) lastfps = nowfps;*/
			
#else			
			//pc는 렌더러 따라서 달라짐
			// write statistics
			static s32 lastfps = 0;
			s32 nowfps = driver->getFPS();

			wchar_t tmp[255];
			swprintf(tmp, 255, L"%ls fps:%d triangles:%0.3f mio",
				driver->getName(),
				driver->getFPS(),
				(f32) driver->getPrimitiveCountDrawn( 1 ) * ( 1.f / 1000000.f )
				);
			if ( nowfps != lastfps )
			{
				device->setWindowCaption ( tmp );
				lastfps = nowfps;
			}
#endif //__ANDROID__

			RakNet::RakString curMsg = GetCurrentMessage();
			if (curMsg.IsEmpty() == false)
			{
				wchar_t dest[500];
				memset(dest, 0, sizeof(dest));
				mbstowcs(dest, curMsg.C_String(), curMsg.GetLength());
				statusText->setText(dest);
			}
			else
			{
				//statusText->setText(tmp);
				statusText->setText(0);
			}

			RakNet::RakString KillMsg = GetCurrentKillLogMessage();
			if (KillMsg.IsEmpty() == false)
			{
				wchar_t dest[500];
				memset(dest, 0, sizeof(dest));
				mbstowcs(dest, KillMsg.C_String(), KillMsg.GetLength());
				killLogText->setText(dest);
			}
			else
			{
				killLogText->setText(0);
			}
		}
		
		// RakNet per 
		// update
		UpdateRakNet();

		//Statistics Timer
		//if (isLogged && isServer) {
		//	int logCnt = (isServer) ? logCount : 1;
		//	//PrintStatistics(false);
		//	if (isLogStart == false && replicaManager3->GetConnectionCount() == logCnt) {
		//		isLogStart = true;
		//		startTime = device->getTimer()->getTime();
		//	}
		//	else if (isLogStart == true) {
		//		//char display[100];
		//		//sprintf(display, "second : %d \n", (now - startTime) / 1000 );
		//		//OutputDebugStringA(display);

		//		if (now - startTime >= logTime) {
		//			device->closeDevice();
		//		}
		//	}
		//}
		
		/*if (GetSceneManager()->getActiveCamera()) {
		DebugPrintf("Player position : %f, %f, %f\n", GetSceneManager()->getActiveCamera()->getPosition().X, GetSceneManager()->getActiveCamera()->getPosition().Y, GetSceneManager()->getActiveCamera()->getPosition().Z);
		DebugPrintf("Player target : %f, %f, %f\n", GetSceneManager()->getActiveCamera()->getTarget().X, GetSceneManager()->getActiveCamera()->getTarget().Y, GetSceneManager()->getActiveCamera()->getTarget().Z);
		}*/

	}

	// RakNet shutdown
	DeinitializeRakNetClasses(isLogged, isBot,baseDir);
	device->drop();
}


bool CDemo::OnEvent(const SEvent& event)
{
#ifdef __ANDROID__
	if (!device)
		return false;

	SEvent fakeKeyEvent;
	fakeKeyEvent.EventType = EET_KEY_INPUT_EVENT;
	fakeKeyEvent.KeyInput.Key = KEY_KEY_CODES_COUNT;

	if (isKeyLock && event.EventType == EET_TOUCH_INPUT_EVENT) {
		curTouchID.move = curTouchID.jump = curTouchID.fire = curTouchID.exit = curTouchID.viewRotate = -1;
		isRotate = false;
		if (fpsCamAnim) fpsCamAnim->isRotate = false;
		return true;
	}

	if (event.EventType == EET_TOUCH_INPUT_EVENT)
	{
		s32 id = event.TouchInput.ID;
		switch (event.TouchInput.Event)
		{
		case ETIE_PRESSED_DOWN:
		{
				if (device)
				{
					core::position2d<s32> touchPoint(event.TouchInput.X, event.TouchInput.Y);
					if (joy_stick && joy_stick->isPointInside(touchPoint)) {
						curTouchID.move = id;
					}
					else
					if (jump_button && jump_button->isPointInside(touchPoint)) {
						//KeyIsDown[KEY_SPACE] = true;
						//fakeKeyEvent.KeyInput.Key = KEY_SPACE;
						//fakeKeyEvent.KeyInput.PressedDown = true;
						curTouchID.jump = id;
					}
					else
					if (fire_button && fire_button->isPointInside(touchPoint)) {
						curTouchID.fire = id;
					}else
					if (exit_button && exit_button->isPointInside(touchPoint)) {
						curTouchID.exit = id;
					}
					else
					{
						isRotate = true;
						if (fpsCamAnim) {
							fpsCamAnim->isRotate = isRotate;
							//fpsCamAnim->TouchStartPos = touchPoint;
							//fpsCamAnim->TouchCurrentPos = touchPoint;
							//fpsCamAnim->startRotation = fpsCamAnim->relativeRotation;
						}
						curTouchID.viewRotate = id;
					}
				}
			break;
		}
		case ETIE_MOVED:
		{
			core::position2d<s32> touchPoint(event.TouchInput.X, event.TouchInput.Y);
			bool is_joy_stick = (joy_stick && joy_stick->isPointInside(touchPoint));
			bool is_gui_viewport = is_joy_stick || (jump_button && jump_button->isPointInside(touchPoint)) || (fire_button && fire_button->isPointInside(touchPoint)) || (exit_button && exit_button->isPointInside(touchPoint));
			
			if (isRotate && id == curTouchID.viewRotate) {
				if (is_gui_viewport ) {
					isRotate = false;
					if (fpsCamAnim) {
						fpsCamAnim->isRotate = isRotate;
					}
					curTouchID.viewRotate = -1;
				}
				else {
					if (fpsCamAnim) {
							//fpsCamAnim->TouchCurrentPos = touchPoint;
					}
				}
			}
			break;
		}
		case ETIE_LEFT_UP:
		{
				core::position2d<s32> touchPoint(event.TouchInput.X, event.TouchInput.Y);
				if (id == curTouchID.move) {
					curTouchID.move = -1;
				}
				else
				if (jump_button && jump_button->isPointInside(touchPoint) && id == curTouchID.jump) {
					//KeyIsDown[KEY_SPACE] = false;
					//fakeKeyEvent.KeyInput.Key = KEY_SPACE;
					//fakeKeyEvent.KeyInput.PressedDown = false;
					if (GetSceneManager()->getActiveCamera())
					{
						Respawn(initPos,initTarget);
						isKeyLock = false;
					}
					
					curTouchID.jump = -1;
				}
				else
				if (fire_button && fire_button->isPointInside(touchPoint) && id == curTouchID.fire && currentScene == 1) {
					curTouchID.fire = -1;
					
					if (GetSceneManager()->getActiveCamera()->isVisible() == false)
					{
						if (device->getCursorControl() != nullptr) device->getCursorControl()->setVisible(false);
						GetSceneManager()->getActiveCamera()->setVisible(true);
					}
					else {
						shoot();
					}

				}else
				if (exit_button && exit_button->isPointInside(touchPoint) && id == curTouchID.exit && currentScene == 1) {
					curTouchID.exit = -1;

					if (GetSceneManager()->getActiveCamera()->isVisible() == false)
					{
						if (device->getCursorControl() != nullptr) device->getCursorControl()->setVisible(false);
						GetSceneManager()->getActiveCamera()->setVisible(true);
					}
					else device->closeDevice();
				}

				if (isRotate && id == curTouchID.viewRotate ) {
					 isRotate = false;
					 if (fpsCamAnim) fpsCamAnim->isRotate = isRotate;
					 curTouchID.viewRotate = -1;
				}
				TouchID = -1;
			break;
		}
		default:
			break;
		}
	}

	if (device->getSceneManager()->getActiveCamera())
	{
		device->getCursorControl();
		device->getSceneManager()->getActiveCamera()->OnEvent(fakeKeyEvent);
	}

#else
if (!device)
return false;

if (isKeyLock && !wasKeyLock) {
	FlushMovementKeys();   // 잠금 켜질 때 즉시 정지
	wasKeyLock = true;
}
else if (!isKeyLock && wasKeyLock) {
	wasKeyLock = false;    // 잠금 해제됨
}

//잠금 중에는 입력 기록/전달을 차단 (화이트리스트 키만 허용)
if (isKeyLock) {
	// 허용할 키(예: ESC, F9, R)만 통과
	if (event.EventType == EET_KEY_INPUT_EVENT) {
		const auto key = event.KeyInput.Key;
		const bool down = event.KeyInput.PressedDown;

		const bool allow =
			key == KEY_ESCAPE ||
			key == KEY_F9 ||
			key == KEY_KEY_R;

		if (!allow) {
			// 눌림은 무시, 떼는 입력은 내부 상태만 false로 정리
			if (!down) KeyIsDown[key] = false;
			return true;   // 이벤트 소비
		}
	}
	// 마우스(회전/사격)도 차단
	if (event.EventType == EET_MOUSE_INPUT_EVENT) {
		return true;       // 이벤트 소비
	}
}

// Remember whether each key is down or up
	if (event.EventType == irr::EET_KEY_INPUT_EVENT)
		KeyIsDown[event.KeyInput.Key] = event.KeyInput.PressedDown;

	if (event.EventType == EET_KEY_INPUT_EVENT &&
		event.KeyInput.Key == KEY_ESCAPE &&
		event.KeyInput.PressedDown == false)
	{
		// user wants to quit.
//		if (currentScene < 3)
//			timeForThisScene = 0;
//		else
			//device->closeDevice();

		// RakNet: Escape to get the mouse back
		if (GetSceneManager()->getActiveCamera()->isVisible())
		{
			if (device->getCursorControl() != nullptr) device->getCursorControl()->setVisible(true);
			GetSceneManager()->getActiveCamera()->setVisible(false);
		}
		else
		{
			device->closeDevice();
		}
	}
	else
		if (
			// RakNet: Use space to jump, not shoot
	//		(event.EventType == EET_KEY_INPUT_EVENT &&
	//		event.KeyInput.Key == KEY_SPACE &&
	//		event.KeyInput.PressedDown == false) ||
			(event.EventType == EET_MOUSE_INPUT_EVENT &&
				event.MouseInput.Event == EMIE_LMOUSE_LEFT_UP) &&
			//currentScene == 3
			currentScene == 1
			)
		{
			
			// RakNet: Click without focus to get focus back
			if (GetSceneManager()->getActiveCamera()->isVisible() == false)
			{
				if (device->getCursorControl() != nullptr) device->getCursorControl()->setVisible(false);
				GetSceneManager()->getActiveCamera()->setVisible(true);
			}
			else
			{
				// shoot
				shoot();
			}
		}
	else
		if (event.EventType == EET_KEY_INPUT_EVENT &&
			event.KeyInput.Key == KEY_F9 &&
			event.KeyInput.PressedDown == false)
		{
				video::IImage* image = device->getVideoDriver()->createScreenShot();
				if (image)
				{
					device->getVideoDriver()->writeImageToFile(image, "screenshot.bmp");
					device->getVideoDriver()->writeImageToFile(image, "screenshot.png");
					device->getVideoDriver()->writeImageToFile(image, "screenshot.tga");
					device->getVideoDriver()->writeImageToFile(image, "screenshot.ppm");
					device->getVideoDriver()->writeImageToFile(image, "screenshot.jpg");
					device->getVideoDriver()->writeImageToFile(image, "screenshot.pcx");
					image->drop();
				}
		}
	else
		if (
				event.EventType == EET_KEY_INPUT_EVENT &&
				event.KeyInput.Key == KEY_KEY_R &&
				event.KeyInput.PressedDown == false)
		{
			if (auto* cam = device->getSceneManager()->getActiveCamera()) {
				if (isBot) {
					SetResetBot();
					Respawn(playerBotReplica->respawnPos, playerBotReplica->respawnTarget);
					botMoveTime = RakNet::GetTimeMS() + BOT_MOVE_TIME;
				}
				else {
					Respawn(initPos, initTarget);
				}
				isKeyLock = false;
				FlushMovementKeys(); //리셋 시에도 모든 이동키 해제
			}
		}
	else
		if (device->getSceneManager()->getActiveCamera())
		{
			if (isKeyLock)
				return true; // 여기서 바로 빠져나가면 기존 방향으로 계속 이동하지 않음

			if (event.EventType == EET_MOUSE_INPUT_EVENT) {
				//device->getSceneManager()->getActiveCamera()->OnEvent(event);
			}
			else if (event.EventType == EET_KEY_INPUT_EVENT) {
				device->getSceneManager()->getActiveCamera()->OnEvent(event);
				//if(event.KeyInput.Key == KEY_KEY_A || event.KeyInput.Key == KEY_KEY_D) device->getSceneManager()->getActiveCamera()->OnEvent(event);
				//DebugPrintf("Player position : %f, %f, %f / isKeyLock : %d / wasKeyLock : %d \n", GetSceneManager()->getActiveCamera()->getPosition().X, GetSceneManager()->getActiveCamera()->getPosition().Y, GetSceneManager()->getActiveCamera()->getPosition().Z, isKeyLock, wasKeyLock);
			}
			return true;
		}
#endif //__ANDROID__
	return false;
}


void CDemo::switchToNextScene()
{
	currentScene++;
	//if (currentScene > 3)
	if (currentScene > 1)
		currentScene = 1;

	scene::ISceneManager* sm = device->getSceneManager();
	scene::ISceneNodeAnimator* sa = 0;
	scene::ICameraSceneNode* camera = 0;

	camera = sm->getActiveCamera();
	if (camera)
	{
		sm->setActiveCamera(0);
		camera->remove();
		camera = 0;
	}

	switch(currentScene)
	{
	case -1: // loading screen
		timeForThisScene = 0;
		createLoadingScreen();
		break;

	case 0: // load scene
		timeForThisScene = 0;
		loadSceneData();
		break;
	case 1: // interactive, go around
		{
			if (model1)
				model1->setVisible(true);
			if (model2)
				model2->setVisible(true);
			campFire->setVisible(true);
			timeForThisScene = -1;

#ifdef __ANDROID__
			SKeyMap keyMap[11];
#else
			SKeyMap keyMap[9];
#endif // __ANDROID__
			keyMap[0].Action = EKA_MOVE_FORWARD;
			keyMap[0].KeyCode = KEY_UP;
			keyMap[1].Action = EKA_MOVE_FORWARD;
			keyMap[1].KeyCode = KEY_KEY_W;

			keyMap[2].Action = EKA_MOVE_BACKWARD;
			keyMap[2].KeyCode = KEY_DOWN;
			keyMap[3].Action = EKA_MOVE_BACKWARD;
			keyMap[3].KeyCode = KEY_KEY_S;

			keyMap[4].Action = EKA_STRAFE_LEFT;
			keyMap[4].KeyCode = KEY_LEFT;
			keyMap[5].Action = EKA_STRAFE_LEFT;
			keyMap[5].KeyCode = KEY_KEY_A;

			keyMap[6].Action = EKA_STRAFE_RIGHT;
			keyMap[6].KeyCode = KEY_RIGHT;
			keyMap[7].Action = EKA_STRAFE_RIGHT;
			keyMap[7].KeyCode = KEY_KEY_D;

			keyMap[8].Action = EKA_JUMP_UP;
			//keyMap[8].KeyCode = KEY_KEY_J;
			keyMap[8].KeyCode = KEY_SPACE;

			//rotate
#ifdef __ANDROID__
			keyMap[9].Action = EKA_ROTATE_LEFT;
			keyMap[9].KeyCode = KEY_KEY_0;
			keyMap[10].Action = EKA_ROTATE_RIGHT;
			keyMap[10].KeyCode = KEY_KEY_1;
			//camera = sm->addCameraSceneNodeFPS(0, 1.0f, .4f, -1, keyMap, 11, true, 250.f); //기본 플레이
			camera = sm->addCameraSceneNodeFPS(0, 1.0f, .4f, -1, keyMap, 11, false, 5.f); //기본 플레이
			SetTransformCamera(camera, gamePlatform);

			core::vector3df gravity = core::vector3df(0, /*-300.f*/quakeLevelMesh ? -10.f : 0.0f, 0);
			scene::ISceneNodeAnimatorCollisionResponse* collider =
				sm->createCollisionResponseAnimator(
					metaSelector, camera, core::vector3df(25, CAMERA_HEIGHT, 25), gravity, core::vector3df(0, 45, 0), 0.005f);

			camera->addAnimator(collider);
			collider->drop();

			const scene::ISceneNodeAnimatorList& animators = camera->getAnimators();
			scene::ISceneNodeAnimatorList::ConstIterator it = animators.begin();
			while (it != animators.end())
			{
				if (scene::ESNAT_COLLISION_RESPONSE == (*it)->getType()) {
					fpsCamResponse = static_cast<scene::ISceneNodeAnimatorCollisionResponse*>(*it);
				}
				else if (scene::ESNAT_CAMERA_FPS == (*it)->getType()) {
					fpsCamAnim = static_cast<scene::ISceneNodeAnimatorCameraFPS*>(*it);
				}
				it++;
			}
#else
			//Last parameter is jump speed
			//Tweaked so you can get up ladders
			camera = sm->addCameraSceneNodeFPS(0, 100.0f, .4f, -1, keyMap, 9, false, 5.f);
			SetTransformCamera(camera, gamePlatform);
			
			core::vector3df gravity = core::vector3df(0, quakeLevelMesh ? -10.f : 0.0f, 0);
			scene::ISceneNodeAnimatorCollisionResponse* collider =
				sm->createCollisionResponseAnimator(
					metaSelector, camera, core::vector3df(25, CAMERA_HEIGHT, 25), gravity, core::vector3df(0, 45, 0), 0.005f);

			//	waypoint[0].set(-150,40,100); waypoint[1].set(350, 40, 100);
			camera->addAnimator(collider);
			collider->drop();

			const scene::ISceneNodeAnimatorList& animators = camera->getAnimators();
			scene::ISceneNodeAnimatorList::ConstIterator it = animators.begin();
			while (it != animators.end())
			{
				if (scene::ESNAT_COLLISION_RESPONSE == (*it)->getType()){
					fpsCamResponse = static_cast<scene::ISceneNodeAnimatorCollisionResponse*>(*it);
				}
				else if (scene::ESNAT_CAMERA_FPS == (*it)->getType()) {
					fpsCamAnim = static_cast<scene::ISceneNodeAnimatorCameraFPS*>(*it);
				}
				it++;
			}

			crosshairTex = device->getVideoDriver()->getTexture(mediaPath + "crossHair_white.png");


#endif // __ANDROID__
		}
		break;
	}

	sceneStartTime = device->getTimer()->getTime();
}


void CDemo::loadSceneData()
{
	// load quake level

	video::IVideoDriver* driver = device->getVideoDriver();
	scene::ISceneManager* sm = device->getSceneManager();

	// Quake3 Shader controls Z-Writing
	sm->getParameters()->setAttribute(scene::ALLOW_ZWRITE_ON_TRANSPARENT, true);

	quakeLevelMesh = (scene::IQ3LevelMesh*) sm->getMesh("20kdm2.bsp");

#ifdef __ANDROID__
	if (!quakeLevelMesh) {
		DebugPrintf("Error: Quake3 Level Mesh 로드 실패!");
	}
	scene::IMesh* levelMesh = quakeLevelMesh->getMesh(scene::quake3::E_Q3_MESH_GEOMETRY);
	if (!levelMesh) {
		DebugPrintf("Error: Quake Level Mesh Geometry가 NULL!");
	}
#endif // __ANDROID__

	if (quakeLevelMesh)
	{
		u32 i;

		//move all quake level meshes (non-realtime)
		core::matrix4 m;
		m.setTranslation ( core::vector3df(-1300,-70,-1249) );

		for ( i = 0; i!= scene::quake3::E_Q3_MESH_SIZE; ++i )
		{
			sm->getMeshManipulator()->transform ( quakeLevelMesh->getMesh(i), m );
		}

		quakeLevelNode = sm->addOctreeSceneNode(
			quakeLevelMesh->getMesh( scene::quake3::E_Q3_MESH_GEOMETRY)
			);
		if (quakeLevelNode)
		{
			//quakeLevelNode->setPosition(core::vector3df(-1300,-70,-1249));
			quakeLevelNode->setVisible(true);

			// create map triangle selector
			mapSelector = sm->createOctreeTriangleSelector(quakeLevelMesh->getMesh(0),
				quakeLevelNode, 128);

			// if not using shader and no gamma it's better to use more lighting, because
			// quake3 level are usually dark
			quakeLevelNode->setMaterialType ( video::EMT_LIGHTMAP_M4 );

			// set additive blending if wanted
			if (additive)
				quakeLevelNode->setMaterialType(video::EMT_LIGHTMAP_ADD);
		}

		// the additional mesh can be quite huge and is unoptimized
		scene::IMesh * additional_mesh = quakeLevelMesh->getMesh ( scene::quake3::E_Q3_MESH_ITEMS );

		for ( i = 0; i!= additional_mesh->getMeshBufferCount (); ++i )
		{
			scene::IMeshBuffer *meshBuffer = additional_mesh->getMeshBuffer ( i );
			const video::SMaterial &material = meshBuffer->getMaterial();

			//! The ShaderIndex is stored in the material parameter
			s32 shaderIndex = (s32) material.MaterialTypeParam2;

			// the meshbuffer can be rendered without additional support, or it has no shader
			const scene::quake3::IShader *shader = quakeLevelMesh->getShader ( shaderIndex );
			if ( 0 == shader )
			{
				continue;
			}
			// Now add the MeshBuffer(s) with the current Shader to the Manager
			sm->addQuake3SceneNode ( meshBuffer, shader );
		}


	}
	scene::ISceneNodeAnimator* anim = 0;

	// create sky box
	driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, false);
	skyboxNode = sm->addSkyBoxSceneNode(
	driver->getTexture(mediaPath+ "irrlicht2_up.jpg"),
	driver->getTexture(mediaPath+ "irrlicht2_dn.jpg"),
	driver->getTexture(mediaPath+ "irrlicht2_lf.jpg"),
	driver->getTexture(mediaPath+ "irrlicht2_rt.jpg"),
	driver->getTexture(mediaPath+ "irrlicht2_ft.jpg"),
	driver->getTexture(mediaPath+ "irrlicht2_bk.jpg"));

	core::vector3df waypoint[2];
	waypoint[0].set(-150,40,100);
	waypoint[1].set(350, 40, 100);

	//if (model2)
	//{
	//	anim = device->getSceneManager()->createFlyStraightAnimator(waypoint[0],
	//		waypoint[1], 2000, true);
	//	model2->addAnimator(anim);
	//	anim->drop();
	//}

	// create animation for portals;

	core::array<video::ITexture*> textures;
	for (s32 g=1; g<8; ++g)
	{
		core::stringc tmp(IRRLICHT_MEDIA_PATH "portal");
		tmp += g;
		tmp += ".png";
		video::ITexture* t = driver->getTexture( tmp );
		textures.push_back(t);
	}

	anim = sm->createTextureAnimator(textures, 100);

	// create portals

	scene::IBillboardSceneNode* bill = 0;

	for (int r=0; r<2; ++r)
	{
		bill = sm->addBillboardSceneNode(0, core::dimension2d<f32>(100,100),
			waypoint[r]+ core::vector3df(0,20,0));
		bill->setMaterialFlag(video::EMF_LIGHTING, false);
		bill->setMaterialTexture(0, driver->getTexture(mediaPath+ "portal1.png"));
		bill->setMaterialType(video::EMT_TRANSPARENT_ADD_COLOR);
		bill->addAnimator(anim);
	}

	anim->drop();

	// create cirlce flying dynamic light with transparent billboard attached

	scene::ILightSceneNode* light = 0;

	light = sm->addLightSceneNode(0,
		core::vector3df(0,0,0),	video::SColorf(1.0f, 1.0f, 1.f, 1.0f), 500.f);

	anim = sm->createFlyCircleAnimator(
		core::vector3df(100,150,80), 80.0f, 0.0005f);

	light->addAnimator(anim);
	anim->drop();

	bill = device->getSceneManager()->addBillboardSceneNode(
		light, core::dimension2d<f32>(40,40));
	bill->setMaterialFlag(video::EMF_LIGHTING, false);
	bill->setMaterialTexture(0, driver->getTexture(mediaPath+ "particlewhite.bmp"));
	bill->setMaterialType(video::EMT_TRANSPARENT_ADD_COLOR);

	// create meta triangle selector with all triangles selectors in it.
	metaSelector = sm->createMetaTriangleSelector();
	metaSelector->addTriangleSelector(mapSelector);

	// create camp fire

	campFire = sm->addParticleSystemSceneNode(false);
	campFire->setPosition(core::vector3df(100, 120, 600));
	//campFire->setPosition(core::vector3df(279.522980, 100.080017, -290.277802));
	campFire->setScale(core::vector3df(2,2,2));


	scene::IParticleEmitter* em = campFire->createBoxEmitter(
		core::aabbox3d<f32>(-7,0,-7,7,1,7),
		core::vector3df(0.0f,0.06f,0.0f),
		80,100, video::SColor(0,255,255,255),video::SColor(0,255,255,255), 800,2000);

	em->setMinStartSize(core::dimension2d<f32>(20.0f, 10.0f));
	em->setMaxStartSize(core::dimension2d<f32>(20.0f, 10.0f));
	campFire->setEmitter(em);
	em->drop();

	scene::IParticleAffector* paf = campFire->createFadeOutParticleAffector();
	campFire->addAffector(paf);
	paf->drop();

	campFire->setMaterialFlag(video::EMF_LIGHTING, false);
	campFire->setMaterialFlag(video::EMF_ZWRITE_ENABLE, false);
	campFire->setMaterialTexture(0, driver->getTexture(mediaPath+ "fireball.bmp"));
	campFire->setMaterialType(video::EMT_TRANSPARENT_VERTEX_ALPHA);
	// load music

#ifdef USE_IRRKLANG
	/*
	if (music)
		startIrrKlang();
	*/
#endif
#ifdef USE_SDL_MIXER
	if (music)
		startSound();
#endif


#ifdef __ANDROID__
	// set game UI
	
	core::dimension2d<u32> size = device->getVideoDriver()->getScreenSize();
	int offset = 50;
	int offset2 = 150;
	core::rect<int> jumpPos(size.Width-150-offset-offset2, size.Height-300 - offset, size.Width-offset2, size.Height - 150);
	device->getGUIEnvironment()->addButton(jumpPos, 0, AppSkin::GUI_JUMP, L"RESET"); //디버그 용으로 점프 -> 리셋으로 수정

	core::rect<int> firePos(size.Width-150 - offset-offset2, size.Height-550 - offset, size.Width-offset2 ,size.Height-400);
	device->getGUIEnvironment()->addButton(firePos, 0, AppSkin::GUI_FIRE, L"FIRE");

	core::rect<int> exitPos(
		size.Width - 200 - 2 * offset - 2 * offset2,  // left  = jumpLeft - gap - w
		size.Height - 300 - offset,               // top
		size.Width - 50 - offset - 2 * offset2,    // right = jumpLeft - gap
		size.Height - 150                              // bottom
	);
	device->getGUIEnvironment()->addButton(exitPos, 0, AppSkin::GUI_EXIT, L"EXIT");

	joy_stick = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(AppSkin::REGULAR_AGGREGATION);
	jump_button = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(AppSkin::GUI_JUMP);
	fire_button = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(AppSkin::GUI_FIRE);
	exit_button = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(AppSkin::GUI_EXIT);

#endif // __ANDROID__
}

void CDemo::createLoadingScreen()
{
	core::dimension2d<u32> size = device->getVideoDriver()->getScreenSize();

	if(device->getCursorControl() != nullptr) device->getCursorControl()->setVisible(false);

	// setup loading screen

	backColor.set(255,90,90,156);

	// create in fader
	//inOutFader = device->getGUIEnvironment()->addInOutFader();
	//inOutFader->setColor(backColor,	video::SColor ( 0, 230, 230, 230 ));

	// loading text
	const int lwidth = size.Width - 20;
	const int lheight = 16;

	core::rect<int> pos(10, size.Height-lheight-80, 10+lwidth, size.Height-80);
	//device->getGUIEnvironment()->addImage(pos);
	statusText = device->getGUIEnvironment()->addStaticText(L"Start", pos, true);
	statusText->setOverrideColor(video::SColor(255,205,200,200));

#ifdef __ANDROID__
	device->getGUIEnvironment()->getSkin()->setFont(device->getGUIEnvironment()->getFont(mediaPath + "bigfont.png"));
	//device->getGUIEnvironment()->getSkin()->setFont(device->getGUIEnvironment()->getFont(mediaPath + "font_56px.xml"));
#else
	device->getGUIEnvironment()->getSkin()->setFont(device->getGUIEnvironment()->getFont(mediaPath+ "fonthaettenschweiler.bmp"));
	//device->getGUIEnvironment()->getSkin()->setFont(device->getGUIEnvironment()->getFont(mediaPath + "font_24px.xml"));
#endif


	device->getGUIEnvironment()->getSkin()->setColor(gui::EGDC_BUTTON_TEXT,
		video::SColor(255,100,100,100));
}
void CDemo::CalculateSyndeyBoundingBox(void)
{
	// Find the extents of the player character's model (for networking collision checks)
	scene::IAnimatedMesh* mesh = 0;
	scene::ISceneManager *sm = device->getSceneManager();
	mesh = sm->getMesh(mediaPath + "sydney.md2");
	irr::scene::IAnimatedMeshSceneNode* model;
	model = sm->addAnimatedMeshSceneNode(mesh, 0);
	model->setScale(core::vector3df(1,1,1));
	// Bounding box changed in Irrlicht 1.5.1
	core::aabbox3df modelBoundingBox = model->getMesh()->getBoundingBox();
	// core::aabbox3df modelBoundingBox = model->getBoundingBox();
	core::vector3df minEdgeExtended = modelBoundingBox.MinEdge;
	core::vector3df maxEdgeExtended = modelBoundingBox.MaxEdge;
	minEdgeExtended.X-=BALL_DIAMETER;
	minEdgeExtended.Y -= BALL_DIAMETER*1.25;
	minEdgeExtended.Z-=BALL_DIAMETER*0.2;
	maxEdgeExtended.X+=BALL_DIAMETER;///2
	maxEdgeExtended.Y+=BALL_DIAMETER*1.25;
	maxEdgeExtended.Z+=BALL_DIAMETER*0.2;
	syndeyBoundingBox.MinEdge=minEdgeExtended;
	syndeyBoundingBox.MaxEdge=maxEdgeExtended;
	model->remove();
};
// RakNet - Precalculate bounding box of Sydney.md2, since our own player's model is never loaded
// This only works on the assumption that all players have the same model
const core::aabbox3df& CDemo::GetSyndeyBoundingBox(void) const
{
	return syndeyBoundingBox;
}
void CDemo::PlayDeathSound(core::vector3df position)
{
	if (irrKlang)
	{
		/*
		irrklang::ISound* sound = 
			irrKlang->play3D(impactSound, position, false, false, true);

		if (sound)
		{
			// adjust max value a bit to make to sound of an impact louder
			sound->setMinDistance(400);
			sound->drop();
		}
		*/
	}	
}
void CDemo::EnableInput(bool enabled)
{
	scene::ICameraSceneNode* camera = GetSceneManager()->getActiveCamera();
	if(camera) camera->setInputReceiverEnabled(enabled);
}
// RakNet - change shoot from assuming the camera, to taking any starting location
// This way the same function can be called from the network
RakNet::TimeMS CDemo::shootFromOrigin(core::vector3df camPosition, core::vector3df camAt, GamePlatform platform)
{
	scene::ISceneManager* sm = device->getSceneManager();
	scene::ICameraSceneNode* camera = sm->getActiveCamera();
	// get line of camera
	core::vector3df start = camPosition;
	core::vector3df end = (camAt);
	//end.normalize();
	start += end*8.0f;
	end = start + (end * camera->getFarValue());

	bool wallHit = false;
	core::vector3df wallHitPoint(0, 0, 0);
	return shootFromOrigin(camPosition, camAt, start, end, wallHit, wallHitPoint, platform);
}

RakNet::TimeMS CDemo::shootFromOrigin(core::vector3df camPosition, core::vector3df camAt,core::vector3df start, core::vector3df end, bool& wallHit, core::vector3df& wallHitPoint, GamePlatform platform)
{
	scene::ISceneManager* sm = device->getSceneManager();
	scene::ICameraSceneNode* camera = sm->getActiveCamera();

	if (!camera || !mapSelector)
		return 0;

	SParticleImpact imp;
	imp.when = 0;
	
	core::triangle3df triangle;

	core::line3d<irr::f32> line(start, end);

	// get intersection point with map
	const scene::ISceneNode* hitNode;

#ifdef __ANDROID__
	scene::SCollisionHit hitResult;
	bool flag = false;
	if (sm->getSceneCollisionManager()->getCollisionPoint(hitResult, line, mapSelector)) {
		end = hitResult.Intersection;
		triangle = hitResult.Triangle;
		hitNode = hitResult.Node;
		flag = true;
	}

	if (flag) {
		// collides with wall
		wallHit = true;
		wallHitPoint = end;
		
		core::vector3df out = triangle.getNormal();
		out.setLength(0.03f);

		imp.when = 1;
		imp.outVector = out;
		imp.pos = end;
	}
	else {
		// doesnt collide with wall
		wallHit = false;
		wallHitPoint = core::vector3df(0, 0, 0);

		core::vector3df start = camPosition;
		core::vector3df end = (camAt);
		//end.normalize();
		start += end * 8.0f;
		end = start + (end * camera->getFarValue());
	}

#else
	if (sm->getSceneCollisionManager()->getCollisionPoint(line, mapSelector, end, triangle, hitNode))
	{
		// collides with wall
		wallHit = true;
		wallHitPoint = end;
		
		core::vector3df out = triangle.getNormal();
		out.setLength(0.03f);

		imp.when = 1;
		imp.outVector = out;
		imp.pos = end;
	}
	else
	{
		// doesnt collide with wall
		wallHit = false;
		wallHitPoint = core::vector3df(0, 0, 0);

		core::vector3df start = camPosition;
		core::vector3df end = (camAt);
		//end.normalize();
		start += end * 8.0f;
		end = start + (end * camera->getFarValue());
	}
#endif // __ANDROID__

	// create fire ball
	scene::ISceneNode* node = 0;
	node = sm->addBillboardSceneNode(0,
		core::dimension2d<f32>(BALL_DIAMETER - 10, BALL_DIAMETER - 10), start);

	node->setMaterialFlag(video::EMF_LIGHTING, false);
	if (platform == Holder) {
		node->setMaterialTexture(0, device->getVideoDriver()->getTexture(mediaPath + "fireball_green.bmp"));
	}
	else if(platform == Shooter) {
		node->setMaterialTexture(0, device->getVideoDriver()->getTexture(mediaPath + "fireball_blue.bmp"));
	}
	else if (platform == Server) {
		node->setMaterialTexture(0, device->getVideoDriver()->getTexture(mediaPath + "fireball.bmp"));
	}
	
	node->setMaterialType(video::EMT_TRANSPARENT_ADD_COLOR);

	f32 length = (f32)(end - start).getLength();
	const f32 speed = SHOT_SPEED;
	u32 time = (u32)(length / speed);

	scene::ISceneNodeAnimator* anim = 0;

	// set flight line
	anim = sm->createFlyStraightAnimator(start, end, time);
	node->addAnimator(anim);
	anim->drop();

	anim = sm->createDeleteAnimator(time);
	node->addAnimator(anim);
	anim->drop();

	if (imp.when)
	{
		// create impact note
		imp.when = device->getTimer()->getTime() + (time - 100);
		Impacts.push_back(imp);
	}

	return (RakNet::TimeMS)time;

}


void CDemo::shoot()
{
	if (playerReplica==nullptr || playerReplica->IsDead())
		return;

	playerReplica->shootCnt++;
	SetPlayerNameText();

	scene::ISceneManager* sm = device->getSceneManager();
	scene::ICameraSceneNode* camera = sm->getActiveCamera();
	core::vector3df camPosition = camera->getPosition();
	core::vector3df camAt = (camera->getTarget() - camPosition);
	camAt.normalize();

	BallReplica *br = new BallReplica;
	br->demo=this;
	br->position=camPosition;
	br->shotDirection=camAt;
	br->shotLifetime=RakNet::GetTimeMS() + shootFromOrigin(camPosition, camAt, gamePlatform);

	replicaManager3->Reference(br);
}

void CDemo::MoveBot()
{
	//Bot movement
	RakNet::TimeMS t = RakNet::GetTimeMS();
	if (isBot && playerBotReplica) {
		if (!playerBotReplica->IsDead() && !playerBotReplica->wasDead && device->getSceneManager()->getActiveCamera()) {
			SEvent botKeyEvent;
			botKeyEvent.EventType = EET_KEY_INPUT_EVENT;
			botKeyEvent.KeyInput.Key = KEY_KEY_W;
			if (botMoveTime >= t) {
				if (dir != 0) {
					botKeyEvent.KeyInput.PressedDown = KeyIsDown[botKeyEvent.KeyInput.Key] = true;
					device->getSceneManager()->getActiveCamera()->OnEvent(botKeyEvent);
				}
				//DebugPrintf("go / timeDiff : %d\n", t-preT);
			}
			else {
				if (dir != 0) {
					botKeyEvent.KeyInput.PressedDown = KeyIsDown[botKeyEvent.KeyInput.Key] = false;
					device->getSceneManager()->getActiveCamera()->OnEvent(botKeyEvent);
				}
				SetResetBot();
				Respawn(playerBotReplica->respawnPos, playerBotReplica->respawnTarget);
				botMoveTime = t + BOT_MOVE_TIME;

				RakNet::BitStream bs;
				bs.Write((RakNet::MessageID)CDemo::ID_GAME_MESSAGE_PLAYER_RESPAWN);
				bs.Write(playerBotReplica->creatingSystemGUID);
				bs.Write(playerBotReplica->respawnPos);
				bs.Write(playerBotReplica->respawnTarget);
				rakPeer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
			}
		}
	}
	preT = t;
}

void CDemo::createParticleImpacts()
{
	u32 now = device->getTimer()->getTime();
	scene::ISceneManager* sm = device->getSceneManager();

	for (s32 i=0; i<(s32)Impacts.size(); ++i)
		if (now > Impacts[i].when)
		{
			// create smoke particle system
			scene::IParticleSystemSceneNode* pas = 0;

			pas = sm->addParticleSystemSceneNode(false, 0, -1, Impacts[i].pos);

			pas->setParticleSize(core::dimension2d<f32>(10.0f, 10.0f));

			scene::IParticleEmitter* em = pas->createBoxEmitter(
				core::aabbox3d<f32>(-5,-5,-5,5,5,5),
				Impacts[i].outVector, 20,40, video::SColor(0,255,255,255),video::SColor(0,255,255,255),
				1200,1600, 20);

			pas->setEmitter(em);
			em->drop();

			scene::IParticleAffector* paf = campFire->createFadeOutParticleAffector();
			pas->addAffector(paf);
			paf->drop();

			pas->setMaterialFlag(video::EMF_LIGHTING, false);
			pas->setMaterialTexture(0, device->getVideoDriver()->getTexture(mediaPath+ "smoke.bmp"));
#ifdef __ANDROID__
			pas->setMaterialType(video::EMT_TRANSPARENT_ADD_COLOR);
#else
			pas->setMaterialType(video::EMT_TRANSPARENT_VERTEX_ALPHA);
#endif //__ANDROID__

			scene::ISceneNodeAnimator* anim = sm->createDeleteAnimator(2000);
			pas->addAnimator(anim);
			anim->drop();

			// play impact sound
			#ifdef USE_IRRKLANG
			/*
			if (irrKlang)
			{
				irrklang::ISound* sound = 
					irrKlang->play3D(impactSound, Impacts[i].pos, false, false, true);

				if (sound)
				{
					// adjust max value a bit to make to sound of an impact louder
					sound->setMinDistance(400);
					sound->drop();
				}
			}
			*/
			#endif

			#ifdef USE_SDL_MIXER
			if (impactSound)
				playSound(impactSound);
			#endif

			// delete entry
			Impacts.erase(i);
			i--;
		}
}

/// RakNet stuff
void CDemo::UpdateRakNet(void)
{
	RakNet::Packet* packet;
	RakNet::TimeMS curTime = RakNet::GetTimeMS();
	RakNet::RakString targetName;
	for (packet = rakPeer->Receive(); packet; rakPeer->DeallocatePacket(packet), packet = rakPeer->Receive())
	{
		if (strcmp(packet->systemAddress.ToString(false), DEFAULT_NAT_PUNCHTHROUGH_FACILITATOR_IP) == 0)
		{
			targetName = "NATPunchthroughServer";
		}
		else
		{
			targetName = packet->systemAddress.ToString(true);
		}

		switch (packet->data[0])
		{
		case ID_IP_RECENTLY_CONNECTED:
		{
			//PushMessage(RakNet::RakString("This IP address recently connected from ") + targetName + RakNet::RakString("."));
		}
		break;
		case ID_INCOMPATIBLE_PROTOCOL_VERSION:
		{
			//PushMessage(RakNet::RakString("Incompatible protocol version from ") + targetName + RakNet::RakString("."));
		}
		break;
		case ID_DISCONNECTION_NOTIFICATION:
		{
			//PushMessage(RakNet::RakString("Disconnected from ") + targetName + RakNet::RakString("."));
		}
		break;
		case ID_CONNECTION_LOST:
		{
			//PushMessage(RakNet::RakString("Connection to ") + targetName + RakNet::RakString(" lost."));
		}
		break;
		case ID_NO_FREE_INCOMING_CONNECTIONS:
		{
			//PushMessage(RakNet::RakString("No free incoming connections to ") + targetName + RakNet::RakString("."));
		}
		break;
		case ID_NEW_INCOMING_CONNECTION:
		{
			PushMessage(RakNet::RakString("Sending player list to new connection"));
			//systemAddress에 할당되는 Connection 객체를 만들고 
			RakNet::Connection_RM3* connection = replicaManager3->AllocConnection(packet->systemAddress, rakPeer->GetGuidFromSystemAddress(packet->systemAddress));
			//replicaManager3에 추적될 수 있도록 할당
			replicaManager3->PushConnection(connection);

			if (logCount == replicaManager3->GetConnectionCount())
			{
				//모든 플레이어가 접속하면 서버 캐릭터 생성
				if (isBot)replicaManager3->Reference(playerBotReplica);
				else replicaManager3->Reference(playerReplica);

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
				RakNet::Connection_RM3* connection = replicaManager3->AllocConnection(serverSystemAddress, rakPeer->GetGuidFromSystemAddress(serverSystemAddress));
				//replicaManager3에 추적될 수 있도록 할당
				replicaManager3->PushConnection(connection);
				
				//객체 생성
				if(isBot)replicaManager3->Reference(playerBotReplica);
				else replicaManager3->Reference(playerReplica);
			}
			
			//SwitchNextScene() 에서 연결하는 것으로 변경 -> 왜 이렇게 바꾸자고 했지?
			//그렇게 변경하면 수신 딜레이 적용시 timeout으로 연결 수립이 안됩니다..
		}
		break;
		case ID_CONNECTION_ATTEMPT_FAILED:
		{
			//PushMessage(RakNet::RakString("Connection attempt to ") + targetName + RakNet::RakString(" failed."));
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
				br->shotLifetime = RakNet::GetTimeMS() + shootFromOrigin(pos, target, gamePlatform); /*원래는 gamePlatform이 보낸 얘의 플랫폼이어야 함*/
				br->bulletCount = bulletCnt;
				replicaManager3->Reference(br);
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

				if (shooterName == playerReplica->playerName) playerReplica->killCnt++;
				else if (holderName == playerReplica->playerName) playerReplica->deathCnt++;
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

			for (int idx = 0; idx < PlayerReplica::playerList.Size(); ++idx)
			{
				PlayerReplica* player = PlayerReplica::playerList[idx];
				if (player->creatingSystemGUID == LifeUpdateGuid)
				{
					player->isDead = isDead;
					if (isDead == false) {
						player->bulletCoolTime = -1;
					}
					
					if (isDead == false && LifeUpdateGuid == rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS) && player->isBot && isServer == false) {
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
						Respawn(playerBotReplica->respawnPos, playerBotReplica->respawnTarget);
						botMoveTime = RakNet::GetTimeMS() + BOT_MOVE_TIME;

						RakNet::BitStream bs;
						bs.Write((RakNet::MessageID)CDemo::ID_GAME_MESSAGE_PLAYER_RESPAWN);
						bs.Write(playerBotReplica->creatingSystemGUID);
						bs.Write(playerBotReplica->respawnPos);
						bs.Write(playerBotReplica->respawnTarget);
						rakPeer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
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

				for (int i = 0; i < PlayerReplica::playerList.Size(); i++) {
					if (PlayerReplica::playerList[i]->creatingSystemGUID == g) {
						PlayerReplica::playerList[i]->playerName = name;

						//만약 현재 플랫폼이 Android (Holder) 이고, 다른 플랫폼이 PC (Shooter)인 경우 (그 반대도 포함)
						//Android의 위치를 GUI에 띄울 수 있도록 한다
						if (playerReplica->gamePlatform == Holder && PlayerReplica::playerList[i]->gamePlatform == Shooter)
							SetHolderPosText(playerReplica->position); 
						else if (playerReplica->gamePlatform == Shooter && PlayerReplica::playerList[i]->gamePlatform == Holder)
							SetHolderPosText(PlayerReplica::playerList[i]->model->getPosition());
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

			//리스폰 요청 보낸 봇의 GUID
			bsIn.Read(botGuid);
			bsIn.Read(respawnPos);
			bsIn.Read(respawnTarget);

			if (isServer) {
				//보낸 이를 제외한 모두에게 다시 브로드캐스팅
				RakNet::BitStream bs;
				bs.Write((RakNet::MessageID)CDemo::ID_GAME_MESSAGE_PLAYER_RESPAWN);
				bs.Write(botGuid);
				bs.Write(respawnPos);
				bs.Write(respawnTarget);
				rakPeer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, botGuid, true);
			}

			for (int idx = 0; idx < PlayerReplica::playerList.Size(); ++idx)
			{
				PlayerReplica* player = PlayerReplica::playerList[idx];
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

			if (matchState == GameMatchState::GAME_MATCH_START) {
				isGameStart = true;
			}
			else if (matchState == GameMatchState::GAME_MATCH_END) {
				isGameEnd = true;
			}
		}
		break;
		}	
	}

	// Call the Update function for networked game objects added to BaseIrrlichtReplica once the game is ready
	if (currentScene >= 1)
	{
		//(서버 봇을 제외한) 모든 플레이어가 생성되면 서버 측에서 이름을 할당
		//클라 측에서 이름을 할당하는게 더 편하나 현재 모바일에서 이름을 할당하기 어려운 문제가 있어서 서버 측에서 할당.
		if (isServer && isPlayersNameSet == false && logCount == PlayerReplica::playerList.Size()- serverPlayerCnt){
			isPlayersNameSet = true;
			int playerCnt = 1; int botCnt = 1;
			RakNet::RakString serverNames("My Name : ");
			
			RakNet::BitStream bs;
			bs.Write((RakNet::MessageID)CDemo::ID_GAME_MESSAGE_PLAYER_NAME);
			bs.Write(PlayerReplica::playerList.Size());
			for (int i = 0; i < PlayerReplica::playerList.Size(); i++) {
				PlayerReplica* player = PlayerReplica::playerList[i];
				if (player->isBot) {
					player->playerName = RakNet::RakString("bot%d", botCnt++);
				}
				else {
					player->playerName = RakNet::RakString("Player%d", playerCnt++); 
				}

				if (player->gamePlatform == Server)
					player->playerName += RakNet::RakString(" (S)");
				else if (player->gamePlatform == Shooter)
					player->playerName += RakNet::RakString(" (PC)");
				else if (player->gamePlatform == Holder)
					player->playerName += RakNet::RakString(" (M)");
				
				if (player->creatingSystemGUID == rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))
					serverNames += player->playerName + RakNet::RakString(" ");
				
				bs.Write(player->playerName);
				bs.Write(player->creatingSystemGUID); 
				//DebugPrintf("Player Name : %s / GUID : %s\n", player->playerName.C_String(), player->creatingSystemGUID.ToString());
			}

			rakPeer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);

			//서버 측 이름은 여기서 할당
			const char* charStr = serverNames.C_String();  // 멀티바이트 문자열 얻기
			wchar_t wcharStr[128];
			mbstowcs(wcharStr, charStr, sizeof(wcharStr) / sizeof(wchar_t));
			myNameText->setText(wcharStr);
			PushMessage(RakNet::RakString("Bot Name Update"));
		}

		//게임 시작 확인
		if (isServer && gameStartTime <= curTime && isGameStart == false && gameStartTime !=0) {
			isGameStart = true;
			PushMessage(RakNet::RakString("Game Start!"));

			//모든 플레이어에게 게임 시작 알리기
			RakNet::BitStream bs;
			bs.Write((RakNet::MessageID)CDemo::ID_GAME_MESSAGE_GAME_MATCH);
			bs.Write(GameMatchState::GAME_MATCH_START);
			rakPeer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
			gameStartTime = curTime; //게임 시작 시간 기록

#if QOS_SUPPORTED 
			sem.setKey(888);
			sem.setupSemaphore(0);
			write_shm.setKey(777);
			write_shm.setupSharedMemory(1200);
			write_shm.attachSharedMemory();

			WriteQoSInfo();
#endif
		}

		//모든 네트워크 객체 업데이트
		unsigned int idx;
		bool isTimeRecorded = false;
		for (idx = 0; idx < replicaManager3->GetReplicaCount(); idx++) {
			((BaseIrrlichtReplica*)(replicaManager3->GetReplicaAtIndex(idx)))->Update(curTime);
			

			//게임 끝내기
			if (isServer && isGameEnd && isLogged) {
				char buffer[200];
				char timeBuf[50];
				PlayerReplica* pr = dynamic_cast<PlayerReplica*>(replicaManager3->GetReplicaAtIndex(idx));
				
				if (pr != nullptr) {
					//첫줄은 경과 시간 기록
					if (!isTimeRecorded) {
						isTimeRecorded = true;
						snprintf(timeBuf, sizeof(timeBuf), "Game End! Total Time : %02u MS\n", (curTime - gameStartTime));
						statBuf.Push(RakNet::RakString(timeBuf), _FILE_AND_LINE_); //Log
					}

					//name/k/d/shoot cnt
					snprintf(buffer, sizeof(buffer), "%s/%d/%d/%d\n", pr->playerName.C_String(), pr->killCnt, pr->deathCnt,pr->shootCnt);
					statBuf.Push(RakNet::RakString(buffer), _FILE_AND_LINE_); //Log
				}
			}
		}

		if (isGameEnd) {
			if (isServer) {
				RakNet::BitStream bs;
				bs.Write((RakNet::MessageID)CDemo::ID_GAME_MESSAGE_GAME_MATCH);
				bs.Write(GameMatchState::GAME_MATCH_END);
				rakPeer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);

#if QOS_SUPPORTED
				//끝났음
#endif
			}
			device->closeDevice();
		}

	}
}

bool CDemo::IsKeyDown(EKEY_CODE keyCode) const {return KeyIsDown[keyCode];}
bool CDemo::IsMovementKeyDown(void) const {
	if (isKeyLock) return false;

#ifdef __ANDROID__
	return (curTouchID.move != -1) | 
		KeyIsDown[KEY_UP] |
		KeyIsDown[KEY_DOWN] |
		KeyIsDown[KEY_LEFT] |
		KeyIsDown[KEY_RIGHT] |
		KeyIsDown[KEY_KEY_W] |
		KeyIsDown[KEY_KEY_S] |
		KeyIsDown[KEY_KEY_A] |
		KeyIsDown[KEY_KEY_D];
#else
	return KeyIsDown[KEY_UP] |
		KeyIsDown[KEY_DOWN] |
		KeyIsDown[KEY_LEFT] |
		KeyIsDown[KEY_RIGHT] |
		KeyIsDown[KEY_KEY_W] |
		KeyIsDown[KEY_KEY_S] |
		KeyIsDown[KEY_KEY_A] |
		KeyIsDown[KEY_KEY_D];
#endif // __ANDROID__
}
void CDemo::PushMessage(RakNet::RakString rs)
{
	outputMessages.Push(rs,_FILE_AND_LINE_);
	if (whenOutputMessageStarted==0)
	{
		whenOutputMessageStarted=RakNet::GetTimeMS();
	}
}
const char *CDemo::GetCurrentMessage(void)
{
	if (outputMessages.GetSize()==0)
		return "";
	RakNet::TimeMS curTime = RakNet::GetTimeMS();
	if (curTime-whenOutputMessageStarted>2500)
	{
		outputMessages.Pop(_FILE_AND_LINE_);
		whenOutputMessageStarted=curTime;
	}

	if (outputMessages.GetSize()==0)
	{
		whenOutputMessageStarted=0;
		return "";
	}
	return outputMessages.Peek().C_String();
}

RakNet::RakString CDemo::GetCurrentKillLogMessage(void)
{
	int ItemCnt = killLogMessages.Size();
	RakNet::RakString result = RakNet::RakString("");
	if (ItemCnt == 0)
		return RakNet::RakString("");
	
	if (ItemCnt > 5) {
		for (int i = 0; i < ItemCnt - 5; i++) killLogMessages.Pop();
		ItemCnt = killLogMessages.Size();
	}
	
	for (int i = 0; i < ItemCnt; i++)
	{
		RakNet::RakString msg = killLogMessages[i].message;
		RakNet::TimeMS time = killLogMessages[i].timeStamp;

		if (time + 5000 < RakNet::GetTimeMS()) // 5초 이상 지난 메시지는 삭제
		{
			killLogMessages.Pop();
			ItemCnt--;
			i--;
			continue;
		}
		result += msg;
	}
	
	return result;
}

void CDemo::SetTransformCamera(scene::ICameraSceneNode* camera, GamePlatform platform) {
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

void CDemo::SetResetBot() {
	if (!playerBotReplica) return;

	//Direction
	//-1 : Left, 0 : Down , 1 : Right
	dir = RandomInt(-1, 1);

	//Spawn Postion
	switch (dir)
	{
	case -1: {
		playerBotReplica->respawnPos = core::vector3df(-118.683563, 224.552368, -493.077454);
		playerBotReplica->respawnTarget = core::vector3df(-118.502869, 229.367813, 61.550100);
	}break;
	case 0: {
		playerBotReplica->respawnPos = RandomVector3(core::vector3df(-46.856121, 363.129883, -292.425385), core::vector3df(-46.856121, 363.129883 + 700, -292.425385));
		playerBotReplica->respawnTarget = core::vector3df(-518.377686, 332.969666, -276.827545);
	}
		  break;
	case 1: {
		playerBotReplica->respawnPos = core::vector3df(-86.982430, 217.035385, -140.506424);
		playerBotReplica->respawnTarget = core::vector3df(-87.819504, 224.415527, -413.191589);
	}
		  break;
	default:
		break;
	}

	//Speed
	if (fpsCamAnim) {
		fpsCamAnim->setMoveSpeed(RandomFloat(0.1f, 0.5f)); 
	}

	//Gravity
	float gravity = -10.0f;
	if (dir == 0) gravity = RandomFloat(-100.f, -10.f);
	
	if (fpsCamResponse) fpsCamResponse->setGravity(core::vector3df(0, gravity, 0));
}

void CDemo::SetHolderPosText(core::vector3df pos) {
	if (holderPosText == nullptr) return;
	RakNet::RakString msg("Hold Pos : %.2f, %.2f, %.2f", pos.X, pos.Y, pos.Z);
	wchar_t wcharStr[128];
	mbstowcs(wcharStr, msg.C_String(), sizeof(wcharStr) / sizeof(wchar_t));
	holderPosText->setText(wcharStr);
}

void CDemo::SetPlayerNameText() {
	if (myNameText == nullptr) return;
	RakNet::RakString msg("my Name : %s / K : %d / D : %d / S : %d", playerReplica->playerName.C_String(), playerReplica->killCnt, playerReplica->deathCnt, playerReplica->shootCnt);
	wchar_t wcharStr[128];
	mbstowcs(wcharStr, msg.C_String(), sizeof(wcharStr) / sizeof(wchar_t));
	myNameText->setText(wcharStr);
}

void CDemo::FlushMovementKeys()
{
	static const EKEY_CODE keys[] = {
		KEY_KEY_W, KEY_KEY_A, KEY_KEY_S, KEY_KEY_D,
		KEY_UP, KEY_LEFT, KEY_DOWN, KEY_RIGHT,
		KEY_SPACE
	};

	SEvent ev{};
	ev.EventType = EET_KEY_INPUT_EVENT;
	ev.KeyInput.PressedDown = false;

	for (EKEY_CODE k : keys) {
		KeyIsDown[k] = false;      // 내부 키 상태 해제
		ev.KeyInput.Key = k;       // 카메라에도 KeyUp 전달
		if (auto* cam = device->getSceneManager()->getActiveCamera())
			cam->OnEvent(ev);
	}
}

void CDemo::Respawn(core::vector3df& pos, core::vector3df& target)
{
	if (!(GetSceneManager()->getActiveCamera())) return;

	GetSceneManager()->getActiveCamera()->setPosition(pos);
	GetSceneManager()->getActiveCamera()->setTarget(target);
	
	if(fpsCamAnim) fpsCamAnim->setReset(true);
	if(fpsCamResponse) fpsCamResponse->setReset(true);
	
}

void CDemo::DrawCrosshairHUD()
{
	    video::IVideoDriver* driver = device->getVideoDriver();
		core::dimension2d<u32> size = driver->getScreenSize();
		const core::dimension2du orig = crosshairTex->getOriginalSize(); // 118×118
		core::rect<s32> srcRect(0, 0, (s32)orig.Width, (s32)orig.Height); // <= 이걸 사용

		s32 minPixel;
		if (size.Width <= size.Height) minPixel = size.Width;
		else minPixel = size.Height;

		// 원하는 스케일 픽셀(예: 64×64, 또는 화면 짧은 변의 10%)
		const s32 target = (s32)(minPixel * 0.10f);
		//const s32 target = 128;
		const s32 posX = (size.Width - target) / 2;
		const s32 posY = (size.Height - target) / 2;
		core::rect<s32> dstRect(posX, posY, posX + target, posY + target);

		video::SColor color(50, 255, 255, 255);
		video::SColor colors[4] = {color,color,color,color };

		device->getVideoDriver()->draw2DImage(crosshairTex, dstRect, srcRect, nullptr, colors, true);
}

#if QOS_SUPPORTED 
void CDemo::WriteQoSInfo() {
	//Port / UserListCnt / UserData (IP Address, Platform, RTT, FPS ) 
	sem.waitSemaphore();
	write_shm.clearSharedMemory();

	const int PORT = SERVER_PORT;
	string data = "";
	data += std::to_string(PORT) + std::string("|");
	data += std::to_string(PlayerReplica::playerList.Size() - 1) + std::string("*"); //서버 자신 제외

	//port|userCnt*UserData*UserData*UserData...
	//UserData = IP/Platform/RTT/FPS
	//20123|1*192.168.1.3/PC/3/144
	for (int idx = 0; idx < PlayerReplica::playerList.Size(); ++idx)
	{
		PlayerReplica* player = PlayerReplica::playerList[idx];
		if (player->creatingSystemGUID == rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))continue;
		data += rakPeer->GetSystemAddressFromGuid(player->creatingSystemGUID).ToString(false) + std::string("/");
		data += player->gamePlatform == Shooter ? "PC" : (player->gamePlatform == Holder ? "M" : "S"); 
		data += std::string("/");
		data += std::to_string(rakPeer->GetAveragePing(player->creatingSystemGUID) / 2) + std::string("/");
		data += std::to_string(player->fps);
		if (idx != PlayerReplica::playerList.Size() - 1) data += std::string("*");
	}


	write_shm.copyToSharedMemory((char*)(data.c_str()));
	sem.releaseSemaphore();
}

unsigned long CDemo::get_nsecs()
{
	//unsigned long now = get_nsecs(); or uint64_t now = get_nsecs();
	//alternative to bpf_ktime_get_ns (ref : https://stackoverflow.com/questions/60970877/xdp-bpf-is-there-an-user-space-alternative-to-bpf-ktime-get-ns)

	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000000000UL + ts.tv_nsec;
}

#endif

#ifdef USE_IRRKLANG
void CDemo::startIrrKlang()
{
	/*
	irrKlang = irrklang::createIrrKlangDevice();

	if (!irrKlang)
		return;

	// play music

	irrklang::ISound* snd = irrKlang->play2D(IRRLICHT_MEDIA_PATH "IrrlichtTheme.ogg", true, false, true);
	if ( !snd )
		snd = irrKlang->play2D("IrrlichtTheme.ogg", true, false, true);

	if (snd)
	{
		snd->setVolume(0.5f); // 50% volume
		snd->drop();
	}

	// preload both sound effects

	ballSound = irrKlang->getSoundSource(IRRLICHT_MEDIA_PATH "ball.wav");
	impactSound = irrKlang->getSoundSource(IRRLICHT_MEDIA_PATH "impact.wav");
	*/
}
#endif


#ifdef USE_SDL_MIXER
void CDemo::startSound()
{
	stream = NULL;
	ballSound = NULL;
	impactSound = NULL;

	SDL_Init(SDL_INIT_AUDIO);

	if (Mix_OpenAudio(22050, AUDIO_S16, 2, 128))
		return;

	stream = Mix_LoadMUS(IRRLICHT_MEDIA_PATH "IrrlichtTheme.ogg");
	if (stream)
		Mix_PlayMusic(stream, -1);

	ballSound = Mix_LoadWAV(IRRLICHT_MEDIA_PATH "ball.wav");
	impactSound = Mix_LoadWAV(IRRLICHT_MEDIA_PATH "impact.wav");
}

void CDemo::playSound(Mix_Chunk *sample)
{
	if (sample)
		Mix_PlayChannel(-1, sample, 0);
}

void CDemo::pollSound(void)
{
	SDL_Event event;

	while (SDL_PollEvent(&event))
		;
}
#endif
