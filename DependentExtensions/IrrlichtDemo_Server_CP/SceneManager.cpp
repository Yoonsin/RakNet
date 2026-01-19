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
	Impacts.clear();
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
}

void SceneManager::Activate() {
	this->device = CInGame::Instance()->GetDevice();
	driver = device->getVideoDriver();
	smgr = device->getSceneManager();
	guienv = device->getGUIEnvironment();
	sceneStartTime = device->getTimer()->getTime();
}

void SceneManager::Update() {
	now = device->getTimer()->getTime();
	if (now - sceneStartTime > timeForThisScene && timeForThisScene != -1)
		switchToNextScene();

	createParticleImpacts();

	driver->beginScene(timeForThisScene != -1, true, backColor);
	smgr->drawAll();
	guienv->drawAll();
	HUDManager::Instance()->Update();
	driver->endScene();

}

void SceneManager::createParticleImpacts()
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


void SceneManager::switchToNextScene()
{
	currentScene++;
	//if (currentScene > 3)
	if (currentScene > 1)
		currentScene = 1;

	scene::ISceneManager* sm = device->getSceneManager();
	scene::ISceneNodeAnimator* sa = 0;
	scene::ICameraSceneNode* camera = 0;

	camera = sm->getActiveCamera();
	if (camera)
	{
		sm->setActiveCamera(0);
		camera->remove();
		camera = 0;
	}

	switch (currentScene)
	{
	case -1: // loading screen
		timeForThisScene = 0;
		createLoadingScreen();
		break;

	case 0: // load scene
		timeForThisScene = 0;
		loadSceneData();
		break;
	case 1: // interactive, go around
	{
		if (model1)
			model1->setVisible(true);
		if (model2)
			model2->setVisible(true);
		campFire->setVisible(true);
		timeForThisScene = -1;

#ifdef __ANDROID__
		SKeyMap keyMap[11];
#else
		SKeyMap keyMap[9];
#endif // __ANDROID__
		keyMap[0].Action = EKA_MOVE_FORWARD;
		keyMap[0].KeyCode = KEY_UP;
		keyMap[1].Action = EKA_MOVE_FORWARD;
		keyMap[1].KeyCode = KEY_KEY_W;

		keyMap[2].Action = EKA_MOVE_BACKWARD;
		keyMap[2].KeyCode = KEY_DOWN;
		keyMap[3].Action = EKA_MOVE_BACKWARD;
		keyMap[3].KeyCode = KEY_KEY_S;

		keyMap[4].Action = EKA_STRAFE_LEFT;
		keyMap[4].KeyCode = KEY_LEFT;
		keyMap[5].Action = EKA_STRAFE_LEFT;
		keyMap[5].KeyCode = KEY_KEY_A;

		keyMap[6].Action = EKA_STRAFE_RIGHT;
		keyMap[6].KeyCode = KEY_RIGHT;
		keyMap[7].Action = EKA_STRAFE_RIGHT;
		keyMap[7].KeyCode = KEY_KEY_D;

		keyMap[8].Action = EKA_JUMP_UP;
		//keyMap[8].KeyCode = KEY_KEY_J;
		keyMap[8].KeyCode = KEY_SPACE;

		//rotate
#ifdef __ANDROID__
		keyMap[9].Action = EKA_ROTATE_LEFT;
		keyMap[9].KeyCode = KEY_KEY_0;
		keyMap[10].Action = EKA_ROTATE_RIGHT;
		keyMap[10].KeyCode = KEY_KEY_1;
		//camera = sm->addCameraSceneNodeFPS(0, 1.0f, .4f, -1, keyMap, 11, true, 250.f); //�⺻ �÷���
		camera = sm->addCameraSceneNodeFPS(0, 1.0f, .4f, -1, keyMap, 11, false, 5.f); //�⺻ �÷���
		CInGame::Instance()->SetTransformCamera(camera, CInGame::Instance()->gamePlatform);

		core::vector3df gravity = core::vector3df(0, /*-300.f*/quakeLevelMesh ? -10.f : 0.0f, 0);
		scene::ISceneNodeAnimatorCollisionResponse* collider =
			sm->createCollisionResponseAnimator(
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
#else
			//Last parameter is jump speed
			//Tweaked so you can get up ladders
		camera = sm->addCameraSceneNodeFPS(0, 100.0f, .4f, -1, keyMap, 9, false, 5.f);
		CInGame::Instance()->SetTransformCamera(camera, CInGame::Instance()->gamePlatform);

		core::vector3df gravity = core::vector3df(0, quakeLevelMesh ? -10.f : 0.0f, 0);
		scene::ISceneNodeAnimatorCollisionResponse* collider =
			sm->createCollisionResponseAnimator(
				metaSelector, camera, core::vector3df(25, CAMERA_HEIGHT, 25), gravity, core::vector3df(0, 45, 0), 0.005f);

		//	waypoint[0].set(-150,40,100); waypoint[1].set(350, 40, 100);
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
	}
	break;
	}
	sceneStartTime = device->getTimer()->getTime();
}
void SceneManager::loadSceneData()
{
	Cleanup();
	// load quake level
	video::IVideoDriver* driver = device->getVideoDriver();
	scene::ISceneManager* sm = device->getSceneManager();

	// Quake3 Shader controls Z-Writing
	sm->getParameters()->setAttribute(scene::ALLOW_ZWRITE_ON_TRANSPARENT, true);

	quakeLevelMesh = (scene::IQ3LevelMesh*)sm->getMesh("20kdm2.bsp");

#ifdef __ANDROID__
	if (!quakeLevelMesh) {
		NetLogManager::Instance()->PrintDebug("Error: Quake3 Level Mesh �ε� ����!");
	}
	scene::IMesh* levelMesh = quakeLevelMesh->getMesh(scene::quake3::E_Q3_MESH_GEOMETRY);
	if (!levelMesh) {
		NetLogManager::Instance()->PrintDebug("Error: Quake Level Mesh Geometry�� NULL!");
	}
#endif // __ANDROID__

	if (quakeLevelMesh)
	{
		u32 i;
		//move all quake level meshes (non-realtime)
		core::matrix4 m;
		m.setTranslation(core::vector3df(-1300, -70, -1249));

		for (i = 0; i != scene::quake3::E_Q3_MESH_SIZE; ++i)
		{
			sm->getMeshManipulator()->transform(quakeLevelMesh->getMesh(i), m);
		}

		quakeLevelNode = sm->addOctreeSceneNode(
			quakeLevelMesh->getMesh(scene::quake3::E_Q3_MESH_GEOMETRY)
		);
		if (quakeLevelNode)
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
			if (additive)
				quakeLevelNode->setMaterialType(video::EMT_LIGHTMAP_ADD);
		}

		// the additional mesh can be quite huge and is unoptimized
		scene::IMesh* additional_mesh = quakeLevelMesh->getMesh(scene::quake3::E_Q3_MESH_ITEMS);

		for (i = 0; i != additional_mesh->getMeshBufferCount(); ++i)
		{
			scene::IMeshBuffer* meshBuffer = additional_mesh->getMeshBuffer(i);
			const video::SMaterial& material = meshBuffer->getMaterial();

			//! The ShaderIndex is stored in the material parameter
			s32 shaderIndex = (s32)material.MaterialTypeParam2;

			// the meshbuffer can be rendered without additional support, or it has no shader
			const scene::quake3::IShader* shader = quakeLevelMesh->getShader(shaderIndex);
			if (0 == shader)
			{
				continue;
			}
			// Now add the MeshBuffer(s) with the current Shader to the Manager
			sm->addQuake3SceneNode(meshBuffer, shader);
		}
	}
	scene::ISceneNodeAnimator* anim = 0;
	// create sky box
	driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, false);
	core::stringc mediaPath = CInGame::Instance()->mediaPath;
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

	core::array<video::ITexture*> textures;
	for (s32 g = 1; g < 8; ++g)
	{
		core::stringc tmp(IRRLICHT_MEDIA_PATH "portal");
		tmp += g;
		tmp += ".png";
		video::ITexture* t = driver->getTexture(tmp);
		textures.push_back(t);
	}

	anim = sm->createTextureAnimator(textures, 100);

	// create portals

	scene::IBillboardSceneNode* bill = 0;

	for (int r = 0; r < 2; ++r)
	{
		bill = sm->addBillboardSceneNode(0, core::dimension2d<f32>(100, 100),
			waypoint[r] + core::vector3df(0, 20, 0));
		bill->setMaterialFlag(video::EMF_LIGHTING, false);
		bill->setMaterialTexture(0, driver->getTexture(mediaPath + "portal1.png"));
		bill->setMaterialType(video::EMT_TRANSPARENT_ADD_COLOR);
		bill->addAnimator(anim);
	}

	anim->drop();

	// create cirlce flying dynamic light with transparent billboard attached

	scene::ILightSceneNode* light = 0;

	light = sm->addLightSceneNode(0,
		core::vector3df(0, 0, 0), video::SColorf(1.0f, 1.0f, 1.f, 1.0f), 500.f);

	anim = sm->createFlyCircleAnimator(
		core::vector3df(100, 150, 80), 80.0f, 0.0005f);

	light->addAnimator(anim);
	anim->drop();

	bill = device->getSceneManager()->addBillboardSceneNode(
		light, core::dimension2d<f32>(40, 40));
	bill->setMaterialFlag(video::EMF_LIGHTING, false);
	bill->setMaterialTexture(0, driver->getTexture(mediaPath + "particlewhite.bmp"));
	bill->setMaterialType(video::EMT_TRANSPARENT_ADD_COLOR);

	// create meta triangle selector with all triangles selectors in it.
	metaSelector = sm->createMetaTriangleSelector();
	if (mapSelector) metaSelector->addTriangleSelector(mapSelector);

	// create camp fire
	campFire = sm->addParticleSystemSceneNode(false);
	campFire->setPosition(core::vector3df(100, 120, 600));
	//campFire->setPosition(core::vector3df(279.522980, 100.080017, -290.277802));
	campFire->setScale(core::vector3df(2, 2, 2));

	scene::IParticleEmitter* em = campFire->createBoxEmitter(
		core::aabbox3d<f32>(-7, 0, -7, 7, 1, 7),
		core::vector3df(0.0f, 0.06f, 0.0f),
		80, 100, video::SColor(0, 255, 255, 255), video::SColor(0, 255, 255, 255), 800, 2000);

	em->setMinStartSize(core::dimension2d<f32>(20.0f, 10.0f));
	em->setMaxStartSize(core::dimension2d<f32>(20.0f, 10.0f));
	campFire->setEmitter(em);
	em->drop();

	scene::IParticleAffector* paf = campFire->createFadeOutParticleAffector();
	campFire->addAffector(paf);
	paf->drop();

	campFire->setMaterialFlag(video::EMF_LIGHTING, false);
	campFire->setMaterialFlag(video::EMF_ZWRITE_ENABLE, false);
	campFire->setMaterialTexture(0, driver->getTexture(mediaPath + "fireball.bmp"));
	campFire->setMaterialType(video::EMT_TRANSPARENT_VERTEX_ALPHA);
}
void SceneManager::createLoadingScreen()
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
	device->getGUIEnvironment()->getSkin()->setFont(device->getGUIEnvironment()->getFont(CInGame::Instance()->mediaPath + "bigfont.png"));
#else
	device->getGUIEnvironment()->getSkin()->setFont(device->getGUIEnvironment()->getFont(CInGame::Instance()->mediaPath + "fonthaettenschweiler.bmp"));
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

	core::stringc mediaPath = CInGame::Instance()->mediaPath;
	node->setMaterialFlag(video::EMF_LIGHTING, false);
	if (platform == Holder) {
		node->setMaterialTexture(0, device->getVideoDriver()->getTexture(mediaPath + "fireball_green.bmp"));
	}
	else if (platform == Shooter) {
		node->setMaterialTexture(0, device->getVideoDriver()->getTexture(mediaPath + "fireball_blue.bmp"));
	}
	else if (platform == Server) {
		node->setMaterialTexture(0, device->getVideoDriver()->getTexture(mediaPath + "fireball.bmp"));
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