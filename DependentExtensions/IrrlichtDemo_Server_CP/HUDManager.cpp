#include "HUDManager.h"

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

HUDManager::HUDManager() : guienv(nullptr), font(nullptr), statusText(nullptr) {}
HUDManager::~HUDManager() {
    // Irrlicht GUI 요소는 guienv->clear()나 drop()으로 정리되므로 
    // 여기서 특별히 delete할 것은 보통 없습니다.
}
void HUDManager::Initialize() {
	//statusText(0),  killLogText(0), myNameText(0), holderPosText(0),
}
void HUDManager::Activate() {
    this->guienv = env;

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

	crosshairTex = device->getVideoDriver()->getTexture(mediaPath + "crossHair_white.png");
}

void HUDManager::Update() {
	if (crosshairTex) DrawCrosshairHUD();

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
		device->setWindowCaption(tmp);
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

void HUDManager::DrawCrosshair() {
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
    video::SColor colors[4] = { color,color,color,color };

    device->getVideoDriver()->draw2DImage(crosshairTex, dstRect, srcRect, nullptr, colors, true);
}

void HUDManager::PushMessage(const core::stringw& message) {
    outputMessages.Push(rs, _FILE_AND_LINE_);
    if (whenOutputMessageStarted == 0)
    {
        whenOutputMessageStarted = RakNet::GetTimeMS();
    }
}

const char* CDemo::GetCurrentMessage(void)
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

void HUDManager::SetPlayerNameText() {
	if (myNameText == nullptr) return;
	RakNet::RakString msg("my Name : %s / K : %d / D : %d / S : %d", NetworkManager::Instance()->GetPlayerReplica()->playerName.C_String(), NetworkManager::Instance()->GetPlayerReplica()->killCnt, NetworkManager::Instance()->GetPlayerReplica()->deathCnt, NetworkManager::Instance()->GetPlayerReplica()->shootCnt);
	wchar_t wcharStr[128];
	mbstowcs(wcharStr, msg.C_String(), sizeof(wcharStr) / sizeof(wchar_t));
	myNameText->setText(wcharStr);
}