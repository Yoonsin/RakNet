#include "HUDManager.h"
#include "InputController.h"
#include "SceneManager.h"
#include "CInGame.h"
#include "GetTime.h"

using namespace RakNet;
using namespace irr;

HUDManager* HUDManager::instance = nullptr;

HUDManager* HUDManager::Instance() {
    if (instance == nullptr) instance = new HUDManager();
    return instance;
}

void HUDManager::DestroyInstance() {
    if (instance) {
        delete instance;
        instance = nullptr;
    }
}

HUDManager::HUDManager() : driver(nullptr), guienv(nullptr), font(nullptr), isVisible(true) {}
HUDManager::~HUDManager() {
}
void HUDManager::Initialize(bool isHUDVisible) {
	joy_stick = jump_button = fire_button = exit_button = statusText = killLogText = myNameText = holderPosText =  nullptr;
	isVisible = isHUDVisible;
}

void HUDManager::Activate() {
	guienv = CInGame::Instance()->GetDevice()->getGUIEnvironment();
	driver = CInGame::Instance()->GetDevice()->getVideoDriver();

	core::rect<int> myNameRect;
	core::rect<int> KillLogRect;
	core::rect<int> holderPosRect;
	core::dimension2d<u32> size = driver->getScreenSize();
	const int lwidth = driver->getScreenSize().Width - 20; const int lheight = 16;
#ifdef __ANDROID__
	myNameRect = core::rect<int>(10, 50, 1000, 100);
	holderPosRect = core::rect<int>(10, 110, 1000, 160);
	KillLogRect = core::rect<int>(lwidth - 550, 50, lwidth - 50, 700);
	InitMobileHUD();
#else
	myNameRect = core::rect<int>(10, 0, 250, 30);
	holderPosRect = core::rect<int>(10, 40, 250, 70);
	KillLogRect = core::rect<int>(lwidth - 100, 0, lwidth, 150);
#endif // __ANDROID__
	core::rect<int> pos(10, size.Height - lheight - 80, 10 + lwidth, size.Height - 80);
	statusText = guienv->addStaticText(L"Start", pos, true);
	statusText->setOverrideColor(video::SColor(255, 205, 200, 200));

	myNameText = guienv->addStaticText(L"My Name : ", myNameRect);
	myNameText->setOverrideColor(video::SColor(255, 255, 255, 255));
	myNameText->setBackgroundColor(video::SColor(255, 0, 0, 0));

	wchar_t* killText = L"";
	killLogText = guienv->addStaticText(killText, KillLogRect);
	killLogText->setOverrideColor(video::SColor(255, 255, 255, 255));
	killLogText->setBackgroundColor(video::SColor(255, 0, 0, 0));

	holderPosText = guienv->addStaticText(L"Holder Position : ", holderPosRect);
	holderPosText->setOverrideColor(video::SColor(255, 255, 255, 255));
	holderPosText->setBackgroundColor(video::SColor(255, 0, 0, 0));

	crosshairTex = driver->getTexture(CInGame::Instance()->mediaPath + "HUD/crossHair_white.png");
	scopeTex = driver->getTexture(CInGame::Instance()->mediaPath + "HUD/MSG90_scope.png");

	const char* gunNames[] = { "AT9mm", "G3", "Ingram", "LMG23", "M79", "MSG90", "Shorty", "Sporting12", "Grenade", "Panzerfaust", "Knife" };
	for (int i = 0; i < 11; ++i) {
		core::stringc path = CInGame::Instance()->mediaPath + "HUD/" + gunNames[i] + "_crosshair.png";
		crosshairTextures[i] = driver->getTexture(path);
	}

	HUDManager::Instance( )->SetVisible(isVisible);
}

void HUDManager::Update() {
	// Determine which crosshair to draw
	video::ITexture* activeCrosshair = crosshairTex;
	GunType currentType = SceneManager::Instance()->GetCurrentWeaponType();
	bool aiming = (currentType == MSG90 && InputController::Instance()->IsRightMouseDown());
	
	if (aiming && scopeTex) {
		// Draw Full Screen Scope
		core::dimension2d<u32> size = driver->getScreenSize();
		driver->draw2DImage(scopeTex, core::rect<s32>(0, 0, size.Width, size.Height), 
			core::rect<s32>(0, 0, scopeTex->getOriginalSize().Width, scopeTex->getOriginalSize().Height), 
			nullptr, nullptr, true);
	}

	else {
		if (currentType < 11 && crosshairTextures[currentType]) {
			activeCrosshair = crosshairTextures[currentType];
		}
		if (activeCrosshair) DrawCrosshair(activeCrosshair);
	}

	if ( !isVisible ) return;
	static s32 lastfps = 0;
	s32 nowfps = driver->getFPS();

	wchar_t tmp[255];
	swprintf(tmp, 255, L"%ls fps:%d triangles:%0.3f mio",
		driver->getName(),
		driver->getFPS(),
		(f32)driver->getPrimitiveCountDrawn(1) * (1.f / 1000000.f)
	);
	if (nowfps != lastfps)
	{
		CInGame::Instance()->GetDevice()->setWindowCaption(tmp);
		lastfps = nowfps;
	}

	RakNet::RakString curMsg = GetCurrentMessage();
	if (curMsg.IsEmpty() == false) {
		wchar_t dest[500];
		memset(dest, 0, sizeof(dest));
		mbstowcs(dest, curMsg.C_String(), curMsg.GetLength());
		statusText->setText(dest);
	}
	else statusText->setText(0);


	RakNet::RakString KillMsg = GetCurrentKillLogMessage();
	if (KillMsg.IsEmpty() == false) {
		wchar_t dest[500];
		memset(dest, 0, sizeof(dest));
		mbstowcs(dest, KillMsg.C_String(), KillMsg.GetLength());
		killLogText->setText(dest);
	}
	else killLogText->setText(0);
}

void HUDManager::SetHolderPosText(const core::vector3df& pos) {
    if (holderPosText == nullptr) return;
    RakNet::RakString msg("Hold Pos : %.2f, %.2f, %.2f", pos.X, pos.Y, pos.Z);
    wchar_t wcharStr[128];
    mbstowcs(wcharStr, msg.C_String(), sizeof(wcharStr) / sizeof(wchar_t));
    holderPosText->setText(wcharStr);
}

void HUDManager::DrawCrosshair(video::ITexture* tex) {
    if (!tex) return;
    core::dimension2d<u32> size = driver->getScreenSize();
    const core::dimension2du orig = tex->getOriginalSize(); 
    core::rect<s32> srcRect(0, 0, (s32)orig.Width, (s32)orig.Height); 

    s32 minPixel;
    if (size.Width <= size.Height) minPixel = size.Width;
    else minPixel = size.Height;

    const s32 target = (s32)(minPixel * 0.10f);
    const s32 posX = (size.Width - target) / 2;
    const s32 posY = (size.Height - target) / 2;
    core::rect<s32> dstRect(posX, posY, posX + target, posY + target);

    video::SColor color(255, 255, 255, 255);
    video::SColor colors[4] = { color,color,color,color };

    driver->draw2DImage(tex, dstRect, srcRect, nullptr, colors, true);
}

void HUDManager::PushMessage(const RakString& message) {
    outputMessages.Push(message, _FILE_AND_LINE_);
    if (whenOutputMessageStarted == 0) whenOutputMessageStarted = RakNet::GetTimeMS();
}

const char* HUDManager::GetCurrentMessage(void)
{
	if (outputMessages.GetSize() == 0)
		return "";
	RakNet::TimeMS curTime = RakNet::GetTimeMS();
	if (curTime - whenOutputMessageStarted > 500)
	{
		outputMessages.Pop(_FILE_AND_LINE_);
		whenOutputMessageStarted = curTime;
	}

	if (outputMessages.GetSize() == 0)
	{
		whenOutputMessageStarted = 0;
		return "";
	}
	return outputMessages.Peek().C_String();
}

RakNet::RakString HUDManager::GetCurrentKillLogMessage(void)
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

		if (time + 5000 < RakNet::GetTimeMS()) 
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

void HUDManager::SetPlayerNameText() {
	if (myNameText == nullptr) return;
	RakNet::RakString msg("my Name : %s / K : %d / D : %d / S : %d", NetworkManager::Instance()->GetPlayerReplica()->playerName.C_String(), NetworkManager::Instance()->GetPlayerReplica()->killCnt, NetworkManager::Instance()->GetPlayerReplica()->deathCnt, NetworkManager::Instance()->GetPlayerReplica()->shootCnt);
	wchar_t wcharStr[128];
	mbstowcs(wcharStr, msg.C_String(), sizeof(wcharStr) / sizeof(wchar_t));
	myNameText->setText(wcharStr);
}

void HUDManager::SetVisible(bool visible) {
	this->isVisible = visible;
	
	if (statusText) statusText->setVisible(visible);
	if (killLogText) killLogText->setVisible(visible);
	if (myNameText) myNameText->setVisible(visible);
	if (holderPosText) holderPosText->setVisible(visible);
	if (joy_stick) joy_stick->setVisible(visible);

}

void HUDManager::InitMobileHUD() {
#ifdef __ANDROID__
	core::dimension2d<u32> size = CInGame::Instance()->GetDevice()->getVideoDriver()->getScreenSize();
	int offset = 50;
	int offset2 = 150;
	core::rect<int> jumpPos(size.Width - 150 - offset - offset2, size.Height - 300 - offset, size.Width - offset2, size.Height - 150);
	CInGame::Instance()->GetDevice()->getGUIEnvironment()->addButton(jumpPos, 0, AppSkin::GUI_JUMP, L"RESET"); 

	core::rect<int> firePos(size.Width - 150 - offset - offset2, size.Height - 550 - offset, size.Width - offset2, size.Height - 400);
	CInGame::Instance()->GetDevice()->getGUIEnvironment()->addButton(firePos, 0, AppSkin::GUI_FIRE, L"FIRE");

	core::rect<int> exitPos(
		size.Width - 200 - 2 * offset - 2 * offset2,  
		size.Height - 300 - offset,               
		size.Width - 50 - offset - 2 * offset2,    
		size.Height - 150                              
	);
	CInGame::Instance()->GetDevice()->getGUIEnvironment()->addButton(exitPos, 0, AppSkin::GUI_EXIT, L"EXIT");

	joy_stick = CInGame::Instance()->GetDevice()->getGUIEnvironment()->getRootGUIElement()->getElementFromId(AppSkin::REGULAR_AGGREGATION);
	jump_button = CInGame::Instance()->GetDevice()->getGUIEnvironment()->getRootGUIElement()->getElementFromId(AppSkin::GUI_JUMP);
	fire_button = CInGame::Instance()->GetDevice()->getGUIEnvironment()->getRootGUIElement()->getElementFromId(AppSkin::GUI_FIRE);
	exit_button = CInGame::Instance()->GetDevice()->getGUIEnvironment()->getRootGUIElement()->getElementFromId(AppSkin::GUI_EXIT);
#endif // __ANDROID__
}
