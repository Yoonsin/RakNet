#pragma once
#include <irrlicht.h>
using namespace irr;

struct CurTouchID {
	s32 move = -1;
	s32 fire = -1;
	s32 jump = -1;
	s32 exit = -1;
	s32 viewRotate = -1;
};

class InputController : public IEventReceiver
{
public:	
	static InputController* Instance();
	static void DestroyInstance();

	InputController();
	virtual ~InputController();
	virtual bool OnEvent(const SEvent& event);
	bool IsKeyDown(EKEY_CODE keyCode) const;
	void SetKeyDown(EKEY_CODE keyCode, bool isDown);
	bool IsMovementKeyDown(void) const;
	void EnableInput(bool enabled);
	void FlushMovementKeys();
	bool isKeyLock;
	bool wasKeyLock;
private:
	static InputController* instance;

	bool KeyIsDown[KEY_KEY_CODES_COUNT];

	//mobile
	s32 TouchID;
	bool isRotate;
	CurTouchID curTouchID;
};

