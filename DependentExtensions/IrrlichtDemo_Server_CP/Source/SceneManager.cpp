#include "SceneManager.h"
#include "NetworkManager.h"
#include "CInGame.h"
#include "HUDManager.h"
#include "InputController.h"

using namespace RakNet;
using namespace irr;

SceneManager* SceneManager::instance = nullptr;
SceneManager* SceneManager::Instance() {
	if (instance == nullptr) instance = new SceneManager();
	return instance;
}
void SceneManager::DestroyInstance() {
	if (instance) {
		delete instance;
		instance = nullptr;
	}
}
SceneManager::SceneManager() {
}

SceneManager::~SceneManager() {
	Cleanup();
}

// ----------------------------------------------------------------------------
// Custom Node and Factory for handling 'unsupported' nodes from CopperCube
// ----------------------------------------------------------------------------

class CUnsupportedDataHolder : public scene::ISceneNode {
public:
	io::IAttributes* StoredAttributes;

	CUnsupportedDataHolder(scene::ISceneNode* parent, scene::ISceneManager* mgr, s32 id)
		: scene::ISceneNode(parent, mgr, id), StoredAttributes(0) {
	}

	virtual ~CUnsupportedDataHolder() {
		if (StoredAttributes)
			StoredAttributes->drop();
	}

	virtual void OnRegisterSceneNode() override {
		if (IsVisible)
			SceneManager->registerNodeForRendering(this);
		ISceneNode::OnRegisterSceneNode();
	}

	virtual void render() override {
		// Nothing to render
	}

	virtual const core::aabbox3d<f32>& getBoundingBox() const override {
		return Box;
	}

	virtual void deserializeAttributes(io::IAttributes* in, io::SAttributeReadWriteOptions* options = 0) override {
		ISceneNode::deserializeAttributes(in, options);

		if (StoredAttributes)
			StoredAttributes->drop();

		// Clone attributes
		video::IVideoDriver* driver = SceneManager->getVideoDriver();
		StoredAttributes = SceneManager->getFileSystem()->createEmptyAttributes(driver);

		if (in && StoredAttributes) {
			for (u32 i = 0; i < in->getAttributeCount(); ++i) {
				io::E_ATTRIBUTE_TYPE t = in->getAttributeType(i);
				const c8* name = in->getAttributeName(i);
				switch (t) {
				case io::EAT_INT: StoredAttributes->addInt(name, in->getAttributeAsInt(i)); break;
				case io::EAT_FLOAT: StoredAttributes->addFloat(name, in->getAttributeAsFloat(i)); break;
				case io::EAT_STRING: StoredAttributes->addString(name, in->getAttributeAsString(i).c_str()); break;
				case io::EAT_BOOL: StoredAttributes->addBool(name, in->getAttributeAsBool(i)); break;
				case io::EAT_VECTOR3D: StoredAttributes->addVector3d(name, in->getAttributeAsVector3d(i)); break;
				case io::EAT_COLOR: StoredAttributes->addColor(name, in->getAttributeAsColor(i)); break;
				case io::EAT_COLORF: StoredAttributes->addColor(name, in->getAttributeAsColor(i)); break; // Approximate
				case io::EAT_ENUM: StoredAttributes->addEnum(name, in->getAttributeAsInt(i), 0); break; // Simple int copy
				default: break; 
				}
			}
		}
	}

	virtual scene::ESCENE_NODE_TYPE getType() const override {
		return (scene::ESCENE_NODE_TYPE)MAKE_IRR_ID('u', 'n', 's', 'p');
	}

private:
	core::aabbox3d<f32> Box;
};

class CUnsupportedNodeFactory : public scene::ISceneNodeFactory {
public:
	CUnsupportedNodeFactory(scene::ISceneManager* mgr) : Manager(mgr) {}

	virtual scene::ISceneNode* addSceneNode(scene::ESCENE_NODE_TYPE type, scene::ISceneNode* parent = 0) override {
		if (type == (scene::ESCENE_NODE_TYPE)MAKE_IRR_ID('u', 'n', 's', 'p')) {
			return new CUnsupportedDataHolder(parent ? parent : Manager->getRootSceneNode(), Manager, -1);
		}
		return 0;
	}

	virtual scene::ISceneNode* addSceneNode(const c8* typeName, scene::ISceneNode* parent = 0) override {
		if (core::stringc(typeName) == "unsupported") {
			return new CUnsupportedDataHolder(parent ? parent : Manager->getRootSceneNode(), Manager, -1);
		}
		return 0;
	}

	virtual u32 getCreatableSceneNodeTypeCount() const override { return 1; }

	virtual const c8* getCreateableSceneNodeTypeName(u32 idx) const override { return "unsupported"; }

	virtual scene::ESCENE_NODE_TYPE getCreateableSceneNodeType(u32 idx) const override { return (scene::ESCENE_NODE_TYPE)MAKE_IRR_ID('u', 'n', 's', 'p'); }

	virtual const c8* getCreateableSceneNodeTypeName(scene::ESCENE_NODE_TYPE type) const override {
		if (type == (scene::ESCENE_NODE_TYPE)MAKE_IRR_ID('u', 'n', 's', 'p')) return "unsupported";
		return 0;
	}

private:
	scene::ISceneManager* Manager;
};

void ConvertUnsupportedNodes(scene::ISceneManager* smgr) {
	core::array<scene::ISceneNode*> nodes;
	smgr->getSceneNodesFromType((scene::ESCENE_NODE_TYPE)MAKE_IRR_ID('u', 'n', 's', 'p'), nodes);

	for (u32 i = 0; i < nodes.size(); ++i) {
		CUnsupportedDataHolder* holder = (CUnsupportedDataHolder*)nodes[i];
		if (!holder->StoredAttributes) continue;

		scene::ISceneNode* newNode = 0;
		io::IAttributes* attr = holder->StoredAttributes;

		// Heuristic to determine type
		if (attr->existsAttribute("Mesh")) {
			core::stringc meshFile = attr->getAttributeAsString("Mesh");
			scene::IAnimatedMesh* mesh = smgr->getMesh(meshFile);
			if (mesh) {
				// Create Mesh Node
				newNode = smgr->addAnimatedMeshSceneNode(mesh, holder->getParent());
			}
		}
		// Add more heuristics here (e.g. Terrain) if needed
		else if (attr->existsAttribute("HeightMap")) {
			// Example for terrain (requires more attributes usually)
			// newNode = smgr->addTerrainSceneNode(...);
		}

		if (newNode) {
			// Copy basic transforms
			newNode->setPosition(holder->getPosition());
			newNode->setRotation(holder->getRotation());
			newNode->setScale(holder->getScale());
			newNode->setName(holder->getName());
			newNode->setID(holder->getID());
			
			// Apply other attributes (Materials, Textures, etc.)
			newNode->deserializeAttributes(attr);

			// Move children
			const core::list<scene::ISceneNode*>& children = holder->getChildren();
			core::list<scene::ISceneNode*>::ConstIterator it = children.begin();
			while (it != children.end()) {
				scene::ISceneNode* child = *it;
				child->setParent(newNode); // Reparent
				it = children.begin(); // Restart iterator as list changed
			}
			
			// Update absolute position
			newNode->updateAbsolutePosition();
		}

		// Remove the placeholder
		holder->remove();
	}
}

// ----------------------------------------------------------------------------

void SceneManager::Cleanup() {
	// 1. Selector ���� (create�� ������ ��ü�� �ݵ�� drop �ؾ� ��)
	if (metaSelector) {
		metaSelector->drop();
		metaSelector = nullptr;
	}
	if (mapSelector) {
		mapSelector->drop();
		mapSelector = nullptr;
	}

	// 2. SceneNode ����
	if (quakeLevelNode) {
		quakeLevelNode->remove();
		quakeLevelNode = nullptr;
	}
	if (skyboxNode) {
		skyboxNode->remove();
		skyboxNode = nullptr;
	}
	if (campFire) {
		campFire->remove();
		campFire = nullptr;
	}
	if (model1) {
		model1->remove();
		model1 = nullptr;
	}
	if (model2) {
		model2->remove();
		model2 = nullptr;
	}

	// ��ƼŬ �迭 �ʱ�ȭ
	// Clear character models
	for (u32 i = 0; i < CharModelArr.size(); ++i) {
		if (CharModelArr[i].first.Node) CharModelArr[i].first.Node->remove();
		if (CharModelArr[i].second.Node) CharModelArr[i].second.Node->remove();
	}
	CharModelArr.clear();
	Impacts.clear();
	cameraArr.clear();
}
void SceneManager::Initialize(bool fullscreen, bool music, bool shadows, bool additive, bool vsync, bool aa, video::E_DRIVER_TYPE d) {
	this->fullscreen = fullscreen;
	this->music = music;
	this->shadows = shadows;
	this->additive = additive;
	this->vsync = vsync;
	this->aa = aa;
	this->driverType = d;
	quakeLevelMesh = nullptr;
	quakeLevelNode = nullptr;
	skyboxNode = nullptr;
	model1 = nullptr; 
	model2 = nullptr; 
	inOutFader = nullptr;
	campFire = nullptr; 
	metaSelector = nullptr; 
	mapSelector = nullptr; 
	sceneStartTime = 0;
	timeForThisScene = 0;
	backColor = 0; 
	currentScene = -2;
	cameraMode = 0;
	currentWeaponIndex = 0;
	currentAnimType = WANT_COUNT;
	isAiming = false;
	currentFOV = 1.25f;
	targetFOV = 1.25f;
}

void SceneManager::PlayWeaponAnimation(WeaponAnimType type, bool force)
{
	if (CharModelArr.empty() || currentWeaponIndex >= CharModelArr.size()) return;
	if (!force && currentAnimType == type) return;

	auto& model = CharModelArr[currentWeaponIndex].first;
	if (model.Node) {
		const AnimRange& range = model.animations[type];
		if (range.start == 0 && range.end == 0 && type != WANT_IDLE) {
			PlayWeaponAnimation(WANT_IDLE, force);
			return;
		}
		model.Node->setFrameLoop(range.start, range.end);
		model.Node->setLoopMode(range.loop);
		model.Node->setAnimationSpeed(30);
		currentAnimType = type;
	}
}

void SceneManager::CycleWeapon(int delta)
{
	if (CharModelArr.empty()) return;

	// Hide current
	if (CharModelArr[currentWeaponIndex].first.Node)
		CharModelArr[currentWeaponIndex].first.Node->setVisible(false);

	// Cycle index
	int next = (int)currentWeaponIndex + delta;
	if (next < 0) next = (int)CharModelArr.size() - 1;
	else if (next >= (int)CharModelArr.size()) next = 0;

	currentWeaponIndex = (u32)next;
	isAiming = false;

	// Show new (if in 1st person camera)
	if (cameraMode == 0) {
		scene::ICameraSceneNode* activeCam = smgr->getActiveCamera();
		auto& model = CharModelArr[currentWeaponIndex].first;
		if (model.Node && activeCam) {
			model.Node->setParent(activeCam);
			
			// Default values
			float scale = 4.0f;
			core::vector3df pos(0, 10, -30);
			core::vector3df rot(0, -180, 0);

			// Special cases for specific weapons
			if (model.type == LMG23) {
				pos = core::vector3df(2.0f, 3.0f, -48.0f);
			}

			if ( model.type == G3 ) {
				pos = core::vector3df(1.0f, 3.0f, -38.0f);
			}
			
			if ( model.type == Grenade ) {
				pos.X = -10.0f; 
			}

			if ( model.type == Knife ) {
				pos = core::vector3df(0.0f, 7.0f, -50.0f);
				rot.Y = -170.0f; rot.Z = -10.0f;
			}
			
			if (model.type == Grenade || model.type == Panzerfaust || model.type == Knife) {
				scale = 1.0f;
			}

			model.Node->setScale(core::vector3df(scale));
			model.Node->setPosition(pos);
			model.Node->setRotation(rot);
			model.Node->setVisible(true);

			PlayWeaponAnimation(WANT_SELECT, true);
		}
	}

	NetLogManager::Instance()->PrintDebug("Switched to weapon index: %d\n", currentWeaponIndex);
}

void SceneManager::Activate() {
	this->device = CInGame::Instance()->GetDevice();
	driver = device->getVideoDriver();
	smgr = device->getSceneManager();
	guienv = device->getGUIEnvironment();
	sceneStartTime = device->getTimer()->getTime();
	currentFOV = 1.25f;
	targetFOV = 1.25f;
}

bool SceneManager::IsHighPriorityAnimPlaying()
{
	if (CharModelArr.empty() || currentWeaponIndex >= CharModelArr.size()) return false;
	auto& model = CharModelArr[currentWeaponIndex].first;
	if (!model.Node) return false;

	// High priority: Select, Fire, Reload, Cock
	if (currentAnimType == WANT_SELECT || currentAnimType == WANT_FIRE || 
		currentAnimType == WANT_RELOAD || currentAnimType == WANT_COCK) 
	{
		const AnimRange& range = model.animations[currentAnimType];
		// If it's non-looping and hasn't finished yet
		if (!range.loop && model.Node->getFrameNr() < (f32)range.end - 0.5f) {
			return true;
		}
	}
	return false;
}

void SceneManager::Update() {
	now = device->getTimer()->getTime();
	if (now - sceneStartTime > timeForThisScene && timeForThisScene != -1)
		SwitchToNextScene();

	CreateParticleImpacts();

	// Update FOV and Aiming
	if (currentScene == 1 && cameraMode == 0) {
		GunType curType = GetCurrentWeaponType();
		isAiming = (curType == MSG90 && InputController::Instance()->IsRightMouseDown());

		targetFOV = isAiming ? 0.2f : 1.25f; // Zoom in if aiming

		// Smooth interpolation
		float lerpSpeed = 0.1f;
		currentFOV += (targetFOV - currentFOV) * lerpSpeed;

		scene::ICameraSceneNode* cam = smgr->getActiveCamera();
		if (cam) cam->setFOV(currentFOV);

		// Weapon model visibility: Hide while scoped for full-screen scope effect
		auto& model = CharModelArr[currentWeaponIndex].first;
		if (model.Node) {
			model.Node->setVisible(!isAiming);
		}
	}

	// Update Weapon Animations
	if (currentScene == 1 && cameraMode == 0) {
		bool finished = false;
		bool chainCock = false;
		auto& model = CharModelArr[currentWeaponIndex].first;
		if (model.Node && !isAiming) { // Only animate if not aiming (scoped)
			const AnimRange& range = model.animations[currentAnimType];
			if (!range.loop && currentAnimType != WANT_IDLE && currentAnimType != WANT_MOVE && currentAnimType != WANT_COUNT) {
				if (model.Node->getFrameNr() >= (f32)range.end - 0.5f) {
					finished = true;
					if (currentAnimType == WANT_RELOAD) chainCock = true;
				}
			}
		}

		if (chainCock) {
			PlayWeaponAnimation(WANT_COCK, true);
		}
		else if (IsHighPriorityAnimPlaying() && !isAiming) {
			// Stay in current high priority animation
		}
		else if (InputController::Instance()->IsMovementKeyDown() && !isAiming) {
			PlayWeaponAnimation(WANT_MOVE);
		}
		else {
			if (currentAnimType == WANT_MOVE || finished || currentAnimType == WANT_COUNT || isAiming) {
				PlayWeaponAnimation(WANT_IDLE);
			}
		}
	}

	driver->beginScene(timeForThisScene != -1, true, backColor);
	smgr->drawAll();
	guienv->drawAll();
	HUDManager::Instance()->Update();
	driver->endScene();

}

void SceneManager::CreateParticleImpacts()
{
	u32 now = device->getTimer()->getTime();
	scene::ISceneManager* sm = device->getSceneManager();

	for (s32 i = 0; i < (s32)Impacts.size(); ++i)
		if (now > Impacts[i].when)
		{
			// create smoke particle system
			scene::IParticleSystemSceneNode* pas = 0;

			pas = sm->addParticleSystemSceneNode(false, 0, -1, Impacts[i].pos);
			pas->setParticleSize(core::dimension2d<f32>(10.0f, 10.0f));

			scene::IParticleEmitter* em = pas->createBoxEmitter(
				core::aabbox3d<f32>(-5, -5, -5, 5, 5, 5),
				Impacts[i].outVector, 20, 40, video::SColor(0, 255, 255, 255), video::SColor(0, 255, 255, 255),
				1200, 1600, 20);

			pas->setEmitter(em);
			em->drop();

			scene::IParticleAffector* paf = campFire->createFadeOutParticleAffector();
			pas->addAffector(paf);
			paf->drop();

			pas->setMaterialFlag(video::EMF_LIGHTING, false);
			pas->setMaterialTexture(0, device->getVideoDriver()->getTexture(CInGame::Instance()->mediaPath + "smoke.bmp"));
#ifdef __ANDROID__
			pas->setMaterialType(video::EMT_TRANSPARENT_ADD_COLOR);
#else
			pas->setMaterialType(video::EMT_TRANSPARENT_VERTEX_ALPHA);
#endif //__ANDROID__

			scene::ISceneNodeAnimator* anim = sm->createDeleteAnimator(2000);
			pas->addAnimator(anim);
			anim->drop();

			// delete entry
			Impacts.erase(i);
			i--;
		}
}

void SceneManager::CalculateSyndeyBoundingBox(void)
{
	// Find the extents of the player character's model (for networking collision checks)
	scene::IAnimatedMesh* mesh = 0;
	scene::ISceneManager* sm = device->getSceneManager();
	mesh = sm->getMesh(CInGame::Instance()->mediaPath + "sydney.md2");
	irr::scene::IAnimatedMeshSceneNode* model;
	model = sm->addAnimatedMeshSceneNode(mesh, 0);
	model->setScale(core::vector3df(1, 1, 1));
	// Bounding box changed in Irrlicht 1.5.1
	core::aabbox3df modelBoundingBox = model->getMesh()->getBoundingBox();
	// core::aabbox3df modelBoundingBox = model->getBoundingBox();
	core::vector3df minEdgeExtended = modelBoundingBox.MinEdge;
	core::vector3df maxEdgeExtended = modelBoundingBox.MaxEdge;
	minEdgeExtended.X -= BALL_DIAMETER;
	minEdgeExtended.Y -= BALL_DIAMETER * 1.25;
	minEdgeExtended.Z -= BALL_DIAMETER * 0.2;
	maxEdgeExtended.X += BALL_DIAMETER;///2
	maxEdgeExtended.Y += BALL_DIAMETER * 1.25;
	maxEdgeExtended.Z += BALL_DIAMETER * 0.2;
	syndeyBoundingBox.MinEdge = minEdgeExtended;
	syndeyBoundingBox.MaxEdge = maxEdgeExtended;
	model->remove();
};
const core::aabbox3df& SceneManager::GetSyndeyBoundingBox(void) const { return syndeyBoundingBox; }

void SceneManager::CreateCamera( )
{
#ifdef __ANDROID__
	SKeyMap keyMap[11];
#else
	SKeyMap keyMap[9];
#endif // __ANDROID__
	keyMap[0].Action = EKA_MOVE_FORWARD; keyMap[0].KeyCode = KEY_UP;
	keyMap[1].Action = EKA_MOVE_FORWARD; keyMap[1].KeyCode = KEY_KEY_W;

	keyMap[2].Action = EKA_MOVE_BACKWARD; keyMap[2].KeyCode = KEY_DOWN;
	keyMap[3].Action = EKA_MOVE_BACKWARD; keyMap[3].KeyCode = KEY_KEY_S;

	keyMap[4].Action = EKA_STRAFE_LEFT; keyMap[4].KeyCode = KEY_LEFT;
	keyMap[5].Action = EKA_STRAFE_LEFT; keyMap[5].KeyCode = KEY_KEY_A;

	keyMap[6].Action = EKA_STRAFE_RIGHT; keyMap[6].KeyCode = KEY_RIGHT;
	keyMap[7].Action = EKA_STRAFE_RIGHT; keyMap[7].KeyCode = KEY_KEY_D;

	keyMap[8].Action = EKA_JUMP_UP; 	keyMap[8].KeyCode = KEY_SPACE;

	//rotate
#ifdef __ANDROID__
	keyMap[9].Action = EKA_ROTATE_LEFT; keyMap[9].KeyCode = KEY_KEY_0;
	keyMap[10].Action = EKA_ROTATE_RIGHT; keyMap[10].KeyCode = KEY_KEY_1;
	//camera = sm->addCameraSceneNodeFPS(0, 1.0f, .4f, -1, keyMap, 11, true, 250.f); //�⺻ �÷���
	camera = sm->addCameraSceneNodeFPS(0, 1.0f, .4f, -1, keyMap, 11, false, 5.f); //�⺻ �÷���
	CInGame::Instance( )->SetTransformCamera(camera, CInGame::Instance( )->gamePlatform);

	core::vector3df gravity = core::vector3df(0, /*-300.f*/quakeLevelMesh ? -10.f : 0.0f, 0);
	scene::ISceneNodeAnimatorCollisionResponse * collider =
		sm->createCollisionResponseAnimator(
			metaSelector, camera, core::vector3df(25, CAMERA_HEIGHT, 25), gravity, core::vector3df(0, 45, 0), 0.005f);

	camera->addAnimator(collider);
	collider->drop( );

	const scene::ISceneNodeAnimatorList & animators = camera->getAnimators( );
	scene::ISceneNodeAnimatorList::ConstIterator it = animators.begin( );
	while ( it != animators.end( ) )
	{
		if ( scene::ESNAT_COLLISION_RESPONSE == ( *it )->getType( ) ) {
			fpsCamResponse = static_cast< scene::ISceneNodeAnimatorCollisionResponse * >( *it );
		}
		else if ( scene::ESNAT_CAMERA_FPS == ( *it )->getType( ) ) {
			fpsCamAnim = static_cast< scene::ISceneNodeAnimatorCameraFPS * >( *it );
		}
		it++;
	}
#else
		//Last parameter is jump speed
		//Tweaked so you can get up ladders
		scene::ICameraSceneNode * camera = smgr->addCameraSceneNodeFPS(0, 100.0f, .4f, -1, keyMap, 9, false, 5.f);
		CInGame::Instance()->SetTransformCamera(camera, CInGame::Instance()->gamePlatform);

		core::vector3df gravity = core::vector3df(0, -10.f, 0);
		scene::ISceneNodeAnimatorCollisionResponse* collider =
			smgr->createCollisionResponseAnimator(
				metaSelector, camera, core::vector3df(25, CAMERA_HEIGHT, 25), gravity, core::vector3df(0, 45, 0), 0.005f);

		camera->addAnimator(collider);
		collider->drop();

		const scene::ISceneNodeAnimatorList& animators = camera->getAnimators();
		scene::ISceneNodeAnimatorList::ConstIterator it = animators.begin();
		while (it != animators.end())
		{
			if (scene::ESNAT_COLLISION_RESPONSE == (*it)->getType()) {
				fpsCamResponse = static_cast<scene::ISceneNodeAnimatorCollisionResponse*>(*it);
			}
			else if (scene::ESNAT_CAMERA_FPS == (*it)->getType()) {
				fpsCamAnim = static_cast<scene::ISceneNodeAnimatorCameraFPS*>(*it);
			}
			it++;
		}
#endif // __ANDROID__

		cameraArr.push_back(camera);
		if (currentWeaponIndex < CharModelArr.size()) {
			CharModelArr[currentWeaponIndex].first.Node->setParent(camera);
			CharModelArr[currentWeaponIndex].first.Node->setScale(core::vector3df(4));
			CharModelArr[currentWeaponIndex].first.Node->setPosition(core::vector3df(0, 10, -30));
			CharModelArr[currentWeaponIndex].first.Node->setRotation(core::vector3df(0, -180, 0));
			CharModelArr[currentWeaponIndex].first.Node->setVisible(true); // 1st person model visible
		}

		// Debug Draw Bounding Box
		//camera->setDebugDataVisible(scene::EDS_BBOX);
		//CharModelArr[AT9mm].first.Node->setDebugDataVisible(scene::EDS_BBOX);

		scene::ICameraSceneNode * mayaCam = smgr->addCameraSceneNodeMaya( );
		mayaCam->setPosition(core::vector3df(0, 0, 0));
		cameraArr.push_back(mayaCam);
		smgr->setActiveCamera(camera);
}

void SceneManager::LoadCharacterData()
{
	// 1. Define gun configurations
	struct GunConfig {
		GunType type;
		std::string mesh1st;
		core::array<std::string> tex1st;
		std::string mesh3rd;
		core::array<std::string> tex3rd;
		AnimRange animations[WANT_COUNT];
	};

	core::array<GunConfig> gunConfigs;

	auto setA = [](GunConfig& d, WeaponAnimType t, s32 s, s32 e, bool l = false) {
		d.animations[t] = AnimRange(s, e, l);
	};

	// AT9mm: gun - ammo - hand
	{
		GunConfig d; d.type = AT9mm; d.mesh1st = "HUD_AT9mm.b3d";
		d.tex1st.push_back("AT9mm_gun.png"); d.tex1st.push_back("AT9mm_ammo.png"); d.tex1st.push_back("hand.png");
		d.mesh3rd = "W_AT9mm.b3d"; d.tex3rd.push_back("AT9mm_gun.png");
		setA(d, WANT_SELECT, 0, 31);
		setA(d, WANT_IDLE, 32, 76, true);
		setA(d, WANT_MOVE, 77, 101, true);
		setA(d, WANT_FIRE, 102, 105);
		setA(d, WANT_RELOAD, 106, 106);
		setA(d, WANT_COCK, 107, 185);
		setA(d, WANT_PUTAWAY, 186, 205);
		gunConfigs.push_back(d);
	}

	// G3: hand - gun - ammo
	{
		GunConfig d; d.type = G3; d.mesh1st = "HUD_G3.b3d";
		d.tex1st.push_back("hand.png"); d.tex1st.push_back("G3_gun.png"); d.tex1st.push_back("G3_ammo.png");
		d.mesh3rd = "W_G3.b3d"; d.tex3rd.push_back("G3_gun.png");
		setA(d, WANT_SELECT, 0, 38);
		setA(d, WANT_IDLE, 39, 83, true);
		setA(d, WANT_MOVE, 84, 108, true);
		setA(d, WANT_FIRE, 109, 126);
		setA(d, WANT_RELOAD, 127, 127);
		setA(d, WANT_COCK, 128, 235);
		setA(d, WANT_PUTAWAY, 235, 258);
		gunConfigs.push_back(d);
	}

	// Ingram: gun - ammo - hand
	{
		GunConfig d; d.type = Ingram; d.mesh1st = "HUD_Ingram.b3d";
		d.tex1st.push_back("Ingram_gun.png"); d.tex1st.push_back("Ingram_ammo.png"); d.tex1st.push_back("hand.png");
		d.mesh3rd = "W_Ingram.b3d"; d.tex3rd.push_back("Ingram_gun.png");
		setA(d, WANT_SELECT, 0, 37);
		setA(d, WANT_IDLE, 38, 82, true);
		setA(d, WANT_MOVE, 83, 107, true);
		setA(d, WANT_FIRE, 108, 125);
		setA(d, WANT_RELOAD, 126, 165);
		setA(d, WANT_COCK, 166, 205);
		setA(d, WANT_PUTAWAY, 206, 225);
		gunConfigs.push_back(d);
	}

	// LMG23: hand - gun
	{
		GunConfig d; d.type = LMG23; d.mesh1st = "HUD_LMG23.b3d";
		d.tex1st.push_back("hand.png"); d.tex1st.push_back("LMG23_gun.png");
		d.mesh3rd = "W_LMG23.b3d"; d.tex3rd.push_back("LMG23_gun.png");
		setA(d, WANT_SELECT, 0, 38);
		setA(d, WANT_IDLE, 39, 83, true);
		setA(d, WANT_MOVE, 84, 108, true);
		setA(d, WANT_FIRE, 109, 126);
		setA(d, WANT_RELOAD, 127, 169);
		setA(d, WANT_COCK, 170, 204);
		setA(d, WANT_PUTAWAY, 205, 228);
		gunConfigs.push_back(d);
	}

	// M79: hand - gun - ammo
	{
		GunConfig d; d.type = M79; d.mesh1st = "HUD_M79.b3d";
		d.tex1st.push_back("hand.png"); d.tex1st.push_back("M79_gun.png"); d.tex1st.push_back("M79_ammo.png");
		d.mesh3rd = "W_M79.b3d"; d.tex3rd.push_back("M79_gun.png");
		setA(d, WANT_SELECT, 0, 22);
		setA(d, WANT_IDLE, 23, 67, true);
		setA(d, WANT_MOVE, 68, 92, true);
		setA(d, WANT_FIRE, 93, 101);
		setA(d, WANT_RELOAD, 102, 186);
		setA(d, WANT_PUTAWAY, 187, 209);
		gunConfigs.push_back(d);
	}

	// MSG90: hand - gun - ammo
	{
		GunConfig d; d.type = MSG90; d.mesh1st = "HUD_MSG90.b3d";
		d.tex1st.push_back("hand.png"); d.tex1st.push_back("MSG90_gun.png"); d.tex1st.push_back("MSG90_ammo.png");
		d.mesh3rd = "W_MSG90.b3d"; d.tex3rd.push_back("MSG90_gun.png");
		setA(d, WANT_SELECT, 0, 38);
		setA(d, WANT_IDLE, 39, 83, true);
		setA(d, WANT_MOVE, 84, 108, true);
		setA(d, WANT_FIRE, 110, 115);
		setA(d, WANT_RELOAD, 117, 117);
		setA(d, WANT_COCK, 118, 224);
		setA(d, WANT_PUTAWAY, 225, 248);
		gunConfigs.push_back(d);
	}

	// Shorty: hand - gun
	{
		GunConfig d; d.type = Shorty; d.mesh1st = "HUD_Shorty.b3d";
		d.tex1st.push_back("hand.png"); d.tex1st.push_back("Shorty_gun.png");
		d.mesh3rd = "W_Shorty.b3d"; d.tex3rd.push_back("Shorty_gun.png");
		setA(d, WANT_SELECT, 0, 25);
		setA(d, WANT_IDLE, 26, 71, true);
		setA(d, WANT_MOVE, 72, 96, true);
		setA(d, WANT_FIRE, 97, 134);
		setA(d, WANT_RELOAD, 135, 184);
		setA(d, WANT_COCK, 135, 266);
		setA(d, WANT_PUTAWAY, 267, 291);
		gunConfigs.push_back(d);
	}

	// Sporting12: gun - hand
	{
		GunConfig d; d.type = Sporting12; d.mesh1st = "HUD_Sporting12.b3d";
		d.tex1st.push_back("Sporting12_gun.png"); d.tex1st.push_back("hand.png");
		d.mesh3rd = "W_Sporting12.b3d"; d.tex3rd.push_back("Sporting12_gun.png");
		setA(d, WANT_SELECT, 0, 22);
		setA(d, WANT_IDLE, 23, 67, true);
		setA(d, WANT_MOVE, 68, 92, true);
		setA(d, WANT_FIRE, 93, 99);
		setA(d, WANT_RELOAD, 100, 252);
		setA(d, WANT_COCK, 253, 296);
		setA(d, WANT_PUTAWAY, 297, 316);
		gunConfigs.push_back(d);
	}

	// Grenade: gun
	{
		GunConfig d; d.type = Grenade; d.mesh1st = "HUD_Grenade.b3d";
		d.tex1st.push_back("Grenade_gun.png");
		d.mesh3rd = "W_Grenade.b3d"; d.tex3rd.push_back("Grenade.png");
		setA(d, WANT_SELECT, 0, 14);
		setA(d, WANT_IDLE, 16, 64, true);
		setA(d, WANT_MOVE, 66, 94, true);
		setA(d, WANT_FIRE, 96, 139);
		setA(d, WANT_RELOAD, 1, 14);
		setA(d, WANT_PUTAWAY, 141, 154);
		gunConfigs.push_back(d);
	}

	// Knife: gun
	{
		GunConfig d; d.type = Knife; d.mesh1st = "HUD_Knife.b3d";
		d.tex1st.push_back("Knife_gun.png");
		d.mesh3rd = "W_Knife.b3d"; d.tex3rd.push_back("Knife.png");
		setA(d, WANT_SELECT, 0, 14);
		setA(d, WANT_IDLE, 16, 64, true);
		setA(d, WANT_MOVE, 66, 94, true);
		setA(d, WANT_FIRE, 96, 114);
		setA(d, WANT_PUTAWAY, 116, 129);
		gunConfigs.push_back(d);
	}

	// Panzerfaust: gun
	{
		GunConfig d; d.type = Panzerfaust; d.mesh1st = "HUD_Panzerfaust.b3d";
		d.tex1st.push_back("Panzerfaust_gun.png");
		d.mesh3rd = "W_Panzerfaust.b3d"; d.tex3rd.push_back("Panzerfaust.png");
		setA(d, WANT_SELECT, 0, 33);
		setA(d, WANT_IDLE, 35, 83, true);
		setA(d, WANT_MOVE, 85, 113, true);
		setA(d, WANT_FIRE, 184, 225);
		setA(d, WANT_RELOAD, 125, 182);
		setA(d, WANT_PUTAWAY, 251, 283);
		gunConfigs.push_back(d);
	}

	// 2. Load and register models
	for (u32 i = 0; i < gunConfigs.size(); ++i)
	{
		GunConfig& cfg = gunConfigs[i];

		auto loadModel = [&](const std::string& file, const core::array<std::string>& texs, GunType t, const AnimRange* anims) -> ModelInfo {
			ModelInfo info;
			info.type = t;
			info.meshFile = file;
			info.textureFiles = texs;
			for (int a = 0; a < WANT_COUNT; ++a) info.animations[a] = anims[a];

			// Search in Asset/Character directory
			core::stringc path = CInGame::Instance()->mediaPath + "Character/" + file.c_str();
			scene::IAnimatedMesh* mesh = smgr->getMesh(path);
			info.Node = smgr->addAnimatedMeshSceneNode(mesh);

			if (info.Node) {
				for (u32 j = 0; j < texs.size(); ++j) {
					core::stringc texPath = CInGame::Instance()->mediaPath + "Character/" + texs[j].c_str();
					video::ITexture* tex = driver->getTexture(texPath);
					if (tex && info.Node->getMaterialCount() > j) {
						// 각 부위(Mesh Buffer)의 0번 텍스처 레이어에 할당
						info.Node->getMaterial(j).setTexture(0, tex);
					}
				}
				info.Node->setVisible(false);
				info.Node->setMaterialFlag(video::EMF_LIGHTING, false);
			}
			return info;
		};

		ModelInfo m1st = loadModel(cfg.mesh1st, cfg.tex1st, cfg.type, cfg.animations);
		ModelInfo m3rd = loadModel(cfg.mesh3rd, cfg.tex3rd, cfg.type, cfg.animations);

		CharModelArr.push_back(std::make_pair(m1st, m3rd));
	}
}

void SceneManager::SwitchToNextScene()
{
	currentScene++;
	//if (currentScene > 3)
	if (currentScene > 1)
		currentScene = 1;
	
	switch (currentScene)
	{
	case -1: // loading screen
		timeForThisScene = 0;
		CreateLoadingScreen();
		break;

	case 0: // load scene
		timeForThisScene = 0;
		LoadSceneData();
		LoadCharacterData();
		break;
	case 1: // interactive, go around
		if (model1) model1->setVisible(true);
		if (model2) model2->setVisible(true);
		//campFire->setVisible(true);
		timeForThisScene = -1;
		CreateCamera( );
		break;
	}
	sceneStartTime = device->getTimer()->getTime();
}

void SceneManager::LoadPlaneScene( )
{
	core::stringc mediaPath = CInGame::Instance( )->mediaPath; 
	// Register custom factory for unsupported nodes
	CUnsupportedNodeFactory* factory = new CUnsupportedNodeFactory(smgr);
	smgr->registerSceneNodeFactory(factory);
	factory->drop();

	smgr->loadScene("map.irr");

	// Convert unsupported nodes to supported types
	ConvertUnsupportedNodes(smgr);

	// Increase ambient light to fix dark map issue
	smgr->setAmbientLight(video::SColorf(0.6f, 0.6f, 0.6f, 1.0f));
	
	// Initialize member metaSelector (Important for CreateCamera gravity/collision)
	if (metaSelector)
		metaSelector->drop();
	metaSelector = smgr->createMetaTriangleSelector();

	core::array<scene::ISceneNode *> nodes;
	scene::IMesh * mesh;
	smgr->getSceneNodesFromType(scene::ESNT_ANY, nodes); // Find all nodes

	for ( u32 i = 0; i < nodes.size( ); ++i )
	{
		scene::ISceneNode * node = nodes[i];
		scene::ITriangleSelector * selector = 0;
		const c8 * typeName = smgr->getSceneNodeTypeName(node->getType( ));
		//NetLogManager::Instance( )->PrintDebug("type : %s\n", typeName);
		
		switch ( node->getType( ) )
		{
		case scene::ESNT_CUBE:
		case scene::ESNT_ANIMATED_MESH:
			selector = smgr->createTriangleSelectorFromBoundingBox(node);
			node->setMaterialFlag(video::EMF_LIGHTING, true);
			if (shadows) ((scene::IAnimatedMeshSceneNode*)node)->addShadowVolumeSceneNode();
			break;

		case scene::ESNT_MESH:
		case scene::ESNT_SPHERE: // Derived from IMeshSceneNode
			// 1. Selector 생성
			mesh =((scene::IMeshSceneNode * )node)->getMesh( );
			if ( mesh ) selector = smgr->createTriangleSelector(mesh, node);

			// 2. [Light 적용] 재질 설정
			node->setMaterialFlag(video::EMF_LIGHTING, true);
			node->setMaterialFlag(video::EMF_NORMALIZE_NORMALS, true);
			
			//if (shadows) ((scene::IMeshSceneNode*)node)->addShadowVolumeSceneNode();
			break;

		case scene::ESNT_TERRAIN:
			selector = smgr->createTerrainTriangleSelector(( scene::ITerrainSceneNode * ) node);
			node->setMaterialFlag(video::EMF_LIGHTING, true);
			break;

		case scene::ESNT_OCTREE:
			selector = smgr->createOctreeTriangleSelector(( ( scene::IMeshSceneNode * ) node )->getMesh( ), node);
			node->setMaterialFlag(video::EMF_LIGHTING, true);
			//if (shadows) ((scene::IMeshSceneNode*)node)->addShadowVolumeSceneNode();
			break;
		case scene::ESNT_LIGHT:
			// 감쇠율 설정 (빛이 거리에 따라 부드럽게 사라지게)
			( ( scene::ILightSceneNode * ) node )->getLightData( ).Attenuation.set(0.0f, 1.0f / 1000.0f, 0.0f);
			break;
		case scene::ESNT_UNKNOWN:
			break;
		default:
			break;
		}

		if ( selector )
		{
			// Add it to the meta selector
			metaSelector->addTriangleSelector(selector);
			selector->drop( );
		}
	}
}

void SceneManager::LoadQuakeScene( )
{
	// load quake level
	video::IVideoDriver * driver = device->getVideoDriver( );
	scene::ISceneManager * sm = device->getSceneManager( );

	core::stringc mediaPath = CInGame::Instance( )->mediaPath;
	// Quake3 Shader controls Z-Writing
	sm->getParameters( )->setAttribute(scene::ALLOW_ZWRITE_ON_TRANSPARENT, true);
	quakeLevelMesh = ( scene::IQ3LevelMesh * ) sm->getMesh("20kdm2.bsp");

#ifdef __ANDROID__
	if ( !quakeLevelMesh ) {
		NetLogManager::Instance( )->PrintDebug("Error: Quake3 Level Mesh �ε� ����!");
	}
	scene::IMesh * levelMesh = quakeLevelMesh->getMesh(scene::quake3::E_Q3_MESH_GEOMETRY);
	if ( !levelMesh ) {
		NetLogManager::Instance( )->PrintDebug("Error: Quake Level Mesh Geometry�� NULL!");
	}
#endif // __ANDROID__

	if ( quakeLevelMesh )
	{
		u32 i;
		//move all quake level meshes (non-realtime)
		core::matrix4 m;
		m.setTranslation(core::vector3df(-1300, -70, -1249));

		for ( i = 0; i != scene::quake3::E_Q3_MESH_SIZE; ++i )
		{
			sm->getMeshManipulator( )->transform(quakeLevelMesh->getMesh(i), m);
		}

		quakeLevelNode = sm->addOctreeSceneNode(
			quakeLevelMesh->getMesh(scene::quake3::E_Q3_MESH_GEOMETRY)
		);
		if ( quakeLevelNode )
		{
			//quakeLevelNode->setPosition(core::vector3df(-1300,-70,-1249));
			quakeLevelNode->setVisible(true);

			// create map triangle selector
			mapSelector = sm->createOctreeTriangleSelector(quakeLevelMesh->getMesh(0),
				quakeLevelNode, 128);

			// if not using shader and no gamma it's better to use more lighting, because
			// quake3 level are usually dark
			quakeLevelNode->setMaterialType(video::EMT_LIGHTMAP_M4);

			// set additive blending if wanted
			if ( additive )
				quakeLevelNode->setMaterialType(video::EMT_LIGHTMAP_ADD);
		}

		// the additional mesh can be quite huge and is unoptimized
		scene::IMesh * additional_mesh = quakeLevelMesh->getMesh(scene::quake3::E_Q3_MESH_ITEMS);

		for ( i = 0; i != additional_mesh->getMeshBufferCount( ); ++i )
		{
			scene::IMeshBuffer * meshBuffer = additional_mesh->getMeshBuffer(i);
			const video::SMaterial & material = meshBuffer->getMaterial( );

			//! The ShaderIndex is stored in the material parameter
			s32 shaderIndex = ( s32 ) material.MaterialTypeParam2;

			// the meshbuffer can be rendered without additional support, or it has no shader
			const scene::quake3::IShader * shader = quakeLevelMesh->getShader(shaderIndex);
			if ( 0 == shader )
			{
				continue;
			}
			// Now add the MeshBuffer(s) with the current Shader to the Manager
			sm->addQuake3SceneNode(meshBuffer, shader);
		}
	}


	scene::ISceneNodeAnimator * anim = 0;
	// create sky box
	driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, false);
	skyboxNode = sm->addSkyBoxSceneNode(
		driver->getTexture(mediaPath + "irrlicht2_up.jpg"),
		driver->getTexture(mediaPath + "irrlicht2_dn.jpg"),
		driver->getTexture(mediaPath + "irrlicht2_lf.jpg"),
		driver->getTexture(mediaPath + "irrlicht2_rt.jpg"),
		driver->getTexture(mediaPath + "irrlicht2_ft.jpg"),
		driver->getTexture(mediaPath + "irrlicht2_bk.jpg"));

	core::vector3df waypoint[2];
	waypoint[0].set(-150, 40, 100);
	waypoint[1].set(350, 40, 100);

	//if (model2)
	//{
	//	anim = device->getSceneManager()->createFlyStraightAnimator(waypoint[0],
	//		waypoint[1], 2000, true);
	//	model2->addAnimator(anim);
	//	anim->drop();
	//}

	// create animation for portals;

	core::array<video::ITexture *> textures;
	for ( s32 g = 1; g < 8; ++g )
	{
		core::stringc tmp(IRRLICHT_MEDIA_PATH "portal");
		tmp += g;
		tmp += ".png";
		video::ITexture * t = driver->getTexture(tmp);
		textures.push_back(t);
	}

	anim = sm->createTextureAnimator(textures, 100);

	// create portals

	scene::IBillboardSceneNode * bill = 0;

	for ( int r = 0; r < 2; ++r )
	{
		bill = sm->addBillboardSceneNode(0, core::dimension2d<f32>(100, 100),
			waypoint[r] + core::vector3df(0, 20, 0));
		bill->setMaterialFlag(video::EMF_LIGHTING, false);
		bill->setMaterialTexture(0, driver->getTexture(mediaPath + "portal1.png"));
		bill->setMaterialType(video::EMT_TRANSPARENT_ADD_COLOR);
		bill->addAnimator(anim);
	}

	anim->drop( );

	// create cirlce flying dynamic light with transparent billboard attached

	scene::ILightSceneNode * light = 0;

	light = sm->addLightSceneNode(0,
		core::vector3df(0, 0, 0), video::SColorf(1.0f, 1.0f, 1.f, 1.0f), 500.f);

	anim = sm->createFlyCircleAnimator(
		core::vector3df(100, 150, 80), 80.0f, 0.0005f);

	light->addAnimator(anim);
	anim->drop( );

	bill = device->getSceneManager( )->addBillboardSceneNode(
		light, core::dimension2d<f32>(40, 40));
	bill->setMaterialFlag(video::EMF_LIGHTING, false);
	bill->setMaterialTexture(0, driver->getTexture(mediaPath + "particlewhite.bmp"));
	bill->setMaterialType(video::EMT_TRANSPARENT_ADD_COLOR);

	// create meta triangle selector with all triangles selectors in it.
	metaSelector = sm->createMetaTriangleSelector( );
	if ( mapSelector ) metaSelector->addTriangleSelector(mapSelector);

	// create camp fire
	campFire = sm->addParticleSystemSceneNode(false);
	campFire->setPosition(core::vector3df(100, 120, 600));
	//campFire->setPosition(core::vector3df(279.522980, 100.080017, -290.277802));
	campFire->setScale(core::vector3df(2, 2, 2));

	scene::IParticleEmitter * em = campFire->createBoxEmitter(
		core::aabbox3d<f32>(-7, 0, -7, 7, 1, 7),
		core::vector3df(0.0f, 0.06f, 0.0f),
		80, 100, video::SColor(0, 255, 255, 255), video::SColor(0, 255, 255, 255), 800, 2000);

	em->setMinStartSize(core::dimension2d<f32>(20.0f, 10.0f));
	em->setMaxStartSize(core::dimension2d<f32>(20.0f, 10.0f));
	campFire->setEmitter(em);
	em->drop( );

	scene::IParticleAffector * paf = campFire->createFadeOutParticleAffector( );
	campFire->addAffector(paf);
	paf->drop( );

	campFire->setMaterialFlag(video::EMF_LIGHTING, false);
	campFire->setMaterialFlag(video::EMF_ZWRITE_ENABLE, false);
	campFire->setMaterialTexture(0, driver->getTexture(mediaPath + "fireball.bmp"));
	campFire->setMaterialType(video::EMT_TRANSPARENT_VERTEX_ALPHA);
}

void SceneManager::LoadSceneData()
{
	Cleanup();
	LoadPlaneScene( );
	//loadQuakeScene( );
}
void SceneManager:: CreateLoadingScreen()
{
	core::dimension2d<u32> size = device->getVideoDriver()->getScreenSize();

	if (device->getCursorControl() != nullptr) device->getCursorControl()->setVisible(false);

	// setup loading screen
	backColor.set(255, 90, 90, 156);

	// create in fader
	//inOutFader = device->getGUIEnvironment()->addInOutFader();
	//inOutFader->setColor(backColor,	video::SColor ( 0, 230, 230, 230 ));

	// loading text
	const int lwidth = size.Width - 20;
	const int lheight = 16;

#ifdef __ANDROID__
	device->getGUIEnvironment()->getSkin()->setFont(device->getGUIEnvironment()->getFont(CInGame::Instance()->mediaPath + "Font/bigfont.png"));
#else
	device->getGUIEnvironment()->getSkin()->setFont(device->getGUIEnvironment()->getFont(CInGame::Instance()->mediaPath + "Font/fonthaettenschweiler.bmp"));
#endif

	device->getGUIEnvironment()->getSkin()->setColor(gui::EGDC_BUTTON_TEXT,
		video::SColor(255, 100, 100, 100));
}

RakNet::TimeMS SceneManager::shootFromOrigin(core::vector3df camPosition, core::vector3df camAt, GamePlatform platform)
{
	scene::ISceneManager* sm = device->getSceneManager();
	scene::ICameraSceneNode* camera = sm->getActiveCamera();
	// get line of camera
	core::vector3df start = camPosition;
	core::vector3df end = (camAt);
	//end.normalize();
	start += end * 8.0f;
	end = start + (end * camera->getFarValue());

	bool wallHit = false;
	core::vector3df wallHitPoint(0, 0, 0);
	return shootFromOrigin(camPosition, camAt, start, end, wallHit, wallHitPoint, platform);
}

RakNet::TimeMS SceneManager::shootFromOrigin(core::vector3df camPosition, core::vector3df camAt, core::vector3df start, core::vector3df end, bool& wallHit, core::vector3df& wallHitPoint, GamePlatform platform)
{
	scene::ISceneManager* sm = device->getSceneManager();
	scene::ICameraSceneNode* camera = sm->getActiveCamera();

	if (!camera || !mapSelector)
		return 0;

	SParticleImpact imp;
	imp.when = 0;
	core::triangle3df triangle;
	core::line3d<irr::f32> line(start, end);

	// get intersection point with map
	const scene::ISceneNode* hitNode;

#ifdef __ANDROID__
	scene::SCollisionHit hitResult;
	bool flag = false;
	if (sm->getSceneCollisionManager()->getCollisionPoint(hitResult, line, mapSelector)) {
		end = hitResult.Intersection;
		triangle = hitResult.Triangle;
		hitNode = hitResult.Node;
		flag = true;
	}

	if (flag) {
		// collides with wall
		wallHit = true;
		wallHitPoint = end;

		core::vector3df out = triangle.getNormal();
		out.setLength(0.03f);

		imp.when = 1;
		imp.outVector = out;
		imp.pos = end;
	}
	else {
		// doesnt collide with wall
		wallHit = false;
		wallHitPoint = core::vector3df(0, 0, 0);

		core::vector3df start = camPosition;
		core::vector3df end = (camAt);
		//end.normalize();
		start += end * 8.0f;
		end = start + (end * camera->getFarValue());
	}

#else
	if (sm->getSceneCollisionManager()->getCollisionPoint(line, mapSelector, end, triangle, hitNode))
	{
		// collides with wall
		wallHit = true;
		wallHitPoint = end;

		core::vector3df out = triangle.getNormal();
		out.setLength(0.03f);

		imp.when = 1;
		imp.outVector = out;
		imp.pos = end;
	}
	else
	{
		// doesnt collide with wall
		wallHit = false;
		wallHitPoint = core::vector3df(0, 0, 0);

		core::vector3df start = camPosition;
		core::vector3df end = (camAt);
		//end.normalize();
		start += end * 8.0f;
		end = start + (end * camera->getFarValue());
	}
#endif // __ANDROID__

	// create fire ball
	scene::ISceneNode* node = 0;
	node = sm->addBillboardSceneNode(0,
		core::dimension2d<f32>(BALL_DIAMETER - 10, BALL_DIAMETER - 10), start);

	node->setMaterialFlag(video::EMF_LIGHTING, false);
	if (platform == Holder) {
		node->setMaterialTexture(0, device->getVideoDriver()->getTexture("fireball_green.bmp"));
	}
	else if (platform == Shooter) {
		node->setMaterialTexture(0, device->getVideoDriver()->getTexture("fireball_blue.bmp"));
	}
	else if (platform == Server) {
		node->setMaterialTexture(0, device->getVideoDriver()->getTexture("fireball.bmp"));
	}

	node->setMaterialType(video::EMT_TRANSPARENT_ADD_COLOR);

	f32 length = (f32)(end - start).getLength();
	const f32 speed = SHOT_SPEED;
	u32 time = (u32)(length / speed);

	scene::ISceneNodeAnimator* anim = 0;

	// set flight line
	anim = sm->createFlyStraightAnimator(start, end, time);
	node->addAnimator(anim);
	anim->drop();

	anim = sm->createDeleteAnimator(time);
	node->addAnimator(anim);
	anim->drop();

	if (imp.when)
	{
		// create impact note
		imp.when = device->getTimer()->getTime() + (time - 100);
		Impacts.push_back(imp);
	}

	return (RakNet::TimeMS)time;
}