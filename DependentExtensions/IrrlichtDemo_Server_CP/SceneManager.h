#pragma once
#include <irrlicht.h>
#include <vector>
#include "NetworkManager.h"
#include "GetTime.h"
using namespace irr;

#ifdef __ANDROID__
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

class SceneManager
{
public:
    static SceneManager* Instance();
    static void DestroyInstance();

    void Initialize(bool fullscreen, bool music, bool shadows, bool additive, bool vsync, bool aa, video::E_DRIVER_TYPE d);
    void Activate();
    void LoadMap(const char* mapName);
    void Update(); // 파티클 업데이트 등
    void Cleanup();
    void createLoadingScreen();
    void loadSceneData();
    void switchToNextScene();
    void createParticleImpacts();
    const core::aabbox3df& GetSyndeyBoundingBox(void) const;
    void CalculateSyndeyBoundingBox(void);
    RakNet::TimeMS shootFromOrigin(core::vector3df camPosition, core::vector3df camAt, GamePlatform platform); // 레이캐스팅 (사격 판정용)
    RakNet::TimeMS shootFromOrigin(core::vector3df camPosition, core::vector3df camAt, core::vector3df start, core::vector3df end, bool& wallHit, core::vector3df& wallHitPoint, GamePlatform platform);

    scene::ISceneNodeAnimatorCameraFPS* fpsCamAnim = nullptr;
    scene::ISceneNodeAnimatorCollisionResponse* fpsCamResponse = nullptr;
    bool fullscreen;
    bool music;
    bool shadows;
    bool additive;
    bool vsync;
    bool aa;
    video::E_DRIVER_TYPE driverType;
    const char* baseDir;
    int currentScene;
private:
    SceneManager();
    ~SceneManager();
    static SceneManager* instance;
    scene::ISceneManager* smgr;

    // 맵 & 충돌 관련
    scene::IMetaTriangleSelector* metaSelector;
    scene::ITriangleSelector* mapSelector;
    scene::ITriangleSelector* playerSelector;
    scene::ISceneNode* quakeLevelNode;
    scene::IQ3LevelMesh* quakeLevelMesh;
    scene::ISceneNode* skyboxNode;
    scene::IAnimatedMeshSceneNode* model1;
    scene::IAnimatedMeshSceneNode* model2;
    scene::IParticleSystemSceneNode* campFire;
    gui::IGUIInOutFader* inOutFader;
    video::SColor backColor;

    // 파티클 관리 구조체 및 배열
    struct SParticleImpact {
        u32 when;
        core::vector3df pos;
        core::vector3df outVector;
    };
    core::array<SParticleImpact> Impacts;
    core::aabbox3df syndeyBoundingBox; // Bounding box of syndney.md2, extended by BALL_DIAMETER/2 for collision against shots
    IrrlichtDevice* device;
    video::IVideoDriver* driver;
    gui::IGUIEnvironment* guienv;
    RakNet::TimeMS sceneStartTime;
    RakNet::TimeMS timeForThisScene;
	RakNet::TimeMS now;
};

