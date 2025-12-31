#include "InputController.h"

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

InputController::InputController() : isKeyLock(false), wasKeyLock(false) {
	for (u32 i = 0; i < KEY_KEY_CODES_COUNT; ++i)
		KeyIsDown[i] = false;
}

InputController::~InputController() {
	// 안전한 포인터 삭제 로직
}

bool InputController::OnEvent(const SEvent& event) {
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
					if (GetSceneManager()->getActiveCamera())
					{
						Respawn(initPos, initTarget);
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

					}
					else
						if (exit_button && exit_button->isPointInside(touchPoint) && id == curTouchID.exit && currentScene == 1) {
							curTouchID.exit = -1;

							if (GetSceneManager()->getActiveCamera()->isVisible() == false)
							{
								if (device->getCursorControl() != nullptr) device->getCursorControl()->setVisible(false);
								GetSceneManager()->getActiveCamera()->setVisible(true);
							}
							else device->closeDevice();
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
							Respawn(NetworkManager::Instance()->GetPlayerBotReplica()->respawnPos, NetworkManager::Instance()->GetPlayerBotReplica()->respawnTarget);
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
	scene::ICameraSceneNode* camera = GetSceneManager()->getActiveCamera();
	if (camera) camera->setInputReceiverEnabled(enabled);
}

void InputController::InitMobileControls() {
	// set game UI

	core::dimension2d<u32> size = device->getVideoDriver()->getScreenSize();
	int offset = 50;
	int offset2 = 150;
	core::rect<int> jumpPos(size.Width - 150 - offset - offset2, size.Height - 300 - offset, size.Width - offset2, size.Height - 150);
	device->getGUIEnvironment()->addButton(jumpPos, 0, AppSkin::GUI_JUMP, L"RESET"); //디버그 용으로 점프 -> 리셋으로 수정

	core::rect<int> firePos(size.Width - 150 - offset - offset2, size.Height - 550 - offset, size.Width - offset2, size.Height - 400);
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
		if (auto* cam = device->getSceneManager()->getActiveCamera())
			cam->OnEvent(ev);
	}
}