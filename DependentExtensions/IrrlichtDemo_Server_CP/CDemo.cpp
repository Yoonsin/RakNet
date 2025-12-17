
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
#include <set>
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
DataStructures::List<DataStructures::List<RakNet::RakString>> statBufList; //statistics buffers for Method

CDemo::CDemo(bool f, bool m, bool s, bool a, bool v, bool fsaa, video::E_DRIVER_TYPE d, core::stringw& _playerName, bool isS, GamePlatform plat, bool isLog, int logCnt, const char* base, bool isBot, int winScore, int methodMask)
: fullscreen(f), music(m), shadows(s), additive(a), vsync(v), aa(fsaa),
driverType(d), device(0), playerName(_playerName), isServer(isS), gamePlatform(plat), isLogged(isLog), logCount(logCnt), baseDir(base), isBot(isBot), winScore(winScore), evalMask(0), eval1bool(false), um_cnt(0), am_cnt(0), sumScore(0), BOT_MOVE_TIME(5000), eval1cnt(0),
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
	eval3LogTime = 0;
	evalMask = methodMask;
	if (evalMask & METHOD_1)
		BOT_MOVE_TIME = 1000;
	statBufList.Preallocate(5, _FILE_AND_LINE_);
	for (int i = 0; i < 5; i++)
		statBufList.Push(DataStructures::List<RakNet::RakString>(), _FILE_AND_LINE_); //Method 0 unused
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
				if((~evalMask) & METHOD_2)
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
			//if(crosshairTex) DrawCrosshairHUD();		
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

		// [추가] 1초마다 네트워크 통계 로그 출력 (Method 3가 활성화된 경우)
		if (isServer && (evalMask & METHOD_3))
		{
			RakNet::TimeMS currentTime = RakNet::GetTimeMS();
			if (currentTime - eval3LogTime >= 1000) // 1000ms = 1초
			{
				PrintStatistics(false, 3); // Method 3 통계 출력
				eval3LogTime = currentTime;
			}
		}
		
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
	}

	// RakNet shutdown
	DeinitializeRakNetClasses(isLogged, isBot,baseDir, evalMask);
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
				if (dir != 0 && evalMask == 0) {
					botKeyEvent.KeyInput.PressedDown = KeyIsDown[botKeyEvent.KeyInput.Key] = true;
					device->getSceneManager()->getActiveCamera()->OnEvent(botKeyEvent);
				}
				//DebugPrintf("go / timeDiff : %d\n", t-preT);
			}
			else {
				if (dir != 0 && evalMask == 0) {
					botKeyEvent.KeyInput.PressedDown = KeyIsDown[botKeyEvent.KeyInput.Key] = false;
					device->getSceneManager()->getActiveCamera()->OnEvent(botKeyEvent);
				}
				
				SetResetBot();
				Respawn(playerBotReplica->respawnPos, playerBotReplica->respawnTarget);
				botMoveTime = t + BOT_MOVE_TIME;


				RakNet::BitStream bs;
				if (evalMask & METHOD_1) {
					if (eval1bool) eval1cnt++;
					if (eval1cnt > 100) {
						isGameEnd = true;
						return;
					}

					bs.Write((RakNet::MessageID)ID_TIMESTAMP);
					bs.Write(RakNet::GetTime());
				}
				
				bs.Write((RakNet::MessageID)CDemo::ID_GAME_MESSAGE_PLAYER_RESPAWN);
				bs.Write(playerBotReplica->creatingSystemGUID);
				bs.Write(playerBotReplica->respawnPos);
				bs.Write(playerBotReplica->respawnTarget);
				bs.Write(eval1bool);

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

	// -----------------------------------------------------------
	// 1. 패킷 수신 및 처리 루프
	// -----------------------------------------------------------
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
		case ID_NEW_INCOMING_CONNECTION:
		{
			PushMessage(RakNet::RakString("Sending player list to new connection"));
			RakNet::Connection_RM3* connection = replicaManager3->AllocConnection(packet->systemAddress, rakPeer->GetGuidFromSystemAddress(packet->systemAddress));
			replicaManager3->PushConnection(connection);

			if (logCount == replicaManager3->GetConnectionCount())
			{
				if (isBot) replicaManager3->Reference(playerBotReplica);
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

						rakPeer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, botGuid, true);
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
						bs.Write(false);
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

			if (matchState == GameMatchState::GAME_MATCH_START) isGameStart = true;
			else if (matchState == GameMatchState::GAME_MATCH_END) isGameEnd = true;
		}
		break;
		}	
	}

	// -----------------------------------------------------------
	// 2. 네트워크 객체 업데이트 (ReplicaManager3)
	// -----------------------------------------------------------
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
				if (player->isBot) player->playerName = RakNet::RakString("bot%d", botCnt++);
				else player->playerName = RakNet::RakString("Player%d", playerCnt++);

				if (player->gamePlatform == Server) player->playerName += RakNet::RakString(" (S)");
				else if (player->gamePlatform == Shooter) player->playerName += RakNet::RakString(" (PC)");
				else if (player->gamePlatform == Holder) player->playerName += RakNet::RakString(" (M)");

				if (player->creatingSystemGUID == rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))
					serverNames += player->playerName + RakNet::RakString(" ");

				bs.Write(player->playerName);
				bs.Write(player->creatingSystemGUID);
			}

			rakPeer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);

			//서버 측 이름은 여기서 할당
			const char* charStr = serverNames.C_String();
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
			sem.setKey(888); sem.setupSemaphore(0);
			write_shm.setKey(777); write_shm.setupSharedMemory(1200); write_shm.attachSharedMemory();
			WriteQoSInfo(); isQosWritten = true; QosWriteTime = curTime + QOS_WRITE_COOL_TIME;
#endif
		}

		// 객체 업데이트 및 게임 종료 통계 저장
		bool isTimeRecorded = false;
		for (unsigned int idx = 0; idx < replicaManager3->GetReplicaCount(); idx++) {
			((BaseIrrlichtReplica*)(replicaManager3->GetReplicaAtIndex(idx)))->Update(curTime);

			if (isServer && isGameEnd && isLogged && (evalMask == 0)) {
				PlayerReplica* pr = dynamic_cast<PlayerReplica*>(replicaManager3->GetReplicaAtIndex(idx));
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
			for (unsigned int idx = 0; idx < PlayerReplica::playerList.Size(); idx++)
			{
				PlayerReplica* player = PlayerReplica::playerList[idx];
				// 서버 로컬 봇은 제외.. 왜 안됨?
				if (player->creatingSystemGUID == rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))
					continue;

				RakNet::TimeMS SampleRTT = rakPeer->GetLastPing(player->creatingSystemGUID);
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
				for (unsigned int p_idx = 0; p_idx < PlayerReplica::playerList.Size(); ++p_idx)
				{
					PlayerReplica* player_j = PlayerReplica::playerList[p_idx];

					// 제외 대상: 서버 자신, 메시지 M_k의 발신자
					if (player_j->creatingSystemGUID == rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS)) continue;
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
					for (unsigned int i = 0; i < PlayerReplica::playerList.Size(); ++i) {
						if (PlayerReplica::playerList[i]->creatingSystemGUID == top.playerGUID) {
							sender = PlayerReplica::playerList[i];
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
				rakPeer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
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
	if (curTime-whenOutputMessageStarted>500)
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
	//그 외 : 고정위치
	dir = RandomInt(-1, 1);
	if (evalMask & METHOD_1) dir = 2;
	if (evalMask & METHOD_2) dir = 3;

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
	case 2: {
		if (eval1bool) {
			//떨어진 곳
			playerBotReplica->respawnPos = core::vector3df(-118.683563, 224.552368, -493.077454);
			playerBotReplica->respawnTarget = core::vector3df(-118.502869, 229.367813, 61.550100);
		}
		else {
			//본래 위치
			playerBotReplica->respawnPos = core::vector3df(-46.856121, 217.035385, -292.425385);
			playerBotReplica->respawnTarget = core::vector3df(-518.377686, 332.969666, -276.827545);
		}
		eval1bool = !eval1bool;
	}
		break;
	case 3: {
		//원래 죽었던 곳에서 부활
		playerBotReplica->respawnPos = playerBotReplica->position;
		playerBotReplica->respawnTarget = core::vector3df(-518.377686, 332.969666, -276.827545);
	}
		break;
	default:
		break;
	}

	//메소드 2같은 경우는 고정 값으로
	if (evalMask & METHOD_2) return;

	//Speed
	if (fpsCamAnim) {
		fpsCamAnim->setMoveSpeed(RandomFloat(0.1f, 0.5f)); 
	}

	//Gravity
	float gravity = -10.0f;
	if (dir == 0 ) gravity = RandomFloat(-100.f, -10.f);
	if (dir == 2) gravity = 0;
	
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

void CDemo::BulletHitDetected(RakNet::RakNetGUID creatingSystemGUID, float ingoingTimeMS) {
	if (isServer == false) return;
	
	unsigned int idx;
	scene::ISceneManager* sm = GetSceneManager();
	scene::ICameraSceneNode* camera = sm->getActiveCamera();

	// Time Warp
	// 쏜 사람 shootPosition, shootDirection 확인
	// 나머지 사람 Transform 확인
	core::vector3df start;
	core::vector3df end;
	core::line3d<irr::f32> line;
	core::triangle3df triangle;
	core::vector3df hitPoint;
	const scene::ISceneNode* hitNode;

	RakNet::TimeMS now = RakNet::GetTimeMS() - (rakPeer->GetAveragePing(creatingSystemGUID) / 2) - INTERP_TIME_MS - ingoingTimeMS; //Rewind Time = Server Current Time - RTT/2 - Client View Interpolation Time - Ingoing Delay Time
	bool wallHit = false;
	core::vector3df wallHitPoint;
	PlayerReplica* shooter = nullptr;
	for (idx = 0; idx < PlayerReplica::playerList.Size(); ++idx)
	{
		auto* player = PlayerReplica::playerList[idx];
		auto* q = player->fq;
		bool didTimeWarp = false;

		if (player->creatingSystemGUID == creatingSystemGUID) {
			shooter = player;
			shooter->shootCnt++;

#if QOS_SUPPORTED 
			//점수 득점 정보 기록
			// //Agent 통계와 시간대 같이 맞추기 위해 demo->get_nsecs() 사용
			HitInfo info = { rakPeer->GetSystemAddressFromGuid(shooter->creatingSystemGUID).ToString(false),  rakPeer->GetSystemAddressFromGuid(shooter->creatingSystemGUID).ToString(false) , shooter->killCnt, get_nsecs() };
			RakNet::RakString str("%s/%d/%lu\n", info.shooterAddr, info.nowScore, info.timeStamp);
			statBufList[0].Push(RakNet::RakString(str), _FILE_AND_LINE_); //Log
#endif
		}

		//ball position은 클라에서 camPosition으로 설정한 값
		//time warp에서는 위 값을 쓰지 않고 매번 playerReplica를 통해서 camPosition을 받아와 그 값을 쓴다
		for (int frameIdx = 1; frameIdx < q->Size(); frameIdx++) {
			if ((*q)[frameIdx].timeStamp >= now) {
				//now가 frameIdx-1 과 frameIdx 사이에 있으므로 보간
				const FrameState& f0 = (*q)[frameIdx - 1];
				const FrameState& f1 = (*q)[frameIdx];
				float alpha = float(now - f0.timeStamp) / float(f1.timeStamp - f0.timeStamp);

				if (player->creatingSystemGUID == creatingSystemGUID) {
					//Shooter
					//Shot Position 보간
					start = f0.shotPosition.getInterpolated(f1.shotPosition, alpha);
					//Shot Direction 보간 및 정규화
					core::vector3df interpolatedShotDir = f0.shotDirection.getInterpolated(f1.shotDirection, alpha);
					interpolatedShotDir.normalize();
					end = start + (interpolatedShotDir * camera->getFarValue());
					line.setLine(start, end);

					//벽 충돌 판정 및 비주얼 애니메이션
					shootFromOrigin(start, interpolatedShotDir, start, end, wallHit, wallHitPoint, player->gamePlatform);
				}
				else {
					//Other Player
					//Collision Transform 보간
					for (int i = 0; i < 16; ++i)
						player->collisionTransform.pointer()[i] = (f0.collisionTransform.pointer()[i]) * (1.0f - alpha) + (f1.collisionTransform.pointer()[i]) * alpha;
				}
				didTimeWarp = true;
				break;
			}
		}

		// fallback 처리: 보간 실패 시 가장 최신 값 사용
		if (!didTimeWarp && q->Size() >= 1) {
			const FrameState& lastFrame = q->PeekTail();
			if (player->creatingSystemGUID == creatingSystemGUID) {
				start = lastFrame.shotPosition;
				core::vector3df dir = lastFrame.shotDirection;
				dir.normalize();
				end = start + dir * camera->getFarValue();
				line.setLine(start, end);

				//벽 충돌 판정 및 비주얼 애니메이션
				shootFromOrigin(lastFrame.shotPosition, lastFrame.shotDirection, start, end, wallHit, wallHitPoint, player->gamePlatform);
			}
			else {
				player->collisionTransform = lastFrame.collisionTransform;
			}
		}
	}


	RakNet::TimeMS debugTime = RakNet::GetTimeMS();
	PlayerReplica* holder = nullptr;
	for (idx = 0; idx < PlayerReplica::playerList.Size(); ++idx)
	{
		auto* player = PlayerReplica::playerList[idx];
		if (player->creatingSystemGUID == creatingSystemGUID) {
			player->PlayAttackAnimation();
			continue; // 총을 쏜 본인
		}
		holder = player;

		scene::ITriangleSelector* selector = nullptr;
		scene::ISceneNode* node = nullptr;

		if (holder->IsDead()) continue;
		if (holder->isBot == false) continue; //현재는 봇 이외의 플레이어는 안맞도록 설정

		selector = CreateSelectorFromTransformedBox(GetSyndeyBoundingBox(), holder->collisionTransform, sm, holder->creatingSystemGUID);

		if (selector == nullptr) continue;
		//DrawDebugFrame(selector, 1000);


		// 충돌 검사
#ifdef __ANDROID__
		scene::SCollisionHit hitResult;
		bool hit = sm->getSceneCollisionManager()->getCollisionPoint(hitResult, line, selector);
		hitPoint = hitResult.Intersection;
		triangle = hitResult.Triangle;
		hitNode = hitResult.Node;
#else 
		bool hit = sm->getSceneCollisionManager()->getCollisionPoint(line, selector, hitPoint, triangle, hitNode);
#endif // __ANDROID__

		selector->drop();

		if (hit && hitNode && hitNode->getID() == static_cast<s32>(holder->creatingSystemGUID.g))
		{
			if (wallHit) {
				//벽에 부딫히면 플레이어 맞음 처리하면 안됨
				float distToPlayer = line.start.getDistanceFrom(hitPoint);
				float distToWall = line.start.getDistanceFrom(wallHitPoint);
				//DebugPrintf("distToPlayer : %f, distToWall : %f\n", distToPlayer, distToWall);
				if (distToWall < distToPlayer) continue;
			}

			if (holder->fq) holder->fq->Clear(_FILE_AND_LINE_);

			//Spawn Time
			if (holder->isBot) holder->deathTimeout = RakNet::GetTimeMS() + RandomInt(3000, 5000);
			else holder->deathTimeout = RakNet::GetTimeMS() + 3000;

			printf("HIT! : %d\n", ++sumScore);
			RakNet::RakString msg("%s Dead from : %s",
				holder->isBot ? "Bot" : "Player",
				rakPeer->GetSystemAddressFromGuid(shooter->creatingSystemGUID).ToString(true));
			PushMessage(msg);
			//OutputDebugStringA(msg.C_String());

			//점수 득점
			shooter->killCnt++;
			holder->deathCnt++;
			//목표 점수에 도달하면 게임 끝
			if (shooter->killCnt == winScore) {
				isGameEnd = true;
			}

#if QOS_SUPPORTED 
			//점수 득점 정보 기록
			//Agent 통계와 시간대 같이 맞추기 위해 demo->get_nsecs() 사용
			HitInfo info = { rakPeer->GetSystemAddressFromGuid(shooter->creatingSystemGUID).ToString(false), rakPeer->GetSystemAddressFromGuid(holder->creatingSystemGUID).ToString(false), shooter->killCnt, get_nsecs() };
			if (info.holderAddr == SERVER_IP_LOCAL)
				info.holderAddr = SERVER_IP;
			else if (info.shooterAddr == SERVER_IP_LOCAL)
				info.shooterAddr = SERVER_IP;

			RakNet::RakString str("%s/%s/%d/%lu\n", info.shooterAddr, info.holderAddr, info.nowScore, info.timeStamp);
			statBufList[0].Push(RakNet::RakString(str), _FILE_AND_LINE_); //Log
#endif

			RakNet::BitStream bs;
			bs.Write((RakNet::MessageID)CDemo::ID_GAME_MESSAGE_PLAYER_LIFE);
			bs.Write((shooter->creatingSystemGUID));       //Shooter 
			bs.Write(holder->IsDead());
			holder->wasDead = true;
			bs.Write(holder->creatingSystemGUID); //Holder
			bs.Write(shooter->playerName);        //Shooter Name
			bs.Write(holder->playerName);         //Holder Name

			if (holder->isBot && holder->creatingSystemGUID == rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS)) {
				SEvent botKeyEvent;
				botKeyEvent.EventType = EET_KEY_INPUT_EVENT;
				botKeyEvent.KeyInput.Key = KEY_KEY_W;
				botKeyEvent.KeyInput.PressedDown = false;
				KeyIsDown[botKeyEvent.KeyInput.Key] = botKeyEvent.KeyInput.PressedDown;
				if (GetDevice()->getSceneManager()->getActiveCamera()) {
					GetDevice()->getSceneManager()->getActiveCamera()->OnEvent(botKeyEvent);
				}
			}

			KillLog logEntry{ shooter->playerName + RakNet::RakString(" -> ") + holder->playerName + RakNet::RakString("\n"), RakNet::GetTimeMS() };
			killLogMessages.Push(logEntry, _FILE_AND_LINE_); // Record the kill log message

			rakPeer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
			break; // 더 검사하지 않음
		}
	}
}

#if QOS_SUPPORTED 
void CDemo::WriteQoSInfo() {
	//Port / UserListCnt / MethodMask /UserData (IP Address, Platform, RTT, FPS ) 
	sem.waitSemaphore();
	write_shm.clearSharedMemory();

	const int PORT = SERVER_PORT;
	string data = "";
	data += std::to_string(PORT) + std::string("|");
	data += std::to_string(PlayerReplica::playerList.Size() - 1) + std::string("|");
	data += std::to_string(evalMask) + std::string("*"); //서버 자신 제외

	//port|userCnt*UserData*UserData*UserData...
	//UserData = IP/Platform/RTT(AveragePing)/ LastPing(LastPing)/2 /FPS
	//20123|1*192.168.1.3/PC/3/144
	for (int idx = 0; idx < PlayerReplica::playerList.Size(); ++idx)
	{
		PlayerReplica* player = PlayerReplica::playerList[idx];
		if (player->creatingSystemGUID == rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS))continue;
		
		RakNet::SystemAddress addr = rakPeer->GetSystemAddressFromGuid(player->creatingSystemGUID);
		data += addr.ToString(false) + std::string("/");
		data += player->gamePlatform == Shooter ? "PC" : (player->gamePlatform == Holder ? "M" : "S"); 
		data += std::string("/");
		data += std::to_string(rakPeer->GetAveragePing(player->creatingSystemGUID)) + std::string("/");
		data += std::to_string(rakPeer->GetLastPing(player->creatingSystemGUID)) + std::string("/");
		data += std::to_string(player->fps);
		if (idx != PlayerReplica::playerList.Size() - 1) data += std::string("*");

		rakPeer->Ping(addr); //RTT의 빠른 갱신을 위한 핑 요청
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
