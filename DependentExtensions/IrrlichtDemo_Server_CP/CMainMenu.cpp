// This is a Demo of the Irrlicht Engine (c) 2005-2008 by N.Gebhardt.
// This file is not documented.

#include "CMainMenu.h"
#include <cwchar>

#ifdef __ANDROID__
#include "android_tools.h"
#include <sys/auxv.h>
#endif


//! we want the lights follow the model when it's moving
class CSceneNodeAnimatorFollowBoundingBox : public irr::scene::ISceneNodeAnimator
{
public:

	//! constructor
	CSceneNodeAnimatorFollowBoundingBox(irr::scene::ISceneNode* tofollow,
			const core::vector3df &offset, u32 frequency, s32 phase)
		: Offset(offset), ToFollow(tofollow), Frequency(frequency), Phase(phase)
	{
		if (ToFollow)
			ToFollow->grab();
	}

	//! destructor
	virtual ~CSceneNodeAnimatorFollowBoundingBox()
	{
		if (ToFollow)
			ToFollow->drop();
	}

	//! animates a scene node
	virtual void animateNode(irr::scene::ISceneNode* node, u32 timeMs)
	{
		if (0 == node || node->getType() != irr::scene::ESNT_LIGHT)
			return;

		irr::scene::ILightSceneNode* l = (irr::scene::ILightSceneNode*) node;

		if (ToFollow)
		{
			core::vector3df now = l->getPosition();
			now += ToFollow->getBoundingBox().getCenter();
			now += Offset;
			l->setPosition(now);
		}

		
		
#ifdef __ANDROID__
		irr::video::SColorHSL color;
		irr::video::SColorf rgb(0);
		color.Hue = ((timeMs + Phase) % Frequency) * (2.f * irr::core::PI / Frequency);
		color.Saturation = 1.f;
		color.Luminance = 0.5f;
		irr::video::SColorf rgb_color = rgb.toSColor();
		color.toRGB(rgb_color);
		irr::video::SColorf rgb_true(rgb_color);

		video::SLight light = l->getLightData();
		light.DiffuseColor = rgb_true; //rgb;
		l->setLightData(light);
#else
		irr::video::SColorHSL color;
		irr::video::SColorf rgb(0);
		color.Hue = ((timeMs + Phase) % Frequency) * (2.f * irr::core::PI / Frequency);
		color.Saturation = 1.f;
		color.Luminance = 0.5f;
		irr::video::SColor rgb_color = rgb.toSColor();
		color.toRGB(rgb_color);
		irr::video::SColorf rgb_true(rgb_color);

		video::SLight light = l->getLightData();
		light.DiffuseColor = rgb_true; //rgb;
		l->setLightData(light);
#endif
		
	}

	virtual scene::ISceneNodeAnimator* createClone(scene::ISceneNode* node, scene::ISceneManager* newManager=0) {return 0;}
private:

	core::vector3df Offset;
	irr::scene::ISceneNode* ToFollow;
	s32 Frequency;
	s32 Phase;
};


CMainMenu::CMainMenu()
: startButton(0), MenuDevice(0), selected(2), start(false),
shadows(false), additive(false), transparent(true), vsync(false), aa(false), isServer(false), isBot(false), isLocalServer(false),
#ifdef _DEBUG
	fullscreen(false), music(false)
#else
	fullscreen(true), music(true)
#endif
{
}


bool CMainMenu::run(bool& outFullscreen, bool& outMusic, bool& outShadows,
			bool& outAdditive, bool& outVSync, bool& outAA,
			video::E_DRIVER_TYPE& outDriver, core::stringw &playerName, bool& outIsServer, int& outLogCount, bool& outIsBot, bool& outIsLocalServer)
{
	
	video::E_DRIVER_TYPE driverType;
	irr::core::stringc mediaPath;
#ifdef __ANDROID__
	driverType = video::EDT_OGLES2;
	mediaPath = "media/"; //"irrlicht/media/";
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
	MenuDevice = createDeviceEx(param);
	
#else
#ifdef _WIN32
	driverType = video::EDT_DIRECT3D9;
	mediaPath = "C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/IrrlichtMedia/";
#else
	driverType = video::EDT_OPENGL;
	mediaPath = "../../../src/IrrlichtMedia/";
#endif // _WIN32

	
	
	MenuDevice = createDevice(driverType,
		core::dimension2d<u32>(512, 384), 16, false, false, false, this);
#endif //__ANDROID__

	if (MenuDevice->getFileSystem()->existFile("irrlicht.dat"))
		MenuDevice->getFileSystem()->addFileArchive("irrlicht.dat", true, true, io::EFAT_ZIP);
	else
		MenuDevice->getFileSystem()->addFileArchive(mediaPath + "irrlicht.dat", true, true, io::EFAT_ZIP);

	video::IVideoDriver* driver = MenuDevice->getVideoDriver();
	scene::ISceneManager* smgr = MenuDevice->getSceneManager();
	gui::IGUIEnvironment* guienv = MenuDevice->getGUIEnvironment();
	io::IFileSystem* fs = MenuDevice->getFileSystem();
	ILogger* logger = MenuDevice->getLogger();

#ifdef __ANDROID__
	
	
	ANativeWindow* nativeWindow = static_cast<ANativeWindow*>(driver->getExposedVideoData().OGLESAndroid.Window);
    int32_t windowWidth = ANativeWindow_getWidth(state->window);
    int32_t windowHeight = ANativeWindow_getHeight(state->window);
	core::dimension2d<s32> dim(driver->getScreenSize());
	
	char strDisplay[1000];
	sprintf(strDisplay, "Window size:(%d/%d)\nDisplay size:(%d/%d)\ngetScreenSize:(%d/%d)", windowWidth, windowHeight, displayMetrics.widthPixels, displayMetrics.heightPixels, dim.Width, dim.Height);
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
#endif // __ANDROID__

	core::stringw str = "Irrlicht Engine Demo v";
	str += MenuDevice->getVersion();
	MenuDevice->setWindowCaption(str.c_str());

	// set new Skin
	gui::IGUISkin* newskin = guienv->createSkin(gui::EGST_BURNING_SKIN);
	guienv->setSkin(newskin);
	newskin->drop();

	// load font
	gui::IGUIFont* font = guienv->getFont(mediaPath +  "fonthaettenschweiler.bmp");
	if (font)
		guienv->getSkin()->setFont(font);


#ifdef __ANDROID__
	// add images
	const s32 leftX = displayMetrics.widthPixels/2;
	const s32 leftY = displayMetrics.heightPixels/2;

	// add tab control
	gui::IGUITabControl* tabctrl = guienv->addTabControl(core::rect<int>(leftX, 10, displayMetrics.widthPixels - 10, displayMetrics.heightPixels - 10),
		0, true, true);
	gui::IGUITab* optTab = tabctrl->addTab(L"Demo");
	gui::IGUITab* aboutTab = tabctrl->addTab(L"About");

	// add list 
	/*gui::IGUIListBox* box = guienv->addListBox(core::rect<int>(10, 10, 800, 300 ), optTab, 1);
	box->addItem(L"OpenGL 1.5");
	box->addItem(L"Direct3D 8.1");
	box->addItem(L"Direct3D 9.0c");
	box->addItem(L"Burning's Video 0.39");
	box->addItem(L"Irrlicht Software Renderer 1.0");
	box->setSelected(selected);*/

	// add button
	startButton = guienv->addButton(core::rect<int>(50, leftY + (displayMetrics.heightPixels/4), leftX/2, displayMetrics.heightPixels-10), 0, 2, L"Start Demo");

	// add checkbox
	const s32 d = 50;
	//110-> 185
	/*guienv->addCheckBox(fullscreen, core::rect<int>(20, 85 + d, 350, 185 + d),
		optTab, 3, L"Fullscreen");
	guienv->addCheckBox(music, core::rect<int>(400, 85 + d, 730, 185 + d),
		optTab, 4, L"Music & Sfx");
	guienv->addCheckBox(shadows, core::rect<int>(20, 285 + d, 350, 385 + d),
		optTab, 5, L"Realtime shadows");
	guienv->addCheckBox(additive, core::rect<int>(20, 135 + d, 230, 160 + d),
		optTab, 6, L"Old HW compatible blending");
	guienv->addCheckBox(vsync, core::rect<int>(20, 160 + d, 230, 185 + d),
		optTab, 7, L"Vertical synchronisation");
	guienv->addCheckBox(aa, core::rect<int>(135, 110 + d, 245, 135 + d),
		optTab, 8, L"Antialiasing");*/
	guienv->addCheckBox(isServer, core::rect<int>(600, 300 + d, 930 + 300, 400 + d),
		optTab, 9, L"Server");

	// RakNet: Add edit box
	//nameEditBox = guienv->addEditBox(L"Your name here", core::rect<int>(20, 185 + d, 230, 210 + d), true, optTab, 9);

#else
	// add images
	const s32 leftX = 260;

	// add tab control
	gui::IGUITabControl* tabctrl = guienv->addTabControl(core::rect<int>(leftX, 10, 512 - 10, 384 - 10),
		0, true, true);
	gui::IGUITab* optTab = tabctrl->addTab(L"Demo");
	gui::IGUITab* aboutTab = tabctrl->addTab(L"About");

	// add list box

	gui::IGUIListBox* box = guienv->addListBox(core::rect<int>(10, 10, 220, 120), optTab, 1);
	box->addItem(L"OpenGL 1.5");
	box->addItem(L"Direct3D 8.1");
	box->addItem(L"Direct3D 9.0c");
	box->addItem(L"Burning's Video 0.39");
	box->addItem(L"Irrlicht Software Renderer 1.0");
	box->setSelected(selected);

	// add button

	startButton = guienv->addButton(core::rect<int>(30, 295, 200, 324), optTab, 2, L"Start Demo");

	// add checkbox

	const s32 d = 50;
	vsync = true;

	guienv->addCheckBox(fullscreen, core::rect<int>(20, 85 + d, 130, 110 + d),
		optTab, 3, L"Fullscreen");
	guienv->addCheckBox(music, core::rect<int>(135, 85 + d, 245, 110 + d),
		optTab, 4, L"Music & Sfx");
	guienv->addCheckBox(shadows, core::rect<int>(20, 110 + d, 135, 135 + d),
		optTab, 5, L"Realtime shadows");
	guienv->addCheckBox(additive, core::rect<int>(20, 135 + d, 230, 160 + d),
		optTab, 6, L"HW compat blend");
	guienv->addCheckBox(vsync, core::rect<int>(20, 160 + d, 230, 185 + d),
		optTab, 7, L"Vsync");
	guienv->addCheckBox(aa, core::rect<int>(135, 110 + d, 245, 135 + d),
		optTab, 8, L"Antialiasing");
	guienv->addCheckBox(isServer, core::rect<int>(135, 160 + d, 245, 185 + d),
		optTab, 9, L"Server");
	guienv->addCheckBox(isBot, core::rect<int>(135, 135 + d, 245, 160 + d),
		optTab, 10, L"Bot");
	guienv->addCheckBox(isLocalServer, core::rect<int>(135, 185 + d, 245, 210 + d),
		optTab, 13, L"LocalServer");

	wchar_t buffer[20];
	swprintf(buffer, 20, L"%d", outLogCount);
	// RakNet: Add edit box
	logCountEditBox = guienv->addEditBox(buffer, core::rect<int>(20, 185 + d, 130, 210 + d), true, optTab, 11);
	nameEditBox = guienv->addEditBox(L"Your name here", core::rect<int>(20, 185 + d + 30, 230, 210 + d + 30), true, optTab, 12);

#endif //__ANDROID__
	
	// add about text
	wchar_t* text2 = L"This is the tech demo of the Irrlicht engine. To start, "\
		L"select a video driver which works best with your hardware and press 'Start Demo'.\n"\
		L"What you currently see is displayed using the Burning Software Renderer (Thomas Alten).\n"\
		L"The Irrlicht Engine was written by me, Nikolaus Gebhardt. The models, "\
		L"maps and textures were placed at my disposal by B.Collins, M.Cook and J.Marton. The music was created by "\
		L"M.Rohde and is played back by irrKlang.\n"\
		L"For more informations, please visit the homepage of the Irrlicht engine:\nhttp://irrlicht.sourceforge.net\n"\
		L"\n*** MULTIPLAYER UPDATE ***\n"\
		L"Peer to peer multiplayer added in two days using RakNet.\n"\
		L"For a description of the networking design, see included readme.txt .\n";

	guienv->addStaticText(text2, core::rect<int>(10, 10, 230, 320),
		true, true, aboutTab);

	// add md2 model

	scene::IAnimatedMesh* mesh = smgr->getMesh(mediaPath + "faerie.md2");
	scene::IAnimatedMeshSceneNode* modelNode = smgr->addAnimatedMeshSceneNode(mesh);
	if (modelNode)
	{
		modelNode->setPosition( core::vector3df(0.f, 0.f, -5.f) );
		modelNode->setMaterialTexture(0, driver->getTexture(mediaPath + "faerie2.bmp"));
		modelNode->setMaterialFlag(video::EMF_LIGHTING, true);
		modelNode->getMaterial(0).Shininess = 28.f;
		modelNode->getMaterial(0).NormalizeNormals = true;
		modelNode->setMD2Animation(scene::EMAT_STAND);
	}

	// set ambient light (no sun light in the catacombs)
	smgr->setAmbientLight( video::SColorf(0.f, 0.f, 0.f) );

	scene::ISceneNodeAnimator* anim;
	scene::ISceneNode* bill;

	// add light 1 (sunset orange)
	scene::ILightSceneNode* light1 =
		smgr->addLightSceneNode(0, core::vector3df(10.f,10.f,0),
		video::SColorf(0.86f, 0.38f, 0.05f), 200.0f);

	// add fly circle animator to light 1
	anim = smgr->createFlyCircleAnimator(core::vector3df(0,0,0),30.0f, -0.004f, core::vector3df(0.41f, 0.4f, 0.0f));
	light1->addAnimator(anim);
	anim->drop();

	// let the lights follow the model...
	anim = new CSceneNodeAnimatorFollowBoundingBox(modelNode, core::vector3df(0,16,0), 4000, 0);
	//light1->addAnimator(anim);
	anim->drop();

	// attach billboard to the light
	bill = smgr->addBillboardSceneNode(light1, core::dimension2d<f32>(10, 10));
	bill->setMaterialFlag(video::EMF_LIGHTING, false);
	bill->setMaterialType(video::EMT_TRANSPARENT_ADD_COLOR);
	bill->setMaterialTexture(0, driver->getTexture(mediaPath + "particlered.bmp"));

#if 1
	// add light 2 (nearly red)
	scene::ILightSceneNode* light2 =
		smgr->addLightSceneNode(0, core::vector3df(0,1,0),
		video::SColorf(0.9f, 1.0f, 0.f, 0.0f), 200.0f);

	// add fly circle animator to light 1
	anim = smgr->createFlyCircleAnimator(core::vector3df(0,0,0),30.0f, 0.004f, core::vector3df(0.41f, 0.4f, 0.0f));
	light2->addAnimator(anim);
	anim->drop();

	// let the lights follow the model...
	anim = new CSceneNodeAnimatorFollowBoundingBox( modelNode, core::vector3df(0,-8,0), 2000, 0 );
	//light2->addAnimator(anim);
	anim->drop();


	// attach billboard to the light
	bill = smgr->addBillboardSceneNode(light2, core::dimension2d<f32>(10, 10));
	bill->setMaterialFlag(video::EMF_LIGHTING, false);
	bill->setMaterialType(video::EMT_TRANSPARENT_ADD_COLOR);
	bill->setMaterialTexture(0, driver->getTexture(mediaPath + "particlered.bmp"));

	// add light 3 (nearly blue)
	scene::ILightSceneNode* light3 =
		smgr->addLightSceneNode(0, core::vector3df(0,-1,0),
		video::SColorf(0.f, 0.0f, 0.9f, 0.0f), 40.0f);

	// add fly circle animator to light 2
	anim = smgr->createFlyCircleAnimator(core::vector3df(0,0,0),40.0f, 0.004f, core::vector3df(-0.41f, -0.4f, 0.0f));
	light3->addAnimator(anim);
	anim->drop();

	// let the lights follow the model...
	anim = new CSceneNodeAnimatorFollowBoundingBox(modelNode, core::vector3df(0,8,0), 8000, 0);
	//light3->addAnimator(anim);
	anim->drop();

	// attach billboard to the light
	bill = smgr->addBillboardSceneNode(light3, core::dimension2d<f32>(10, 10));
	if (bill)
	{
		bill->setMaterialFlag(video::EMF_LIGHTING, false);
		bill->setMaterialType(video::EMT_TRANSPARENT_ADD_COLOR);
		bill->setMaterialTexture(0, driver->getTexture(mediaPath + "portal1.bmp"));
	}
#endif

	// create a fixed camera
	smgr->addCameraSceneNode(0, core::vector3df(45,0,0), core::vector3df(0,0,10));

	// irrlicht logo and background
	// add irrlicht logo
	bool oldMipMapState = driver->getTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS);
	driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, false);

	guienv->addImage(driver->getTexture(mediaPath + "irrlichtlogo2.png"),
		core::position2d<s32>(5,5));

	video::ITexture* irrlichtBack = driver->getTexture(mediaPath + "demoback.jpg");

	driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, oldMipMapState);

	// query original skin color
	getOriginalSkinColor();

	// set transparency
	setTransparency();

	// draw all

	u32 loop = 0;	// loop is reset when the app is destroyed unlike runCounter
	static u32 runCounter = 0;	// static's seem to survive even an app-destroy message (not sure if that's guaranteed).
	
#ifdef __ANDROID__
	while (start == false)
#else
	while (MenuDevice->run())
#endif // __ANDROID__
	{
		if (MenuDevice->isWindowActive())
		{
			driver->beginScene(false, true, video::SColor(0,0,0,0));

#ifdef __ANDROID__
			if (irrlichtBack)
				driver->draw2DImage(irrlichtBack,
					core::position2d<int>(0, 0), core::rect<int>(0, 0, displayMetrics.widthPixels, displayMetrics.heightPixels));
#else
			if (irrlichtBack)
				driver->draw2DImage(irrlichtBack,
					core::position2d<int>(0, 0), core::rect<int>(0, 0, 512, 384));
#endif // __ANDROID__

			smgr->drawAll();
			guienv->drawAll();
			driver->endScene();
		}
#ifdef __ANDROID__
		MenuDevice->yield(); // probably nicer to the battery
		++runCounter;
		++loop;
#else
#endif //__ANDROID__

	}

#ifdef __ANDROID__
#else
#endif // __ANDROID__

	playerName = core::stringw(name.c_str());
	
	wchar_t* end = nullptr;
	errno = 0;
	long v = std::wcstol(logCnt.c_str(), &end, 10);
	bool ok = (end != logCnt.c_str()) && (errno != ERANGE);
	outLogCount = ok ? static_cast<int>(v) : outLogCount;

	outFullscreen = fullscreen;
	outMusic = music;
	outShadows = shadows;
	outAdditive = additive;
	outVSync = vsync;
	outAA = aa;
	outIsServer = isServer;
	outIsBot = isBot;
	outIsLocalServer = isLocalServer;

	switch(selected)
	{
	case 0:	outDriver = video::EDT_OPENGL; break;
	//case 1:	outDriver = video::EDT_DIRECT3D8; break;
	case 2:	outDriver = video::EDT_DIRECT3D9; break;
	case 3:	outDriver = video::EDT_BURNINGSVIDEO; break;
	case 4:	outDriver = video::EDT_SOFTWARE; break;
	}

#ifdef __ANDROID__
	outDriver = video::EDT_OGLES2;
#endif // __ANDROID__

	return start;
}


bool CMainMenu::OnEvent(const SEvent& event)
{
#ifdef __ANDROID__

	if (event.EventType == EET_TOUCH_INPUT_EVENT)
	{
		/*
			For now we fake mouse-events. Touch-events will be handled inside Irrlicht in the future, but until
			that is implemented you can use this workaround to get a GUI which works at least for simple elements like
			buttons. That workaround does ignore multi-touch events - if you need several buttons pressed at the same
			time you have to handle that yourself.
		*/
		SEvent fakeMouseEvent;
		fakeMouseEvent.EventType = EET_MOUSE_INPUT_EVENT;
		fakeMouseEvent.MouseInput.X = event.TouchInput.X;
		fakeMouseEvent.MouseInput.Y = event.TouchInput.Y;
		fakeMouseEvent.MouseInput.Shift = false;
		fakeMouseEvent.MouseInput.Control = false;
		fakeMouseEvent.MouseInput.ButtonStates = 0;
		fakeMouseEvent.MouseInput.Event = EMIE_COUNT;

		/*char strDisplay[100];
		sprintf(strDisplay, "fakeeee event type:(%d) / event TouchInput type:(%d) \n", fakeMouseEvent.EventType, event.TouchInput.Event);
		MenuDevice->getLogger()->log(strDisplay);*/

		switch (event.TouchInput.Event)
		{
		case ETIE_PRESSED_DOWN:
		{
			// We only work with the first for now.force opengl error
			if (TouchID == -1)
			{
				fakeMouseEvent.MouseInput.Event = EMIE_LMOUSE_PRESSED_DOWN;

				if (MenuDevice)
				{
					TouchID = event.TouchInput.ID;
				}
			}
			break;
		}
		case ETIE_MOVED:
			if (TouchID == event.TouchInput.ID)
			{
				fakeMouseEvent.MouseInput.Event = EMIE_MOUSE_MOVED;
				fakeMouseEvent.MouseInput.ButtonStates = EMBSM_LEFT;

			}
			break;
		case ETIE_LEFT_UP:
			if (TouchID == event.TouchInput.ID)
			{
				fakeMouseEvent.MouseInput.Event = EMIE_LMOUSE_LEFT_UP;
				TouchID = -1;
			}
			break;
		default:
			break;
		}

		if (fakeMouseEvent.MouseInput.Event != EMIE_COUNT && MenuDevice)
		{
			MenuDevice->postEventFromUser(fakeMouseEvent);
		}
	}
#else
	if (event.EventType == EET_KEY_INPUT_EVENT &&
		event.KeyInput.Key == KEY_F9 &&
		event.KeyInput.PressedDown == false)
	{
		video::IImage* image = MenuDevice->getVideoDriver()->createScreenShot();
		if (image)
		{
			MenuDevice->getVideoDriver()->writeImageToFile(image, "screenshot_main.jpg");
			image->drop();
		}
	}
	else
		if (event.EventType == irr::EET_MOUSE_INPUT_EVENT &&
			event.MouseInput.Event == EMIE_RMOUSE_LEFT_UP)
		{
			core::rect<s32> r(event.MouseInput.X, event.MouseInput.Y, 0, 0);
			gui::IGUIContextMenu* menu = MenuDevice->getGUIEnvironment()->addContextMenu(r, 0, 45);
			menu->addItem(L"transparent menus", 666, transparent == false);
			menu->addItem(L"solid menus", 666, transparent == true);
			menu->addSeparator();
			menu->addItem(L"Cancel");
		}
#endif //__ANDROID__
	else
	if (event.EventType == EET_GUI_EVENT)
	{
		s32 id = event.GUIEvent.Caller->getID();
		//char strDisplay[100];
		//sprintf(strDisplay, "gui id:(%d)\n", id);
		//MenuDevice->getLogger()->log(strDisplay);
		
		switch(id)
		{
		case 45: // context menu
			if (event.GUIEvent.EventType == gui::EGET_MENU_ITEM_SELECTED)
			{
				s32 s = ((gui::IGUIContextMenu*)event.GUIEvent.Caller)->getSelectedItem();
				if (s == 0 || s == 1)
				{
					transparent = !transparent;
					setTransparency();
				}
			}
			break;
		case 1:
			if (event.GUIEvent.EventType == gui::EGET_LISTBOX_CHANGED ||
				event.GUIEvent.EventType == gui::EGET_LISTBOX_SELECTED_AGAIN)
			{
				selected = ((gui::IGUIListBox*)event.GUIEvent.Caller)->getSelected();
				//startButton->setEnabled(selected != 4);
				startButton->setEnabled(true);
			}
			break;
		case 2:
			if (event.GUIEvent.EventType == gui::EGET_BUTTON_CLICKED )
			{
				/*char strDisplay[100];
				sprintf(strDisplay, "start button click!!!");
				MenuDevice->getLogger()->log(strDisplay);*/
				
				if (nameEditBox) { name = nameEditBox->getText(); }

				if (logCountEditBox) { logCnt = logCountEditBox->getText(); }

				MenuDevice->closeDevice();
				MenuDevice->drop();
				start = true;
			}
		case 3:
			if (event.GUIEvent.EventType == gui::EGET_CHECKBOX_CHANGED )
				fullscreen = ((gui::IGUICheckBox*)event.GUIEvent.Caller)->isChecked();
			break;
		case 4:
			if (event.GUIEvent.EventType == gui::EGET_CHECKBOX_CHANGED )
				music = ((gui::IGUICheckBox*)event.GUIEvent.Caller)->isChecked();
			break;
		case 5:
			if (event.GUIEvent.EventType == gui::EGET_CHECKBOX_CHANGED )
				shadows = ((gui::IGUICheckBox*)event.GUIEvent.Caller)->isChecked();
			break;
		case 6:
			if (event.GUIEvent.EventType == gui::EGET_CHECKBOX_CHANGED )
				additive = ((gui::IGUICheckBox*)event.GUIEvent.Caller)->isChecked();
			break;
		case 7:
			if (event.GUIEvent.EventType == gui::EGET_CHECKBOX_CHANGED )
				vsync = ((gui::IGUICheckBox*)event.GUIEvent.Caller)->isChecked();
			break;
		case 8:
			if (event.GUIEvent.EventType == gui::EGET_CHECKBOX_CHANGED )
				aa = ((gui::IGUICheckBox*)event.GUIEvent.Caller)->isChecked();
			break;
		case 9:
			if (event.GUIEvent.EventType == gui::EGET_CHECKBOX_CHANGED)
				isServer = ((gui::IGUICheckBox*)event.GUIEvent.Caller)->isChecked();
			break;
		case 10:
			if (event.GUIEvent.EventType == gui::EGET_CHECKBOX_CHANGED)
				isBot = ((gui::IGUICheckBox*)event.GUIEvent.Caller)->isChecked();
			break;
		case 13:
			if (event.GUIEvent.EventType == gui::EGET_CHECKBOX_CHANGED)
				isLocalServer = ((gui::IGUICheckBox*)event.GUIEvent.Caller)->isChecked();
			break;
		}
	}

	return false;
}


void CMainMenu::getOriginalSkinColor()
{
	irr::gui::IGUISkin * skin = MenuDevice->getGUIEnvironment()->getSkin();
	for (s32 i=0; i<gui::EGDC_COUNT ; ++i)
	{
		SkinColor[i] = skin->getColor( (gui::EGUI_DEFAULT_COLOR)i );
	}

}


void CMainMenu::setTransparency()
{
	irr::gui::IGUISkin * skin = MenuDevice->getGUIEnvironment()->getSkin();

	for (u32 i=0; i<gui::EGDC_COUNT ; ++i)
	{
		video::SColor col = SkinColor[i];

		if (false == transparent)
			col.setAlpha(255);

		skin->setColor((gui::EGUI_DEFAULT_COLOR)i, col);
	}
}

