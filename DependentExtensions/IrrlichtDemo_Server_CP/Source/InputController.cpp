#include "InputController.h"
#include "HUDManager.h"
#include "SceneManager.h"
#include "CInGame.h"
#include "NetLogManager.h"
#include "Replicas.h"

using namespace RakNet;
using namespace irr;

InputController* InputController::instance = nullptr;
InputController* InputController::Instance() {
	if (instance == nullptr) instance = new InputController();
	return instance;
}

void InputController::DestroyInstance() {
	if (instance) {
		delete instance;
		instance = nullptr;
	}
}

InputController::InputController() : isKeyLock(false), wasKeyLock(false), isRotate(false), TouchID(-1) {
	if ( instance == nullptr ) instance = this;
	for (u32 i = 0; i < KEY_KEY_CODES_COUNT; ++i)
		KeyIsDown[i] = false;
}

InputController::~InputController() {
	// 
}

bool InputController::OnEvent(const SEvent& event) {

	if (event.EventType == EET_LOG_TEXT_EVENT) return true;

#ifdef __ANDROID__
	if (!CInGame::Instance()->GetDevice())
		return false;

	SEvent fakeKeyEvent;
	fakeKeyEvent.EventType = EET_KEY_INPUT_EVENT;
	fakeKeyEvent.KeyInput.Key = KEY_KEY_CODES_COUNT;

	scene::ISceneNodeAnimatorCameraFPS* fpsCamAnim = SceneManager::Instance()->fpsCamAnim;
	gui::IGUIElement* joy_stick = HUDManager::Instance()->joy_stick;
	gui::IGUIElement* jump_button = HUDManager::Instance()->jump_button;
	gui::IGUIElement* fire_button = HUDManager::Instance()->fire_button;
	gui::IGUIElement* exit_button = HUDManager::Instance()->exit_button;
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
			if (CInGame::Instance()->GetDevice())
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
						}
						else
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
				if (is_gui_viewport) {
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
					if (CInGame::Instance()->GetSceneManager()->getActiveCamera())
					{
						CInGame::Instance()->Respawn(CInGame::Instance()->initPos, CInGame::Instance()->initTarget);
						isKeyLock = false;
					}

					curTouchID.jump = -1;
				}
				else
					if (fire_button && fire_button->isPointInside(touchPoint) && id == curTouchID.fire && SceneManager::Instance()->currentScene == 1) {
						curTouchID.fire = -1;

						if (CInGame::Instance()->GetSceneManager()->getActiveCamera()->isVisible() == false)
						{
							if (CInGame::Instance()->GetDevice()->getCursorControl() != nullptr) CInGame::Instance()->GetDevice()->getCursorControl()->setVisible(false);
							CInGame::Instance()->GetSceneManager()->getActiveCamera()->setVisible(true);
						}
						else {
							CInGame::Instance()->shoot();
						}

					}
					else
						if (exit_button && exit_button->isPointInside(touchPoint) && id == curTouchID.exit && SceneManager::Instance()->currentScene == 1) {
							curTouchID.exit = -1;

							if (CInGame::Instance()->GetSceneManager()->getActiveCamera()->isVisible() == false)
							{
								if (CInGame::Instance()->GetDevice()->getCursorControl() != nullptr) CInGame::Instance()->GetDevice()->getCursorControl()->setVisible(false);
								CInGame::Instance()->GetSceneManager()->getActiveCamera()->setVisible(true);
							}
							else CInGame::Instance()->GetDevice()->closeDevice();
						}

			if (isRotate && id == curTouchID.viewRotate) {
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

	if (CInGame::Instance()->GetDevice()->getSceneManager()->getActiveCamera())
	{
		CInGame::Instance()->GetDevice()->getCursorControl();
		CInGame::Instance()->GetDevice()->getSceneManager()->getActiveCamera()->OnEvent(fakeKeyEvent);
	}

#else
    if (!(CInGame::Instance()->GetDevice()))
		return false;

	if (isKeyLock && !wasKeyLock) {
		FlushMovementKeys();   
		wasKeyLock = true;
	}
	else if (!isKeyLock && wasKeyLock) {
		wasKeyLock = false;    
	}

	if (isKeyLock) {
		if (event.EventType == EET_KEY_INPUT_EVENT) {
			const auto key = event.KeyInput.Key;
			const bool down = event.KeyInput.PressedDown;

			const bool allow =
				key == KEY_ESCAPE ||
				key == KEY_F9 ||
				key == KEY_KEY_R;

			if (!allow) {
				if (!down) KeyIsDown[key] = false;
				return true; 
			}
		}
		
		if (event.EventType == EET_MOUSE_INPUT_EVENT) {
			return true;    
		}
	}

	if (event.EventType == EET_MOUSE_INPUT_EVENT)
	{
		if (event.MouseInput.Event == EMIE_LMOUSE_PRESSED_DOWN)
			leftMouseDown = true;
		else if (event.MouseInput.Event == EMIE_LMOUSE_LEFT_UP)
			leftMouseDown = false;
		else if (event.MouseInput.Event == EMIE_RMOUSE_PRESSED_DOWN)
			rightMouseDown = true;
		else if (event.MouseInput.Event == EMIE_RMOUSE_LEFT_UP)
			rightMouseDown = false;
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
//		//else
			//CInGame::Instance()->GetDevice()->closeDevice();

		// RakNet: Escape to get the mouse back
		if (CInGame::Instance()->GetSceneManager()->getActiveCamera()->isVisible())
		{
			if (CInGame::Instance()->GetDevice()->getCursorControl() != nullptr) CInGame::Instance()->GetDevice()->getCursorControl()->setVisible(true);
			CInGame::Instance()->GetSceneManager()->getActiveCamera()->setVisible(false);
		}
		else
		{
			CInGame::Instance()->GetDevice()->closeDevice();
		}
	}
	else if (
			(event.EventType == EET_MOUSE_INPUT_EVENT &&
			event.MouseInput.Event == EMIE_LMOUSE_LEFT_UP) &&
			SceneManager::Instance()->currentScene == 1)
	{

		// RakNet: Click without focus to get focus back
		if (CInGame::Instance()->GetSceneManager()->getActiveCamera()->isVisible() == false)
		{
			if (CInGame::Instance()->GetDevice()->getCursorControl() != nullptr) CInGame::Instance()->GetDevice()->getCursorControl()->setVisible(false);
			CInGame::Instance()->GetSceneManager()->getActiveCamera()->setVisible(true);
		}
	}
	else if (event.EventType == EET_KEY_INPUT_EVENT &&
			event.KeyInput.Key == KEY_F9 &&
			event.KeyInput.PressedDown == false)
	{
		video::IImage* image = CInGame::Instance()->GetDevice()->getVideoDriver()->createScreenShot();
		if (image)
		{
			CInGame::Instance()->GetDevice()->getVideoDriver()->writeImageToFile(image, "screenshot.bmp");
			CInGame::Instance()->GetDevice()->getVideoDriver()->writeImageToFile(image, "screenshot.png");
			CInGame::Instance()->GetDevice()->getVideoDriver()->writeImageToFile(image, "screenshot.tga");
			CInGame::Instance()->GetDevice()->getVideoDriver()->writeImageToFile(image, "screenshot.ppm");
			CInGame::Instance()->GetDevice()->getVideoDriver()->writeImageToFile(image, "screenshot.jpg");
			CInGame::Instance()->GetDevice()->getVideoDriver()->writeImageToFile(image, "screenshot.pcx");
			image->drop();
		}
	}
	else if (event.EventType == EET_KEY_INPUT_EVENT &&
			event.KeyInput.Key == KEY_KEY_C &&
			event.KeyInput.PressedDown == false)
	{
		SceneManager::Instance()->PlayWeaponAnimation(WANT_COCK, true);
	}
	else if (event.EventType == EET_KEY_INPUT_EVENT &&
			 event.KeyInput.Key == KEY_KEY_R &&
			 event.KeyInput.PressedDown == false)
	{
		if (CInGame::Instance()->GetDevice()->getSceneManager() == nullptr) return true;
		if (auto* cam = CInGame::Instance()->GetDevice()->getSceneManager()->getActiveCamera()) {
			if (KeyIsDown[KEY_LCONTROL] || KeyIsDown[KEY_RCONTROL]) {
				SceneManager::Instance()->PlayWeaponAnimation(WANT_RELOAD, true);
				return true;
			}
		if (CInGame::Instance()->isBot) {
			CInGame::Instance()->SetResetBot();
			CInGame::Instance()->Respawn(NetworkManager::Instance()->GetPlayerBotReplica()->respawnPos, NetworkManager::Instance()->GetPlayerBotReplica()->respawnTarget);
			CInGame::Instance()->botMoveTime = RakNet::GetTimeMS() + CInGame::Instance()->BOT_MOVE_TIME;
		}
		else {
			CInGame::Instance()->Respawn(CInGame::Instance()->initPos, CInGame::Instance()->initTarget);
		}
		isKeyLock = false;
		FlushMovementKeys(); // 
		}
	}
	else  if (event.EventType == EET_KEY_INPUT_EVENT && event.KeyInput.Key == KEY_KEY_T && event.KeyInput.PressedDown == false)
	{
		if (NetworkManager::Instance()->IsServer()) {
			NetworkManager::Instance()->StartStressTest(1000); // 2000 loops
		}
	}
	else  if ( event.EventType == EET_KEY_INPUT_EVENT && event.KeyInput.Key == KEY_KEY_G && event.KeyInput.PressedDown == false )
	{
		auto smgr = SceneManager::Instance();
		smgr->cameraMode = ( smgr->cameraMode + 1 ) % smgr->cameraArr.size( );
		auto* activeCam = smgr->cameraArr[smgr->cameraMode];
		CInGame::Instance( )->GetSceneManager( )->setActiveCamera(activeCam);
		
		if (smgr->cameraMode != 0) {
			// Set 3rd person camera to look at player
			core::vector3df playerPos = NetworkManager::Instance()->GetPlayerReplica()->position + core::vector3df(0, CAMERA_HEIGHT, 0);
			activeCam->setTarget(playerPos);
		}
	}
	else if (event.EventType == EET_KEY_INPUT_EVENT && event.KeyInput.Key == KEY_KEY_0 && event.KeyInput.PressedDown == false)
	{
		SceneManager::Instance()->showOtherPerspective = !SceneManager::Instance()->showOtherPerspective;
		NetLogManager::Instance()->PrintDebug("Show Other Perspective: %s\n", SceneManager::Instance()->showOtherPerspective ? "ON" : "OFF");
	}
	else if ( event.EventType == EET_KEY_INPUT_EVENT && event.KeyInput.Key == KEY_KEY_B && event.KeyInput.PressedDown == false )
	{
		SceneManager::Instance( )->CreateObject(false, ObjectType::SuppliesBox);
		NetLogManager::Instance( )->PrintDebug("Create Object \n");
	}
	else if (event.EventType == EET_KEY_INPUT_EVENT && event.KeyInput.PressedDown == true)
	{
		// Blender-like transformation controls for 3rd person weapon model (attached to FIRESPOT)
		PlayerReplica* localPlayer = NetworkManager::Instance()->GetPlayerReplica();
		scene::IAnimatedMeshSceneNode* node = (localPlayer) ? localPlayer->weaponNode : nullptr;
		if (node)
		{
			core::vector3df pos = node->getPosition();
			core::vector3df rot = node->getRotation();
			bool changed = false;
			float step = 1.0f; // Scale down step for weapon fine-tuning
			float rotStep = 5.0f;

			if (KeyIsDown[KEY_LSHIFT]) { step *= 0.1f; rotStep *= 0.1f; }

			// Translation (Relative to FIRESPOT joint): I/K (Y), J/L (X), U/O (Z)
			if (event.KeyInput.Key == KEY_KEY_I) { pos.Y += step; changed = true; }
			if (event.KeyInput.Key == KEY_KEY_K) { pos.Y -= step; changed = true; }
			if (event.KeyInput.Key == KEY_KEY_J) { pos.X -= step; changed = true; }
			if (event.KeyInput.Key == KEY_KEY_L) { pos.X += step; changed = true; }
			if (event.KeyInput.Key == KEY_KEY_U) { pos.Z += step; changed = true; }
			if (event.KeyInput.Key == KEY_KEY_O) { pos.Z -= step; changed = true; }

			// Rotation: 8/2 (X), 4/6 (Y), 7/9 (Z)
			if ( event.KeyInput.Key == KEY_KEY_8 ) { rot.X += rotStep; changed = true; }
			if ( event.KeyInput.Key == KEY_KEY_2 ) { rot.X -= rotStep; changed = true; }
			if (event.KeyInput.Key == KEY_KEY_4) { rot.Y += rotStep; changed = true; }
			if (event.KeyInput.Key == KEY_KEY_6) { rot.Y -= rotStep; changed = true; }
			if ( event.KeyInput.Key == KEY_KEY_7 ) { rot.Z += rotStep; changed = true; }
			if ( event.KeyInput.Key == KEY_KEY_9 ) { rot.Z -= rotStep; changed = true; }

			if (changed)
			{
				node->setPosition(pos);
				node->setRotation(rot);
				NetLogManager::Instance()->PrintDebug("3rd Person Weapon Offset Pos: %.2f, %.2f, %.2f | Rot: %.2f, %.2f, %.2f\n", 
					pos.X, pos.Y, pos.Z, rot.X, rot.Y, rot.Z);
			}
		}
	}
	else if (CInGame::Instance()->GetDevice()->getSceneManager() != nullptr &&
			CInGame::Instance()->GetDevice()->getSceneManager()->getActiveCamera())
	{
		if (isKeyLock)
			return true; // 

		if (event.EventType == EET_MOUSE_INPUT_EVENT) {
			if (event.MouseInput.Event == EMIE_MOUSE_WHEEL && SceneManager::Instance()->cameraMode == 0)
			{
				SceneManager::Instance()->CycleWeapon(event.MouseInput.Wheel > 0 ? 1 : -1);
				return true;
			}
			CInGame::Instance()->GetDevice()->getSceneManager()->getActiveCamera()->OnEvent(event);
		}
		else if (event.EventType == EET_KEY_INPUT_EVENT) {
			auto* smgr = SceneManager::Instance();
			if (smgr->cameraMode != 0 && !smgr->cameraArr.empty()) {
				smgr->cameraArr[0]->OnEvent(event);
			}
			CInGame::Instance()->GetDevice()->getSceneManager()->getActiveCamera()->OnEvent(event);
		}
		return true;
	}
#endif //__ANDROID__
	return false;
}

bool InputController::IsKeyDown(EKEY_CODE keyCode) const { return KeyIsDown[keyCode]; }

void InputController::SetKeyDown(EKEY_CODE keyCode, bool isDown) { KeyIsDown[keyCode] = isDown; }

bool InputController::IsMovementKeyDown(void) const {
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

void InputController::EnableInput(bool enabled)
{
	scene::ICameraSceneNode* camera = CInGame::Instance()->GetSceneManager()->getActiveCamera();
	if (camera) camera->setInputReceiverEnabled(enabled);
}

void InputController::FlushMovementKeys()
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
		KeyIsDown[k] = false;      // 
		ev.KeyInput.Key = k;       // 
		if (auto* cam = CInGame::Instance()->GetDevice()->getSceneManager()->getActiveCamera())
			cam->OnEvent(ev);
	}
}
