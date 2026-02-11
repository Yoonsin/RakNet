#pragma once
#include <irrlicht.h>
#include <vector>
#include <string>
#include <utility>
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

enum GunType {
	AT9mm = 0, //Pistol
	G3 = 1, //Battle Rifle
    Ingram = 2, //Submachine Gun
    LMG23 = 3, //Light Machine Gun
    M79 = 4, //Grenade Launcher
	MSG90 = 5, //Sniper Rifle
	Shorty = 6, //Sawed-off Shotgun
	Sporting12 = 7, //Shotgun
	Grenade = 8, //Grenade
	Panzerfaust = 9, //Recoilless Rifle (Bazooka)
	Knife = 10 //Knife
    //TODO : Add Land Mine, Shield
};

enum GunTraceType {
	GTT_HITSCAN,
	GTT_PROJECTILE,
	GTT_KNIFE
};

inline GunTraceType GetTraceType(GunType type) {
	if (type == M79 || type == Grenade || type == Panzerfaust) return GTT_PROJECTILE;
	if (type == Knife) return GTT_KNIFE;
	return GTT_HITSCAN;
}

enum WeaponAnimType {
    WANT_IDLE,
    WANT_MOVE,
    WANT_FIRE,
    WANT_SELECT,
    WANT_RELOAD,
    WANT_COCK,
    WANT_PUTAWAY,
    WANT_COUNT
};

struct AnimRange {
    s32 start;
    s32 end;
    bool loop;
    AnimRange() : start(0), end(0), loop(false) {}
    AnimRange(s32 s, s32 e, bool l = false) : start(s), end(e), loop(l) {}
};

struct ModelInfo {
    std::string meshFile;
	core::array<std::string> textureFiles;
	scene::IAnimatedMeshSceneNode* Node = nullptr;
    GunType type;
    AnimRange animations[WANT_COUNT];

    // 머즐 플래시 설정
    bool useFlash = true;
    u32 flashDuration = 50;
    u32 flashRepeatCount = 1;
    u32 flashDelay = 0;
};

enum ObjectType {
    Obstacle = 0,
    SuppliesBox = 1,
    Turret = 2,
    Wall = 3,
    HealPack = 4,
    Car = 5,
};

const s32 OBJECT_ID_OFFSET = 10000;

class SceneManager
{
public:
    static SceneManager* Instance();
    static void DestroyInstance();

    void Initialize(bool fullscreen, bool music, bool shadows, bool additive, bool vsync, bool aa, video::E_DRIVER_TYPE d);
    void Activate();
    void Update(); // 파티클 업데이트 등
    void Cleanup();
    void CreateLoadingScreen();
    void LoadCharacterData( );
    void LoadSceneData();
    void LoadQuakeScene( );
    void LoadPlaneScene( );
    void CreateObject(bool hasServerAuthority, ObjectType type ); //hasServerAuthority
    void CreateCamera( );
    void SwitchToNextScene();
    void CreateParticleImpacts();
    const core::aabbox3df& GetPlayerBoundingBox(void) const;
    void CalculatePlayerBoundingBox(void);
    static AnimRange GetSASAnim(GunType gun, WeaponAnimType anim, bool isMoving, bool isDead = false, int direction = 0);
    RakNet::TimeMS shootFromOrigin(core::vector3df camPosition, core::vector3df camAt, GamePlatform platform); // 레이캐스팅 (사격 판정용)
    RakNet::TimeMS shootFromOrigin(core::vector3df camPosition, core::vector3df camAt, core::vector3df start, core::vector3df end, core::vector3df visualStart, bool& wallHit, core::vector3df& wallHitPoint, GamePlatform platform);

    scene::IAnimatedMeshSceneNode* GetModelNode(u32 index, bool isFirstPerson) {
        if (index < CharModelArr.size()) {
            return isFirstPerson ? CharModelArr[index].first.Node : CharModelArr[index].second.Node;
        }
        return nullptr;
    }

    GunType GetCurrentWeaponType() {
        if (currentWeaponIndex < CharModelArr.size()) {
            return CharModelArr[currentWeaponIndex].first.type;
        }
        return AT9mm;
    }

    void PlayWeaponAnimation(WeaponAnimType type, bool force = false);
    bool IsHighPriorityAnimPlaying();

    scene::ISceneNodeAnimatorCameraFPS* fpsCamAnim = nullptr;
    scene::ISceneNodeAnimatorCollisionResponse* fpsCamResponse = nullptr;
    core::array<scene::ICameraSceneNode *> cameraArr;
    bool fullscreen;
    bool music;
    bool shadows;
    bool additive;
    bool vsync;
    bool aa;
    video::E_DRIVER_TYPE driverType;
    const char* baseDir;
    int currentScene;
	int cameraMode; // 0: 1st person, 1: 3rd person
    u32 currentWeaponIndex;
    WeaponAnimType currentAnimType;
    bool isAiming;
    bool showOtherPerspective = false;
    float currentFOV;
    float targetFOV;
    core::array<std::pair<ModelInfo, ModelInfo>> CharModelArr;
    void CycleWeapon(int delta);
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
    core::aabbox3df playerBoundingBox; // Bounding box of sas.b3d, extended by BALL_DIAMETER/2 for collision against shots
    IrrlichtDevice* device;
    video::IVideoDriver* driver;
    gui::IGUIEnvironment* guienv;
    RakNet::TimeMS sceneStartTime;
    RakNet::TimeMS timeForThisScene;
	RakNet::TimeMS now;
};

