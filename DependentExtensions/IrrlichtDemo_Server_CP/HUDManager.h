#pragma once
#include <irrlicht.h>
#include <vector>
#include "DS_Multilist.h"
#include "RakPeer.h"

using namespace irr;
using namespace RakNet;
using namespace gui;

struct SParticleImpact
{
    u32 when;
    core::vector3df pos;
    core::vector3df outVector;
};

// 메시지 관리용
struct MessageInfo {
    core::stringw content;
    u32 timeOut;
};

struct KillLog {
    RakNet::RakString message;
    RakNet::TimeMS timeStamp;
};

class HUDManager
{
public:
    static HUDManager* Instance();
    static void DestroyInstance();
    void Initialize();
    void Activate(IGUIEnvironment* guienv, const core::dimension2d<u32>& screenSize);
    void Update();
    //void UpdateStats(int fps, const char* pingStr, const char* packetLossStr);
    //void UpdateScore(int redScore, int blueScore, bool isServer);
    void PushMessage(const core::stringw& message);
    void SetHolderPosText(const core::vector3df& pos);
    void DrawCrosshair(video::IVideoDriver* driver, const core::dimension2d<u32>& screenSize);
    void SetPlayerNameText();
    const char* GetCurrentMessage(void);
    RakNet::RakString GetCurrentKillLogMessage(void);
private:
    HUDManager(); // private 생성자
    ~HUDManager();
    static HUDManager* instance;
    
    IGUIEnvironment* guienv;
    IGUIFont* font;
    gui::IGUIStaticText* statusText;
    gui::IGUIStaticText* killLogText;
    gui::IGUIStaticText* myNameText;
    gui::IGUIStaticText* holderPosText;
    gui::IGUIInOutFader* inOutFader;
    video::SColor backColor;
    
    scene::IQ3LevelMesh* quakeLevelMesh;
    scene::ISceneNode* quakeLevelNode;
    scene::ISceneNode* skyboxNode;
    scene::IAnimatedMeshSceneNode* model1;
    scene::IAnimatedMeshSceneNode* model2;
    scene::IParticleSystemSceneNode* campFire;
    
    IGUIListBox* messageBox; // EditBox 대신 ListBox가 로그 출력에 더 적합할 수 있음 (기존 EditBox 유지도 가능)
    video::ITexture* crosshairTex;
    core::array<SParticleImpact> Impacts;

    DataStructures::Multilist<ML_QUEUE, RakNet::RakString> outputMessages;
    DataStructures::Queue<KillLog> killLogMessages;
    TimeMS whenOutputMessageStarted;
    
    int currentScene;
    s32 sceneStartTime;
    s32 timeForThisScene;

    gui::IGUIElement* joy_stick;
    gui::IGUIElement* jump_button;
    gui::IGUIElement* fire_button;
    gui::IGUIElement* exit_button;
};

