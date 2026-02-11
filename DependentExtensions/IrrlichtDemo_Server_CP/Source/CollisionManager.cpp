#include "CollisionManager.h"
#include "CInGame.h"
#include "SceneManager.h"
#include "PacketHandler.h"
#include "InputController.h"
#include "GetTime.h"
#include "irrlicht.h"
#include "HUDManager.h"

using namespace RakNet;
using namespace irr;

DebugBoxSceneNode::DebugBoxSceneNode(scene::ISceneNode* parent, scene::ISceneManager* mgr, s32 id) : scene::ISceneNode(parent, mgr, id)
{
#ifdef _DEBUG
	setDebugName("DebugBoxSceneNode");
#endif
	setAutomaticCulling(scene::EAC_OFF);
}
const core::aabbox3d<f32>& DebugBoxSceneNode::getBoundingBox() const
{
	return SceneManager::Instance( )->GetPlayerBoundingBox( );
}
void DebugBoxSceneNode::OnRegisterSceneNode()
{
	if (IsVisible)
		CInGame::Instance()->GetSceneManager()->registerNodeForRendering(this, scene::ESNRP_SOLID);
}
void DebugBoxSceneNode::SetSelector(scene::ITriangleSelector* selector) {
	triangleSelector = selector;
}
void DebugBoxSceneNode::EnableDrawTriangles(bool enable) {
	drawTriangles = enable;
}
void DebugBoxSceneNode::render()
{
	video::IVideoDriver* driver = SceneManager->getVideoDriver();

	if (DebugDataVisible)
	{
		video::SMaterial m;
		m.Lighting = false;
		driver->setMaterial(m);
		driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);
		driver->draw3DBox(SceneManager::Instance()->GetPlayerBoundingBox(), video::SColor(255, 0, 255, 255));
	}

	// TriangleSelector 기반 triangle 출력
	if (drawTriangles && triangleSelector != nullptr) {
		core::matrix4 worldMat;
		worldMat.makeIdentity();
		DrawBoxTriangles(triangleSelector, worldMat, driver);
	}
}

CollisionBoxQueueSceneNode::CollisionBoxQueueSceneNode(scene::ISceneNode* parent, scene::ISceneManager* mgr, s32 id) : scene::ISceneNode(parent, mgr, id){
#ifdef _DEBUG
	 setDebugName("CollisionBoxQueueSceneNode");
#endif
	 setAutomaticCulling(scene::EAC_OFF);
	 q.Clear(_FILE_AND_LINE_);
 }
const core::aabbox3d<f32>& CollisionBoxQueueSceneNode::getBoundingBox() const {
	return SceneManager::Instance( )->GetPlayerBoundingBox( );
 }
void CollisionBoxQueueSceneNode::OnRegisterSceneNode() {
	 if (IsVisible) CInGame::Instance()->GetSceneManager()->registerNodeForRendering(this, scene::ESNRP_SOLID);
 }
void CollisionBoxQueueSceneNode::render() {
	 video::IVideoDriver* driver = SceneManager->getVideoDriver();
	 video::SMaterial m;
	 m.Lighting = false;
	 m.Thickness = 3.0f;
	 driver->setMaterial(m);
	 // 기존 transform을 유지하고, 월드 좌표계로 설정
	 driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);

	 RakNet::TimeMS now = RakNet::GetTimeMS();
	 int idx = 0;
	 while (idx < q.Size()) {
		 CollDebugState& item = q[idx];
		 for (int i = 0; i < item.outCount; ++i) {
			 driver->draw3DLine(item.tris[i].pointA, item.tris[i].pointB, irr::video::SColor(255, 0, 0, 255));
			 driver->draw3DLine(item.tris[i].pointB, item.tris[i].pointC, irr::video::SColor(255, 0, 0, 255));
			 driver->draw3DLine(item.tris[i].pointC, item.tris[i].pointA, irr::video::SColor(255, 0, 0, 255));
		 }
		 if (item.line != irr::core::line3d<irr::f32>()) driver->draw3DLine(item.line.start, item.line.end, item.color);

		 for (u32 i = 0; i < item.extraLines.size(); ++i) {
			 driver->draw3DLine(item.extraLines[i].start, item.extraLines[i].end, item.color);
		 }

		 if (now > item.timeStamp + item.drawTimeOut) q.RemoveAtIndex(idx); // 시간 초과된 항목 제거
		 else ++idx;
	 }
 }

void DrawBoxTriangles(irr::scene::ITriangleSelector* selector, const irr::core::matrix4& transform, irr::video::IVideoDriver* driver)
{
	const int maxTriangles = 512;
	irr::core::triangle3df tris[maxTriangles];
	s32 outCount = 0;

	selector->getTriangles(tris, maxTriangles, outCount, &transform);  // 월드 변환 적용!

	for (int i = 0; i < outCount; ++i) {
		driver->draw3DLine(tris[i].pointA, tris[i].pointB, irr::video::SColor(255, 255, 0, 0));
		driver->draw3DLine(tris[i].pointB, tris[i].pointC, irr::video::SColor(255, 255, 0, 0));
		driver->draw3DLine(tris[i].pointC, tris[i].pointA, irr::video::SColor(255, 255, 0, 0));
	}
}
void DrawDebugFrame(const core::aabbox3df& boundingBox, core::vector3df position, float rotationAroundYAxis, scene::ISceneManager* sm, RakNet::RakNetGUID g, RakNet::TimeMS drawTimeOut)
{
	if (CollisionManager::Instance()->collisionBoxQueue == nullptr) return;
	core::matrix4 transform;
	transform.setTranslation(position);

	core::matrix4 rotation;
	rotation.setRotationDegrees(core::vector3df(0, rotationAroundYAxis, 0));

	core::matrix4 scale;
	scale.setScale(core::vector3df(1, 1, 1));

	transform *= rotation;
	transform *= scale;

	irr::scene::ITriangleSelector* selector = CreateSelectorFromTransformedBox(boundingBox, transform, sm, g);
	DrawDebugFrame(selector, drawTimeOut, true);
}
void DrawDebugFrame(irr::scene::ITriangleSelector* selector, RakNet::TimeMS drawTimeOut, bool isDrop)
{
	if (CollisionManager::Instance()->collisionBoxQueue == nullptr) return;
	const int maxTriangles = 512;
	std::vector<irr::core::triangle3df> tris(maxTriangles);
	s32 outCount = 0;
	selector->getTriangles(tris.data(), maxTriangles, outCount, &core::IdentityMatrix);
	// 새로운 구조체 순서: timeStamp, tris, outCount, line, extraLines, drawTimeOut, color
	CollDebugState item = { RakNet::GetTimeMS(), tris, outCount, irr::core::line3d<irr::f32>(), {}, drawTimeOut, irr::video::SColor(255, 0, 255, 255) };
	CollisionManager::Instance()->collisionBoxQueue->GetQueue()->Push(item, _FILE_AND_LINE_);
	if (isDrop) selector->drop();
}
scene::ITriangleSelector* CreateSelectorFromTransformedBox( const core::aabbox3df& localBox, const core::matrix4& worldTransform, scene::ISceneManager* smgr, const RakNet::RakNetGUID& guid)
{
	// 로컬 박스의 꼭지점 8개 정의
	core::vector3df corners[8];
	localBox.getEdges(corners);

	core::vector3df transformedCorner;
	worldTransform.transformVect(transformedCorner, corners[0]);
	core::aabbox3df worldBox;
	worldBox.reset(transformedCorner);

	for (int i = 1; i < 8; ++i) {
		core::vector3df transformed;
		worldTransform.transformVect(transformed, corners[i]);
		worldBox.addInternalPoint(transformed);
	}

	// 임시 노드 (bounding box 전달용)
	class TempBoxNode : public scene::ISceneNode {
	public:
		core::aabbox3df box;
		TempBoxNode(const core::aabbox3df& b, scene::ISceneNode* parent, scene::ISceneManager* mgr)
			: scene::ISceneNode(parent, mgr), box(b) {
			setAutomaticCulling(scene::EAC_OFF);
		}
		virtual const core::aabbox3df& getBoundingBox() const override { return box; }
		virtual void render() override {}
		virtual void OnRegisterSceneNode() override {
			if (IsVisible)
				SceneManager->registerNodeForRendering(this);
		}
	};

	TempBoxNode* dummy = new TempBoxNode(worldBox, smgr->getRootSceneNode(), smgr);
	s32 id = static_cast<s32>(guid.g);
	dummy->setID(id); // RakNetGUID를 ID로 설정

	scene::ITriangleSelector* selector = smgr->createTriangleSelectorFromBoundingBox(dummy);
	dummy->remove(); // 노드 제거 (참조 카운트 감소)

	return selector; // drop()은 사용자가 책임
}

void CollisionManager::DrawDebugLine(core::line3d<f32> line, video::SColor color, RakNet::TimeMS drawTimeOut) {
	if (collisionBoxQueue == nullptr) return;
	CollDebugState item;
	item.timeStamp = RakNet::GetTimeMS();
	item.drawTimeOut = drawTimeOut;
	item.line = line;
	item.outCount = 0;
	item.color = color;
	collisionBoxQueue->GetQueue()->Push(item, _FILE_AND_LINE_);
}

void CollisionManager::DrawDebugCircle(core::vector3df center, f32 radius, core::vector3df normal, video::SColor color, RakNet::TimeMS drawTimeOut) {
	if (collisionBoxQueue == nullptr) return;
	CollDebugState item;
	item.timeStamp = RakNet::GetTimeMS();
	item.drawTimeOut = drawTimeOut;
	item.line = core::line3d<f32>(0, 0, 0, 0, 0, 0);
	item.outCount = 0;
	item.color = color;

	core::vector3df v(1, 0, 0);
	if (abs(normal.X) > 0.9f) v = core::vector3df(0, 1, 0);
	core::vector3df x = normal.crossProduct(v).normalize();
	core::vector3df y = normal.crossProduct(x).normalize();

	const int segments = 24;
	core::vector3df prev;
	for (int i = 0; i <= segments; ++i) {
		f32 angle = (f32)i / segments * 2.0f * core::PI;
		core::vector3df current = center + x * cos(angle) * radius + y * sin(angle) * radius;
		if (i > 0) item.extraLines.push_back(core::line3d<f32>(prev, current));
		prev = current;
	}
	collisionBoxQueue->GetQueue()->Push(item, _FILE_AND_LINE_);
}

void CollisionManager::DrawDebugEllipsoid(core::vector3df center, core::vector3df radius, video::SColor color, RakNet::TimeMS drawTimeOut) {
	if (collisionBoxQueue == nullptr) return;

	auto pushEllipse = [&](core::vector3df xDir, core::vector3df yDir, f32 xRadius, f32 yRadius) {
		CollDebugState item;
		item.timeStamp = RakNet::GetTimeMS();
		item.drawTimeOut = drawTimeOut;
		item.line = core::line3d<f32>(0, 0, 0, 0, 0, 0);
		item.outCount = 0;
		item.color = color;

		const int segments = 24;
		core::vector3df prev;
		for (int i = 0; i <= segments; ++i) {
			f32 angle = (f32)i / segments * 2.0f * core::PI;
			core::vector3df current = center + xDir * cos(angle) * xRadius + yDir * sin(angle) * yRadius;
			if (i > 0) item.extraLines.push_back(core::line3d<f32>(prev, current));
			prev = current;
		}
		collisionBoxQueue->GetQueue()->Push(item, _FILE_AND_LINE_);
	};

	pushEllipse(core::vector3df(1, 0, 0), core::vector3df(0, 1, 0), radius.X, radius.Y);
	pushEllipse(core::vector3df(1, 0, 0), core::vector3df(0, 0, 1), radius.X, radius.Z);
	pushEllipse(core::vector3df(0, 1, 0), core::vector3df(0, 0, 1), radius.Y, radius.Z);
}

void CollisionManager::DrawDebugArea(core::vector3df p1, core::vector3df p2, core::vector3df p3, core::vector3df p4, video::SColor color, RakNet::TimeMS drawTimeOut) {
	if (collisionBoxQueue == nullptr) return;
	CollDebugState item;
	item.timeStamp = RakNet::GetTimeMS();
	item.drawTimeOut = drawTimeOut;
	item.line = core::line3d<f32>(0, 0, 0, 0, 0, 0);
	item.outCount = 0;
	item.color = color;

	item.extraLines.push_back(core::line3d<f32>(p1, p2));
	item.extraLines.push_back(core::line3d<f32>(p2, p4));
	item.extraLines.push_back(core::line3d<f32>(p4, p3));
	item.extraLines.push_back(core::line3d<f32>(p3, p1));
	item.extraLines.push_back(core::line3d<f32>(p1, p4)); // Diagonal

	collisionBoxQueue->GetQueue()->Push(item, _FILE_AND_LINE_);
}

CollisionManager* CollisionManager::instance = nullptr;
CollisionManager* CollisionManager::Instance() {
	if (instance == nullptr) instance = new CollisionManager();
	return instance;
}
void CollisionManager::DestroyInstance() {
	if (instance) {
		delete instance;
		instance = nullptr;
	}
}
CollisionManager::CollisionManager( ) {
	if ( instance == nullptr ) instance = this;
	collisionBoxQueue = new CollisionBoxQueueSceneNode(CInGame::Instance( )->GetSceneManager( )->getRootSceneNode( ), CInGame::Instance( )->GetSceneManager( ));
}
CollisionManager::~CollisionManager() {
	if ( collisionBoxQueue ) {
		collisionBoxQueue->remove( );
		collisionBoxQueue = nullptr;
	}
}

void CollisionManager::BulletHitDetected(RakNet::RakNetGUID creatingSystemGUID, float ingoingTimeMS, GunType gunType, core::vector3df overrideStart, core::vector3df overrideDir) {
	if (NetworkManager::Instance()->IsServer() == false) return;

	unsigned int idx;
	scene::ISceneManager* sm = CInGame::Instance()->GetDevice()->getSceneManager();
	scene::ICameraSceneNode* camera = sm->getActiveCamera();

	// Time Warp
	core::vector3df start;
	core::vector3df end;
	core::line3d<irr::f32> line;
	core::triangle3df triangle;
	core::vector3df hitPoint;
	const scene::ISceneNode* hitNode;

	RakNet::TimeMS now = RakNet::GetTimeMS() - (NetworkManager::Instance()->GetPeer()->GetAveragePing(creatingSystemGUID) / 2) - INTERP_TIME_MS - (RakNet::TimeMS)ingoingTimeMS;
	bool wallHit = false;
	core::vector3df wallHitPoint;
	PlayerReplica* shooter = nullptr;
	
	GunTraceType traceType = GetTraceType(gunType);

	for (idx = 0; idx < NetworkManager::Instance()->GetPlayerList().Size(); ++idx)
	{
		auto* player = NetworkManager::Instance()->GetPlayerList()[idx];
		auto* q = player->fq;
		bool didTimeWarp = false;

		if (player->creatingSystemGUID == creatingSystemGUID) {
			shooter = player;
			// shooter->shootCnt++; // This should be handled elsewhere if it's a sub-step of a projectile

			if (overrideStart != core::vector3df(0, 0, 0)) {
				start = overrideStart;
				core::vector3df dir = overrideDir;
				dir.normalize();
				end = start + (dir * (traceType == GTT_PROJECTILE ? 100.0f : camera->getFarValue())); // Explosion radius or far value
				line.setLine(start, end);
				didTimeWarp = true;
			}
		}

		if (!didTimeWarp) {
			for (int frameIdx = 1; frameIdx < q->Size(); frameIdx++) {
				if ((*q)[frameIdx].timeStamp >= now) {
					const FrameState& f0 = (*q)[frameIdx - 1];
					const FrameState& f1 = (*q)[frameIdx];
					float alpha = float(now - f0.timeStamp) / float(f1.timeStamp - f0.timeStamp);

					if (player->creatingSystemGUID == creatingSystemGUID) {
						start = f0.shotPosition.getInterpolated(f1.shotPosition, alpha);
						core::vector3df interpolatedShotDir = f0.shotDirection.getInterpolated(f1.shotDirection, alpha);
						interpolatedShotDir.normalize();
						float traceRange = (traceType == GTT_KNIFE) ? 50.0f : camera->getFarValue();
						end = start + (interpolatedShotDir * traceRange);
						line.setLine(start, end);
						SceneManager::Instance()->shootFromOrigin(start, interpolatedShotDir, start, end, start, wallHit, wallHitPoint, player->gamePlatform);
					}
					else {
						for (int i = 0; i < 16; ++i)
							player->collisionTransform.pointer()[i] = (f0.collisionTransform.pointer()[i]) * (1.0f - alpha) + (f1.collisionTransform.pointer()[i]) * alpha;
					}
					didTimeWarp = true;
					break;
				}
			}
		}

		if (!didTimeWarp && q->Size() >= 1) {
			const FrameState& lastFrame = q->PeekTail();
			if (player->creatingSystemGUID == creatingSystemGUID) {
				start = lastFrame.shotPosition;
				core::vector3df dir = lastFrame.shotDirection;
				dir.normalize();
				float traceRange = (traceType == GTT_KNIFE) ? 50.0f : camera->getFarValue();
				end = start + dir * traceRange;
				line.setLine(start, end);
				SceneManager::Instance()->shootFromOrigin(lastFrame.shotPosition, lastFrame.shotDirection, start, end, lastFrame.shotPosition, wallHit, wallHitPoint, player->gamePlatform);
			}
			else {
				player->collisionTransform = lastFrame.collisionTransform;
			}
		}
	}

	// Draw debug line for the trace
	DrawDebugLine(line);

	PlayerReplica* holder = nullptr;
	for (idx = 0; idx < NetworkManager::Instance()->GetPlayerList().Size(); ++idx)
	{
		auto* player = NetworkManager::Instance()->GetPlayerList()[idx];
		if (player->creatingSystemGUID == creatingSystemGUID && traceType != GTT_PROJECTILE) {
			player->PlayAttackAnimation();
			continue; 
		}
		holder = player;

		scene::ITriangleSelector* selector = nullptr;
		if (holder->IsDead()) continue;
		if (holder->isBot == false) continue; 

		selector = CreateSelectorFromTransformedBox(SceneManager::Instance()->GetPlayerBoundingBox(), holder->collisionTransform, sm, holder->creatingSystemGUID);
		if (selector == nullptr) continue;

#ifdef __ANDROID__
		scene::SCollisionHit hitResult;
		bool hit = sm->getSceneCollisionManager()->getCollisionPoint(hitResult, line, selector);
		hitPoint = hitResult.Intersection;
		triangle = hitResult.Triangle;
		hitNode = hitResult.Node;
#else 
		bool hit = sm->getSceneCollisionManager()->getCollisionPoint(line, selector, hitPoint, triangle, hitNode);
#endif 

		selector->drop();

		if (hit && hitNode && hitNode->getID() == static_cast<s32>(holder->creatingSystemGUID.g))
		{
			if (wallHit && traceType != GTT_PROJECTILE) {
				float distToPlayer = line.start.getDistanceFrom(hitPoint);
				float distToWall = line.start.getDistanceFrom(wallHitPoint);
				if (distToWall < distToPlayer) continue;
			}
// ... (rest of the hit handling logic) ...

			if (holder->fq) holder->fq->Clear(_FILE_AND_LINE_);

			//Spawn Time
			if (holder->isBot) holder->deathTimeout = RakNet::GetTimeMS() + RandomInt(3000, 5000);
			else holder->deathTimeout = RakNet::GetTimeMS() + 3000;

			RakNet::RakString msg("%s Dead from : %s",
				holder->isBot ? "Bot" : "Player",
				NetworkManager::Instance()->GetPeer()->GetSystemAddressFromGuid(shooter->creatingSystemGUID).ToString(true));
			HUDManager::Instance()->PushMessage(msg);
			//OutputDebugStringA(msg.C_String());

			//점수 득점
			shooter->killCnt++;
			holder->deathCnt++;
			//목표 점수에 도달하면 게임 끝
			if (shooter->killCnt == CInGame::Instance()->winScore) {
				CInGame::Instance()->isGameEnd = true;
			}

#if QOS_SUPPORTED 
			//점수 득점 정보 기록
			//Agent 통계와 시간대 같이 맞추기 위해 CInGame::Instance()->get_nsecs() 사용
			HitInfo info = { NetworkManager::Instance()->GetPeer()->GetSystemAddressFromGuid(shooter->creatingSystemGUID).ToString(false), NetworkManager::Instance()->GetPeer()->GetSystemAddressFromGuid(holder->creatingSystemGUID).ToString(false), shooter->killCnt, get_nsecs() };
			if (info.holderAddr == SERVER_IP_LOCAL)
				info.holderAddr = SERVER_IP;
			else if (info.shooterAddr == SERVER_IP_LOCAL)
				info.shooterAddr = SERVER_IP;

			RakNet::RakString str("%s/%s/%d/%lu\n", info.shooterAddr, info.holderAddr, info.nowScore, info.timeStamp);
			statBufList[0].Push(RakNet::RakString(str), _FILE_AND_LINE_); //Log
#endif

			RakNet::BitStream bs;
			bs.Write((RakNet::MessageID)ID_GAME_MESSAGE_PLAYER_LIFE);
			bs.Write((shooter->creatingSystemGUID));       //Shooter 
			bs.Write(holder->IsDead());
			holder->wasDead = true;
			bs.Write(holder->creatingSystemGUID); //Holder
			bs.Write(shooter->playerName);        //Shooter Name
			bs.Write(holder->playerName);         //Holder Name

			if (holder->isBot && holder->creatingSystemGUID == NetworkManager::Instance()->GetPeer()->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS)) {
				SEvent botKeyEvent;
				botKeyEvent.EventType = EET_KEY_INPUT_EVENT;
				botKeyEvent.KeyInput.Key = KEY_KEY_W;
				botKeyEvent.KeyInput.PressedDown = false;
				InputController::Instance()->SetKeyDown(botKeyEvent.KeyInput.Key, botKeyEvent.KeyInput.PressedDown);
				if (CInGame::Instance()->GetDevice()->getSceneManager()->getActiveCamera()) {
					CInGame::Instance()->GetDevice()->getSceneManager()->getActiveCamera()->OnEvent(botKeyEvent);
				}
			}

			KillLog logEntry{ shooter->playerName + RakNet::RakString(" -> ") + holder->playerName + RakNet::RakString("\n"), RakNet::GetTimeMS() };
			HUDManager::Instance()->killLogMessages.Push(logEntry, _FILE_AND_LINE_); // Record the kill log message
			NetworkManager::Instance()->GetPeer()->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
			break; // 더 검사하지 않음
		}
	}
}