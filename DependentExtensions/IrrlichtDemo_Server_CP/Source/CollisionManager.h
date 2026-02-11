#pragma once
#include "NetworkManager.h"
#include "SceneManager.h"
#include "IAnimatedMeshSceneNode.h"
using namespace std;
using namespace irr;
using namespace RakNet;

class CollisionBoxQueueSceneNode;
struct CollDebugState {
	RakNet::TimeMS timeStamp;
	std::vector<core::triangle3df> tris;
	s32 outCount;
	core::line3d<f32> line;
	std::vector<core::line3d<f32>> extraLines;
	RakNet::TimeMS drawTimeOut;
	video::SColor color;
};

struct HitInfo {
	const char* shooterAddr;
	const char* holderAddr;
	int nowScore;
	unsigned long timeStamp;
};

irr::scene::ITriangleSelector* CreateSelectorFromTransformedBox(const irr::core::aabbox3df& localBox, const irr::core::matrix4& worldTransform, irr::scene::ISceneManager* smgr, const RakNet::RakNetGUID& guid); //Collision Check ��
void DrawBoxTriangles(irr::scene::ITriangleSelector* selector, const irr::core::matrix4& transform, irr::video::IVideoDriver* driver);
void DrawDebugFrame(const irr::core::aabbox3df& boundingBox, irr::core::vector3df position, float rotationAroundYAxis, irr::scene::ISceneManager* sm, RakNet::RakNetGUID g, RakNet::TimeMS drawTimeOut);
void DrawDebugFrame(irr::scene::ITriangleSelector* selector, RakNet::TimeMS drawTimeOut, bool isDrop = false);

class DebugBoxSceneNode : public scene::ISceneNode
{
public:
	DebugBoxSceneNode(scene::ISceneNode* parent, scene::ISceneManager* mgr, s32 id = -1);
	virtual const core::aabbox3d<f32>& getBoundingBox() const;
	virtual void OnRegisterSceneNode();
	virtual void render();
	void SetSelector(irr::scene::ITriangleSelector* selector);
	void EnableDrawTriangles(bool enable);

private:
	scene::ITriangleSelector* triangleSelector = nullptr;
	bool drawTriangles = false;
};
class CollisionBoxQueueSceneNode : public scene::ISceneNode
{
public:
	CollisionBoxQueueSceneNode(scene::ISceneNode* parent, scene::ISceneManager* mgr, s32 id = -1);
	virtual const core::aabbox3d<f32>& getBoundingBox() const;
	virtual void OnRegisterSceneNode();
	virtual void render();
	DataStructures::Queue <CollDebugState>* GetQueue() { return &q; }

private:
	DataStructures::Queue <CollDebugState> q;
};

class CollisionManager
{
public:
	static CollisionManager* Instance();
	static void DestroyInstance();
	void BulletHitDetected(RakNet::RakNetGUID creatingSystemGUID, float ingoingTimeMS, GunType gunType = AT9mm, core::vector3df overrideStart = core::vector3df(0,0,0), core::vector3df overrideDir = core::vector3df(0,0,0));
	void DrawDebugLine(core::line3d<f32> line, video::SColor color = video::SColor(255, 0, 255, 0), RakNet::TimeMS drawTimeOut = 1000);
	void DrawDebugCircle(core::vector3df center, f32 radius, core::vector3df normal, video::SColor color = video::SColor(255, 0, 255, 0), RakNet::TimeMS drawTimeOut = 1000);
	void DrawDebugEllipsoid(core::vector3df center, core::vector3df radius, video::SColor color = video::SColor(255, 0, 255, 0), RakNet::TimeMS drawTimeOut = 1000);
	void DrawDebugArea(core::vector3df p1, core::vector3df p2, core::vector3df p3, core::vector3df p4, video::SColor color = video::SColor(255, 0, 255, 0), RakNet::TimeMS drawTimeOut = 1000);
	CollisionBoxQueueSceneNode* collisionBoxQueue;
	bool isWithinRange(f32 value, f32 target, f32 range) { return (target - range <= value) && (value <= target + range); }
private:
	CollisionManager();
	~CollisionManager();
	static CollisionManager* instance;
};


