#include "InputController.h"
#include "HUDManager.h"
#include "SceneManager.h"
#include "CInGame.h"

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
	for (u32 i = 0; i < KEY_KEY_CODES_COUNT; ++i)
		KeyIsDown[i] = false;
}

InputController::~InputController() {
	// 안전한 포인터 삭제 로직
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
			// RakNet: Use space to jump, not shoot
	//		(event.EventType == EET_KEY_INPUT_EVENT &&
	//		event.KeyInput.Key == KEY_SPACE &&
	//		event.KeyInput.PressedDown == false) ||
			(event.EventType == EET_MOUSE_INPUT_EVENT &&
			event.MouseInput.Event == EMIE_LMOUSE_LEFT_UP) &&
			//currentScene == 3
			SceneManager::Instance()->currentScene == 1)
	{

		// RakNet: Click without focus to get focus back
		if (CInGame::Instance()->GetSceneManager()->getActiveCamera()->isVisible() == false)
		{
			if (CInGame::Instance()->GetDevice()->getCursorControl() != nullptr) CInGame::Instance()->GetDevice()->getCursorControl()->setVisible(false);
			CInGame::Instance()->GetSceneManager()->getActiveCamera()->setVisible(true);
		}
		else
		{
			// shoot
			CInGame::Instance()->shoot();
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
			 event.KeyInput.Key == KEY_KEY_R &&
			 event.KeyInput.PressedDown == false)
	{
		if (CInGame::Instance()->GetDevice()->getSceneManager() == nullptr) return true;
		if (auto* cam = CInGame::Instance()->GetDevice()->getSceneManager()->getActiveCamera()) {
		if (CInGame::Instance()->isBot) {
			CInGame::Instance()->SetResetBot();
			CInGame::Instance()->Respawn(NetworkManager::Instance()->GetPlayerBotReplica()->respawnPos, NetworkManager::Instance()->GetPlayerBotReplica()->respawnTarget);
			CInGame::Instance()->botMoveTime = RakNet::GetTimeMS() + CInGame::Instance()->BOT_MOVE_TIME;
		}
		else {
			CInGame::Instance()->Respawn(CInGame::Instance()->initPos, CInGame::Instance()->initTarget);
		}
		isKeyLock = false;
		FlushMovementKeys(); //리셋 시에도 모든 이동키 해제
		}
	}
	else  if (event.EventType == EET_KEY_INPUT_EVENT && event.KeyInput.Key == KEY_KEY_T && event.KeyInput.PressedDown == false)
	{
		if (NetworkManager::Instance()->IsServer()) {
			NetworkManager::Instance()->StartStressTest(10000); // 10초간 지속
		}
	}
	else if (CInGame::Instance()->GetDevice()->getSceneManager() != nullptr &&
			CInGame::Instance()->GetDevice()->getSceneManager()->getActiveCamera())
	{
		if (isKeyLock)
			return true; // 여기서 바로 빠져나가면 기존 방향으로 계속 이동하지 않음

		if (event.EventType == EET_MOUSE_INPUT_EVENT) {
			//CInGame::Instance()->GetDevice()->getSceneManager()->getActiveCamera()->OnEvent(event);
		}
		else if (event.EventType == EET_KEY_INPUT_EVENT) {
			CInGame::Instance()->GetDevice()->getSceneManager()->getActiveCamera()->OnEvent(event);
			//if(event.KeyInput.Key == KEY_KEY_A || event.KeyInput.Key == KEY_KEY_D) CInGame::Instance()->GetDevice()->getSceneManager()->getActiveCamera()->OnEvent(event);
			//DebugPrintf("Player position : %f, %f, %f / isKeyLock : %d / wasKeyLock : %d \n", GetSceneManager()->getActiveCamera()->getPosition().X, GetSceneManager()->getActiveCamera()->getPosition().Y, GetSceneManager()->getActiveCamera()->getPosition().Z, isKeyLock, wasKeyLock);
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
		KeyIsDown[k] = false;      // 내부 키 상태 해제
		ev.KeyInput.Key = k;       // 카메라에도 KeyUp 전달
		if (auto* cam = CInGame::Instance()->GetDevice()->getSceneManager()->getActiveCamera())
			cam->OnEvent(ev);
	}
}