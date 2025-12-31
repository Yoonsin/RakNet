#pragma once
#include "NetworkManager.h"
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
	RakNet::TimeMS drawTimeOut;
};

struct HitInfo {
	const char* shooterAddr;
	const char* holderAddr;
	int nowScore;
	unsigned long timeStamp;
};

irr::scene::ITriangleSelector* CreateSelectorFromTransformedBox(const irr::core::aabbox3df& localBox, const irr::core::matrix4& worldTransform, irr::scene::ISceneManager* smgr, const RakNet::RakNetGUID& guid); //Collision Check ¿ë
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
	CDemo* demo;
	scene::ITriangleSelector* triangleSelector = nullptr;
	bool drawTriangles = false;
};
class CollisionBoxQueueSceneNode : public scene::ISceneNode
{
public:
	inline bool isWithinRange(f32 value, f32 target, f32 range) { return (target - range <= value) && (value <= target + range); }
	CollisionBoxQueueSceneNode(scene::ISceneNode* parent, scene::ISceneManager* mgr, s32 id = -1);
	virtual const core::aabbox3d<f32>& getBoundingBox() const;
	virtual void OnRegisterSceneNode();
	virtual void render();
	DataStructures::Queue <CollDebugState>* GetQueue() { return &q; }

private:
	CDemo* demo;
	DataStructures::Queue <CollDebugState> q;
};

class CollisionManager
{
public:
	static CollisionManager* Instance();
	static void DestroyInstance();
	void BulletHitDetected(RakNet::RakNetGUID creatingSystemGUID, float ingoingTimeMS);
	CollisionBoxQueueSceneNode* collisionBoxQueue;
private:
	CollisionManager();
	~CollisionManager();
	static CollisionManager* instance;
};


