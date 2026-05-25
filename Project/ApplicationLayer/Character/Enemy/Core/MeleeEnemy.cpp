#define NOMINMAX
#include "MeleeEnemy.h"

#include <cmath>
#include <algorithm>
#include <cstdlib>
#include "Wireframe.h"
#include "CollisionTypeIdDef.h"
#include <fstream>
#include <json.hpp>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace Ken4lowEngine;

namespace
{
	constexpr float kEpsilon = 0.0001f;
	constexpr float kPi = 3.1415926535f;
	constexpr float kTwoPi = kPi * 2.0f;

	float LengthXZ(const Vector3& v)
	{
		return std::sqrt(v.x * v.x + v.z * v.z);
	}

	Vector3 NormalizeXZ(const Vector3& v)
	{
		const float len = LengthXZ(v);
		if (len < kEpsilon) { return { 0.0f, 0.0f, 0.0f }; }
		return { v.x / len, 0.0f, v.z / len };
	}

	float NormalizeAngleRad(float v)
	{
		while (v > kPi) { v -= kTwoPi; }
		while (v < -kPi) { v += kTwoPi; }
		return v;
	}

	float Clamp(float v, float minValue, float maxValue)
	{
		return std::max(minValue, std::min(maxValue, v));
	}


	float DistancePointToSegmentXZ(const K4E::Vector3& p, const K4E::Vector3& a, const K4E::Vector3& b)
	{
		const K4E::Vector3 ab{ b.x - a.x, 0.0f, b.z - a.z };
		const K4E::Vector3 ap{ p.x - a.x, 0.0f, p.z - a.z };
		const float denom = ab.x * ab.x + ab.z * ab.z;
		if (denom <= kEpsilon)
		{
			return LengthXZ(K4E::Vector3{ p.x - a.x, 0.0f, p.z - a.z });
		}
		const float t = std::clamp((ap.x * ab.x + ap.z * ab.z) / denom, 0.0f, 1.0f);
		const K4E::Vector3 closest{ a.x + ab.x * t, 0.0f, a.z + ab.z * t };
		return LengthXZ(K4E::Vector3{ p.x - closest.x, 0.0f, p.z - closest.z });
	}

}

void MeleeEnemy::Initialize()
{
	EnemyBase::Initialize();
	SetMaxHp(160);
	SetCenterPosition({ 0.0f, 2.0f, 10.0f });
	wander_.timer = 0.0f;
	currentBehaviorName_ = "Wander";
	animationState_.animState = AnimState::Idle;
	attackController_.Initialize();
	spawnPosition_ = GetCenterPosition();
	pathState_.lastStuckCheckPosition = spawnPosition_;
	collision_.lastSafePosition = spawnPosition_;
	animation_.visualYawOffset = 0.0f;
	animationState_.visualYawOffsetDeg = 0.0f;
	// 初期化時に近接敵の調整データをJSONから読み込む
	LoadTuningFromJson(tuningIo_.jsonPath, &tuningIo_.lastLoadResult);
}

void MeleeEnemy::Update(float deltaTime)
{
	const Vector3 beforePos = GetCenterPosition();
	attackController_.Update(*this, deltaTime);
	attackState_.lockTimer = std::max(0.0f, attackState_.lockTimer - deltaTime);
	// 連続ジャンプを防ぐため、追跡ジャンプのクールダウンタイマーを更新する
	jumpState_.cooldownTimer = std::max(0.0f, jumpState_.cooldownTimer - deltaTime);

	// 優先度の高い行動から順に評価して近接敵の行動を決定する
	EvaluateBehavior(deltaTime);
	EnemyBase::Update(deltaTime);

	collision_.usingWorldAABBCount = GetResolvedWorldAABBs() ? static_cast<int>(GetResolvedWorldAABBs()->size()) : 0;
	collision_.usingObstacleAABBCount = GetResolvedNavigationObstacleAABBs() ? static_cast<int>(GetResolvedNavigationObstacleAABBs()->size()) : 0;
	collision_.collisionManagerRegistered = true;
	collision_.lastCollisionCount = 0;
	collision_.pushedThisFrame = false;
	collision_.restoredToSafePosition = false;
	collision_.landedOnObstacleTop = false;
	collision_.lastObstacleTopLandingName = "None";
	// フレーム開始時に接触ジャンプ判定の呼び出し状態を初期化する
	contactJumpDebugState_.calledThisFrame = false;
	contactObstacleState_.hasContact = false;
	contactObstacleState_.climbable = false;
	contactObstacleState_.obstacleIndex = -1;
	contactObstacleState_.climbableByHeight = false;
	contactObstacleState_.rejectedByWidthDepth = false;
	contactObstacleState_.rejectedByAABBSize = false;
	contactObstacleState_.judgedByContactFace = false;
	contactObstacleState_.obstacleWidth = 0.0f;
	contactObstacleState_.obstacleDepth = 0.0f;
	contactObstacleState_.obstacleForwardThickness = 0.0f;
	contactObstacleState_.contactFaceDistance = 0.0f;
	contactObstacleState_.reason = "None";
	contactObstacleState_.possibleReason = "None";
	const auto* floorAabbs = floorAABBs_ ? floorAABBs_ : GetResolvedWorldAABBs();
	worldAABBs_ = floorAabbs;
	Vector3 afterPos = GetCenterPosition();
	Vector3 push = afterPos - (beforePos + GetVelocity() * deltaTime);
	// 押し出し補正の暴発でステージ外へ飛ばされるのを防ぐため、フレーム補正量を制限する
	const float pushLen = std::sqrt(push.x * push.x + push.y * push.y + push.z * push.z);
	const float horizontalPushLen = LengthXZ(push);
	if (pushLen > move_.maxResolvePushPerFrame || horizontalPushLen > move_.maxHorizontalPushPerFrame)
	{
		const float clampScale = std::min(move_.maxResolvePushPerFrame / std::max(pushLen, kEpsilon), move_.maxHorizontalPushPerFrame / std::max(horizontalPushLen, kEpsilon));
		afterPos = beforePos + (afterPos - beforePos) * std::max(0.0f, std::min(1.0f, clampScale));
		SetCenterPosition(afterPos);
	}
	collision_.lastResolvePush = afterPos - beforePos;
	// 障害物上面に着地できるフレームは横押し出しより上面接地を優先する
	const bool landedOnObstacleTop = TryLandOnObstacleTop(deltaTime);
	if (!landedOnObstacleTop && ResolveObstaclePenetrationXZ(deltaTime))
	{
		afterPos = GetCenterPosition();
		pathState_.lineBlocked = true;
		pathState_.blockedObstacleName = collision_.lastBlockedObstacleName;
		collision_.pushedThisFrame = true;
		// 実接触フォールバックとして、低い障害物なら乗り越えジャンプを試す
		TryJumpOverContactObstacle();
	}


	if (const auto* aabbs = GetResolvedWorldAABBs(); aabbs && !aabbs->empty())
	{
		for (const auto& aabb : *aabbs)
		{
			const float sizeX = aabb.max.x - aabb.min.x;
			const float sizeZ = aabb.max.z - aabb.min.z;
			const float sizeY = aabb.max.y - aabb.min.y;
			if (sizeX > 2.0f && sizeZ > 2.0f && sizeY <= 3.5f)
			{
				if (!collision_.hasStageBounds)
				{
					collision_.stageBoundsMin = aabb.min;
					collision_.stageBoundsMax = aabb.max;
					collision_.hasStageBounds = true;
				}
				else
				{
					collision_.stageBoundsMin.x = std::min(collision_.stageBoundsMin.x, aabb.min.x);
					collision_.stageBoundsMin.z = std::min(collision_.stageBoundsMin.z, aabb.min.z);
					collision_.stageBoundsMax.x = std::max(collision_.stageBoundsMax.x, aabb.max.x);
					collision_.stageBoundsMax.z = std::max(collision_.stageBoundsMax.z, aabb.max.z);
				}
			}
		}
	}

	collision_.isOutsideStage = collision_.hasStageBounds && !IsInsideStageBounds(GetCenterPosition());
	collision_.isOnFloor = grounded_;
	if (grounded_ && !collision_.isOutsideStage && !collision_.isOverlappingWallObstacle)
	{
		collision_.lastSafePosition = GetCenterPosition();
	}
	else if (collision_.isOutsideStage)
	{
		// 押し出し補正が大きすぎる場合は安全位置へ戻し、ステージ外へ弾き出されるのを防ぐ
		SetCenterPosition(collision_.lastSafePosition);
		collision_.restoredToSafePosition = true;
		SetVelocity({ 0.0f, GetVelocity().y, 0.0f });
	}

	UpdateVisualAnimation(deltaTime);
}

bool MeleeEnemy::TryLandOnObstacleTop(float deltaTime)
{
	(void)deltaTime;
	if (!move_.obstacleTopLandingEnabled) { return false; }
	const auto* obstacleAabbs = !climbableObstacleAABBs_.empty() ? &climbableObstacleAABBs_ : (wallObstacleAABBs_ ? wallObstacleAABBs_ : GetResolvedNavigationObstacleAABBs());
	if (!obstacleAabbs || obstacleAabbs->empty()) { return false; }
	const auto vel = GetVelocity();
	if (vel.y > 0.0f) { return false; }

	Vector3 pos = GetCenterPosition();
	const Vector3 half = { 1.0f, 2.0f, 1.0f };
	const float footY = pos.y - half.y;
	const float prevFootY = footY - vel.y * deltaTime;
	for (size_t i = 0; i < obstacleAabbs->size(); ++i)
	{
		const auto& o = (*obstacleAabbs)[i];
		const float overlapX = std::min(pos.x + half.x, o.max.x) - std::max(pos.x - half.x, o.min.x);
		const float overlapZ = std::min(pos.z + half.z, o.max.z) - std::max(pos.z - half.z, o.min.z);
		if (overlapX < move_.obstacleTopLandingMinHorizontalOverlap || overlapZ < move_.obstacleTopLandingMinHorizontalOverlap) { continue; }
		const float topY = o.max.y;
		if (topY - footY > move_.obstacleTopLandingMaxHeight) { continue; }
		const bool nearTop = std::abs(footY - topY) <= move_.obstacleTopLandingTolerance;
		const bool crossedFromAbove = prevFootY >= topY - kEpsilon;
		if (!nearTop || !crossedFromAbove) { continue; }
		// 上から接触した障害物だけを床として扱い、ジャンプ着地を成立させる
		pos.y = topY + half.y;
		SetCenterPosition(pos);
		auto nextVel = vel;
		nextVel.y = 0.0f;
		SetVelocity(nextVel);
		grounded_ = true;
		collision_.isOnFloor = true;
		collision_.landedOnObstacleTop = true;
		collision_.lastObstacleTopLandingName = "Obstacle[" + std::to_string(i) + "]";
		collision_.lastWallObstacleName = collision_.lastObstacleTopLandingName;
		return true;
	}
	return false;
}

bool MeleeEnemy::ResolveObstaclePenetrationXZ(float deltaTime)
{
	(void)deltaTime;
	const auto* obstacleAabbs = wallObstacleAABBs_ ? wallObstacleAABBs_ : GetResolvedNavigationObstacleAABBs();
	collision_.usingObstacleAABBCount = obstacleAabbs ? static_cast<int>(obstacleAabbs->size()) : 0;
	collision_.isCollidingWithStage = false;
	collision_.blockedByObstacle = false;
	collision_.lastStageCollisionType = "None";
	collision_.lastStageCollisionName = "None";
	collision_.lastBlockedObstacleName = "None";
	collision_.isOverlappingWallObstacle = false;
	collision_.lastWallResolvePush = { 0.0f, 0.0f, 0.0f };
	// 接触中障害物を毎フレーム初期化し、押し出し発生時に必ず再記録する
	contactObstacleState_ = ContactObstacleState{};
	if (!obstacleAabbs || obstacleAabbs->empty()) { return false; }

	Vector3 pos = GetCenterPosition();
	const Vector3 half = { 1.0f, 2.0f, 1.0f };
	const float maxPush = std::max(0.05f, move_.maxHorizontalPushPerFrame);
	bool resolved = false;
	for (size_t i = 0; i < obstacleAabbs->size(); ++i)
	{
		const auto& o = (*obstacleAabbs)[i];
		if (pos.y + half.y < o.min.y || pos.y - half.y > o.max.y) { continue; }
		const float overlapX = std::min(pos.x + half.x, o.max.x) - std::max(pos.x - half.x, o.min.x);
		const float overlapZ = std::min(pos.z + half.z, o.max.z) - std::max(pos.z - half.z, o.min.z);
		if (overlapX <= 0.0f || overlapZ <= 0.0f) { continue; }
		collision_.isCollidingWithStage = true;
		collision_.isOverlappingWallObstacle = true;
		collision_.blockedByObstacle = true;
		collision_.lastStageCollisionType = "Obstacle";
		collision_.lastStageCollisionName = "Obstacle[" + std::to_string(i) + "]";
		collision_.lastWallObstacleName = collision_.lastStageCollisionName;
		collision_.lastBlockedObstacleName = collision_.lastStageCollisionName;
		// 横押し出しが発生した障害物を接触候補として記録し、高さ最優先で乗り越え判定する
		const bool canLandOnTop = EvaluateContactObstacleClimbable(o, static_cast<int>(i));
		const bool isCandidateBetter = !contactObstacleState_.hasContact || (canLandOnTop && !contactObstacleState_.climbable) || (contactObstacleState_.obstacleIndex < 0);
		if (isCandidateBetter)
		{
			(void)EvaluateContactObstacleClimbable(o, static_cast<int>(i));
		}
		float pushX = 0.0f, pushZ = 0.0f;
		if (overlapX < overlapZ) { pushX = (pos.x < (o.min.x + o.max.x) * 0.5f ? -overlapX : overlapX); }
		else { pushZ = (pos.z < (o.min.z + o.max.z) * 0.5f ? -overlapZ : overlapZ); }
		pushX = std::clamp(pushX, -maxPush, maxPush);
		pushZ = std::clamp(pushZ, -maxPush, maxPush);
		pos.x += pushX;
		pos.z += pushZ;
		collision_.lastWallResolvePush.x += pushX;
		collision_.lastWallResolvePush.z += pushZ;
		// 乗り越え可能な接触障害物なら横速度を止める前に接触ジャンプを先に試す
		TryJumpOverContactObstacle();
		auto vel = GetVelocity();
		if ((pushX < 0.0f && vel.x > 0.0f) || (pushX > 0.0f && vel.x < 0.0f)) vel.x = 0.0f;
		if ((pushZ < 0.0f && vel.z > 0.0f) || (pushZ > 0.0f && vel.z < 0.0f)) vel.z = 0.0f;
		SetVelocity(vel);
		resolved = true;
	}
	if (resolved)
	{
		// Floorは接地用、Obstacle系は横押し出し用として分け、MeleeEnemyが壁へ埋まらないようにする
		SetCenterPosition(pos);
		navigator_.Reset();
		pathState_.lastRepathReason = "ObstaclePushResolve";
	}
	return resolved;
}

bool MeleeEnemy::EvaluateContactObstacleClimbable(const K4E::AABB& obstacle, int index)
{
	// 接触障害物の判定を1つに集約して毎フレーム同じ基準で評価する
	const Vector3 pos = GetCenterPosition();
	const Vector3 half = { 1.0f, 2.0f, 1.0f };
	const float footY = pos.y - half.y;
	const float topY = obstacle.max.y;
	const float heightFromFoot = topY - footY;
	const float overlapX = std::min(pos.x + half.x, obstacle.max.x) - std::max(pos.x - half.x, obstacle.min.x);
	const float overlapZ = std::min(pos.z + half.z, obstacle.max.z) - std::max(pos.z - half.z, obstacle.min.z);
	const bool xzOverlap = overlapX > 0.0f && overlapZ > 0.0f;
	Vector3 forward = LengthXZ(GetVelocity()) > kEpsilon ? NormalizeXZ(GetVelocity()) : NormalizeXZ(GetTargetPosition() - pos);
	if (LengthXZ(forward) <= kEpsilon) { forward = { 0.0f, 0.0f, 1.0f }; }
	const Vector3 obstacleCenter = (obstacle.min + obstacle.max) * 0.5f;
	const Vector3 toObstacle = NormalizeXZ(obstacleCenter - pos);
	const float facingDot = toObstacle.x * forward.x + toObstacle.z * forward.z;
	const bool facingObstacle = facingDot >= 0.1f;
	const float nearFaceDistanceX = std::min(std::abs((pos.x + half.x) - obstacle.min.x), std::abs((pos.x - half.x) - obstacle.max.x));
	const float nearFaceDistanceZ = std::min(std::abs((pos.z + half.z) - obstacle.min.z), std::abs((pos.z - half.z) - obstacle.max.z));
	const float nearFaceDistance = std::min(nearFaceDistanceX, nearFaceDistanceZ);
	const float width = obstacle.max.x - obstacle.min.x;
	const float depth = obstacle.max.z - obstacle.min.z;
	const bool topTooLow = heightFromFoot <= traversal_.minClimbHeight;
	const bool tooHigh = heightFromFoot > traversal_.maxClimbHeight;
	const bool isFloorLike = std::abs(obstacle.max.y - obstacle.min.y) <= 0.05f;
	const bool sideNearEnough = nearFaceDistance <= 1.75f;
	const bool climbable = traversal_.enabled && traversal_.allowJumpOverLowObstacles && xzOverlap && facingObstacle && sideNearEnough && !isFloorLike && !topTooLow && !tooHigh;
	contactObstacleState_.hasContact = true;
	contactObstacleState_.climbable = climbable;
	contactObstacleState_.climbableByHeight = !topTooLow && !tooHigh;
	contactObstacleState_.rejectedByWidthDepth = false;
	contactObstacleState_.rejectedByAABBSize = false;
	contactObstacleState_.judgedByContactFace = true;
	contactObstacleState_.obstacleIndex = index;
	contactObstacleState_.obstacleAABB = obstacle;
	contactObstacleState_.obstacleTopY = topY;
	contactObstacleState_.obstacleHeightFromFoot = heightFromFoot;
	contactObstacleState_.obstacleWidth = width;
	contactObstacleState_.obstacleDepth = depth;
	contactObstacleState_.obstacleForwardThickness = std::abs(forward.x) >= std::abs(forward.z) ? width : depth;
	contactObstacleState_.contactFaceDistance = nearFaceDistance;
	contactObstacleState_.enemyFootY = footY;
	contactObstacleState_.xzOverlapping = xzOverlap;
	contactObstacleState_.facingObstacle = facingObstacle;
	contactObstacleState_.reason = climbable ? "ContactClimbableObstacle" : "ContactNotClimbable";
	if (isFloorLike) { contactObstacleState_.notClimbableReason = "FloorOrStageAABB"; }
	else if (!xzOverlap) { contactObstacleState_.notClimbableReason = "NoXZOverlap"; }
	else if (!facingObstacle) { contactObstacleState_.notClimbableReason = "NotFacingObstacle"; }
	else if (!sideNearEnough) { contactObstacleState_.notClimbableReason = "NotSideContact"; }
	else if (topTooLow) { contactObstacleState_.notClimbableReason = "ObstacleTopTooLow"; }
	else if (tooHigh) { contactObstacleState_.notClimbableReason = "ObstacleTooHigh"; }
	else { contactObstacleState_.notClimbableReason = "None"; }
	return climbable;
}



void MeleeEnemy::OnCollisionEnter(K4E::Collider* other)
{
	EnemyBase::OnCollisionEnter(other);
	if (!other) { return; }
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kWorld))
	{
		collision_.isCollidingWithStage = true;
		collision_.isOverlappingWallObstacle = true;
		collision_.lastStageCollisionType = "World";
		collision_.lastStageCollisionName = "StageCollider";
		++collision_.lastCollisionCount;
	}
}

void MeleeEnemy::OnCollisionStay(K4E::Collider* other)
{
	OnCollisionEnter(other);
}
void MeleeEnemy::Draw()
{
	EnemyBase::Draw();
	const Vector3 origin = GetCenterPosition() + Vector3{ 0.0f, 1.0f, 0.0f };
	Wireframe::GetInstance()->DrawLine(origin, origin + animationState_.movementDirection * 1.8f, { 0.2f, 0.8f, 1.0f, 1.0f });
	Wireframe::GetInstance()->DrawLine(origin, origin + animationState_.targetDirection * 2.0f, { 1.0f, 1.0f, 0.2f, 1.0f });
	Wireframe::GetInstance()->DrawLine(origin, origin + animationState_.visualForward * 2.2f, { 1.0f, 0.4f, 1.0f, 1.0f });
	Wireframe::GetInstance()->DrawLine(origin, origin + animationState_.attackForward * 2.4f, { 1.0f, 0.2f, 0.2f, 1.0f });
	const auto& path = navigator_.GetCurrentPath();
	for (size_t i = 1; i < path.size(); ++i)
	{
		Wireframe::GetInstance()->DrawLine(path[i - 1] + Vector3{ 0.0f, 0.15f, 0.0f }, path[i] + Vector3{ 0.0f, 0.15f, 0.0f }, { 0.1f, 1.0f, 0.6f, 1.0f });
	}
	for (size_t i = 0; i < path.size(); ++i)
	{
		const Vector4 c = (static_cast<int>(i) == navigator_.GetCurrentPathIndex()) ? Vector4{ 1.0f, 0.3f, 0.1f, 1.0f } : Vector4{ 0.1f, 0.9f, 1.0f, 1.0f };
		Wireframe::GetInstance()->DrawSphere(path[i] + Vector3{ 0.0f, 0.2f, 0.0f }, (static_cast<int>(i) == navigator_.GetCurrentPathIndex()) ? 0.26f : 0.16f, c);
	}
	if (pathState_.lineBlocked)
	{
		Wireframe::GetInstance()->DrawLine(pathState_.blockedSegmentFrom + Vector3{ 0.0f, 0.2f, 0.0f }, pathState_.blockedSegmentTo + Vector3{ 0.0f, 0.2f, 0.0f }, { 1.0f, 0.15f, 0.1f, 1.0f });
	}
	if (HasTarget())
	{
		// ジャンプ追跡判定の可視化として、敵からターゲットへの線を専用色で表示する
		Wireframe::GetInstance()->DrawLine(origin + Vector3{ 0.0f, 0.2f, 0.0f }, GetTargetPosition() + Vector3{ 0.0f, 0.2f, 0.0f }, { 0.5f, 1.0f, 0.1f, 1.0f });
	}
	for (const auto& inflated : navigator_.GetInflatedObstacleAABBs())
	{
		Wireframe::GetInstance()->DrawAABB(inflated, { 1.0f, 0.7f, 0.2f, 0.35f });
	}
	for (size_t i = 0; i < climbableObstacleAABBs_.size(); ++i)
	{
		// 乗り越え候補AABBを色分けし、直線乗り越え選択中の障害物は強調する
		const bool isSelected = traversalState_.selectedClimbObstacleIndex == static_cast<int>(i);
		const Vector4 c = isSelected ? Vector4{ 0.0f, 1.0f, 0.2f, 0.75f } : Vector4{ 0.2f, 0.4f, 1.0f, 0.35f };
		Wireframe::GetInstance()->DrawAABB(climbableObstacleAABBs_[i], c);
	}
	for (const auto& blockedAabb : pathBlockingObstacleAABBs_)
	{
		// 回避対象AABBは別色で可視化し、A*障害物に残っていることを確認しやすくする
		Wireframe::GetInstance()->DrawAABB(blockedAabb, { 1.0f, 0.2f, 0.2f, 0.28f });
	}
	if (contactObstacleState_.hasContact)
	{
		// 実際に接触した障害物を緑/赤で強調表示し、接触フォールバックの対象判定を確認できるようにする
		const Vector4 c = contactObstacleState_.climbable ? Vector4{ 0.1f, 1.0f, 0.2f, 0.9f } : Vector4{ 1.0f, 0.2f, 0.2f, 0.9f };
		Wireframe::GetInstance()->DrawAABB(contactObstacleState_.obstacleAABB, c);
		if (contactObstacleState_.climbable)
		{
			Wireframe::GetInstance()->DrawAABB(contactObstacleState_.obstacleAABB.min - Vector3{ 0.03f, 0.03f, 0.03f }, contactObstacleState_.obstacleAABB.max + Vector3{ 0.03f, 0.03f, 0.03f }, { 0.6f, 1.0f, 0.6f, 0.95f });
		}
		const Vector3 foot = GetCenterPosition() + Vector3{ 0.0f, -2.0f, 0.0f };
		const Vector3 top = Vector3{ foot.x, contactObstacleState_.obstacleTopY, foot.z };
		Wireframe::GetInstance()->DrawLine(foot, top, { 1.0f, 1.0f, 0.2f, 0.9f });
	}
	for (const auto& blocked : navigator_.GetTemporaryBlockedAreas())
	{
		Wireframe::GetInstance()->DrawSphere(blocked.center + Vector3{ 0.0f, 0.2f, 0.0f }, blocked.radius, { 1.0f, 0.2f, 0.8f, 0.35f });
	}
}

void MeleeEnemy::DrawImGui()
{
#ifdef USE_IMGUI
	if (ImGui::Begin("MeleeEnemy Debug"))
	{
		EnemyBase::DrawImGui();
		if (ImGui::CollapsingHeader("データ保存/読み込み", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::Button("読み込み")) { LoadTuningFromJson(tuningIo_.jsonPath, &tuningIo_.lastLoadResult); }
			ImGui::SameLine();
			if (ImGui::Button("保存")) { SaveTuningToJson(tuningIo_.jsonPath, &tuningIo_.lastSaveResult); }
			ImGui::SameLine();
			if (ImGui::Button("デフォルトに戻す")) { ResetTuningToDefault(); tuningIo_.lastLoadResult = "デフォルト値へ復帰"; }
			ImGui::Text("保存先: %s", tuningIo_.jsonPath.string().c_str());
			ImGui::Text("読み込み結果: %s", tuningIo_.lastLoadResult.c_str());
			ImGui::Text("保存結果: %s", tuningIo_.lastSaveResult.c_str());
		}
		if (ImGui::CollapsingHeader("検知・攻撃距離", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::SliderFloat("検知範囲", &detection_.detectRange, 1.0f, 50.0f);
			ImGui::SliderFloat("近接攻撃距離", &detection_.meleeAttackRange, 0.5f, 10.0f);
			ImGui::SliderFloat("停止距離", &detection_.stopDistance, 0.5f, 6.0f);
			ImGui::SliderFloat("攻撃開始距離", &detection_.attackStartRange, 0.5f, 8.0f);
			ImGui::SliderFloat("追跡再開距離", &detection_.resumeChaseDistance, 0.5f, 10.0f);
			ImGui::SliderFloat("OneTwo前進最小距離", &detection_.minOneTwoForwardDistance, 0.1f, 5.0f);
		}
		if (ImGui::CollapsingHeader("移動", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::SliderFloat("移動速度", &move_.moveSpeed, 0.1f, 10.0f);
			ImGui::SliderFloat("回転速度", &move_.rotateSpeed, 0.1f, 20.0f);
			ImGui::SliderFloat("最大押し戻し量", &move_.maxResolvePushPerFrame, 0.05f, 2.0f);
			ImGui::SliderFloat("水平押し戻し量", &move_.maxHorizontalPushPerFrame, 0.05f, 2.0f);
		}
		if (ImGui::CollapsingHeader("ジャンプ", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Checkbox("ジャンプを使う", &jump_.enabled);
			ImGui::SliderFloat("ジャンプ力", &jump_.baseVelocity, 2.0f, 18.0f);
			ImGui::SliderFloat("ジャンプクールダウン", &jump_.cooldown, 0.0f, 3.0f);
			// 接触ジャンプの強制実行でジャンプ力そのものの効きだけを確認できるようにする
			if (ImGui::Button("接触ジャンプを強制"))
			{
				Vector3 forcedVel = GetVelocity();
				forcedVel.y = jump_.baseVelocity;
				SetVelocity(forcedVel);
				jumpState_.appliedVelocity = forcedVel.y;
				jumpState_.lastReason = "ForcedContactJump";
			}
			ImGui::Text("クールダウン残り: %.2f", jumpState_.cooldownTimer);
			ImGui::Text("最後のジャンプ理由: %s", jumpState_.lastReason.c_str());
			ImGui::Text("適用ジャンプ力: %.2f", jumpState_.appliedVelocity);
			ImGui::Text("高さ依存ジャンプ: OFF (通常近接雑魚固定)");
		}
		if (ImGui::CollapsingHeader("経路探索", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Checkbox("経路探索を使う", &pathSettings_.enabled);
			ImGui::SliderFloat("再探索間隔", &pathSettings_.repathInterval, 0.05f, 2.0f);
			ImGui::SliderFloat("到達判定距離", &pathSettings_.waypointReachDistance, 0.5f, 1.5f);
			ImGui::SliderFloat("グリッドサイズ", &pathSettings_.gridSize, 0.5f, 2.0f);
			ImGui::SliderFloat("探索半径", &pathSettings_.searchRadius, 6.0f, 80.0f);
			ImGui::SliderFloat("障害物拡張半径", &pathSettings_.obstacleExpandRadius, 0.7f, 1.6f);
			ImGui::SliderFloat("一時ブロック時間", &pathSettings_.temporaryBlockDuration, 0.3f, 4.0f);
			ImGui::SliderFloat("一時ブロック半径", &pathSettings_.temporaryBlockRadius, 0.4f, 2.2f);
			ImGui::Checkbox("角抜け無効", &pathSettings_.cornerCuttingDisabled);
			const int sourceObstacleCount = wallObstacleAABBs_ ? static_cast<int>(wallObstacleAABBs_->size()) : (GetResolvedNavigationObstacleAABBs() ? static_cast<int>(GetResolvedNavigationObstacleAABBs()->size()) : 0);
			ImGui::Text("navigatorに渡している障害物数: %d", static_cast<int>(pathBlockingObstacleAABBs_.size()));
			ImGui::Text("元の障害物数: %d", sourceObstacleCount);
			ImGui::Text("乗り越え除外後の障害物数: %d", static_cast<int>(pathBlockingObstacleAABBs_.size()));
		}
		
		if (ImGui::CollapsingHeader("乗り越え", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Checkbox("乗り越えを使う", &traversal_.enabled);
			ImGui::Checkbox("直線乗り越えを優先", &traversal_.preferDirectClimb);
			ImGui::Checkbox("低障害物ジャンプを許可", &traversal_.allowJumpOverLowObstacles);
			ImGui::SliderFloat("最大乗り越え高さ", &traversal_.maxClimbHeight, 0.2f, 5.0f);
			ImGui::SliderFloat("最小乗り越え高さ", &traversal_.minClimbHeight, 0.0f, 1.0f);
			ImGui::SliderFloat("最大乗り越え幅", &traversal_.maxClimbObstacleWidth, 0.2f, 8.0f);
			ImGui::SliderFloat("最大乗り越え奥行き", &traversal_.maxClimbObstacleDepth, 0.2f, 8.0f);
			ImGui::SliderFloat("直線乗り越え最大距離", &traversal_.directClimbDistanceMax, 1.0f, 20.0f);
			ImGui::SliderFloat("乗り越え開始距離", &traversal_.climbJumpTriggerDistance, 0.3f, 6.0f);
			ImGui::SliderFloat("直線判定幅", &traversal_.directLineWidth, 0.2f, 4.0f);
			ImGui::Text("乗り越え可能障害物数: %d", traversalState_.climbableObstacleCount);
			ImGui::Text("回避対象障害物数: %d", traversalState_.blockingObstacleCount);
			ImGui::Text("近くに乗り越え可能障害物あり: %s", traversalState_.nearClimbableObstacle ? "はい" : "いいえ");
			ImGui::Text("直線乗り越え候補あり: %s", traversalState_.directClimbCandidateFound ? "はい" : "いいえ");
			ImGui::Text("最後の乗り越え理由: %s", traversalState_.lastReason.c_str());
			ImGui::Text("選択中の乗り越え障害物Index: %d", traversalState_.selectedClimbObstacleIndex);
			ImGui::Text("選択中障害物の高さ: %.2f", traversalState_.selectedObstacleHeight);
			ImGui::Text("選択中障害物の判定理由: %s", traversalState_.selectedObstacleJudgeReason.c_str());
			ImGui::Text("climb不可理由: %s", traversalState_.selectedObstacleRejectReason.c_str());
			ImGui::Text("obstacleWidth: %.2f obstacleDepth: %.2f", traversalState_.selectedObstacleWidth, traversalState_.selectedObstacleDepth);
			ImGui::Text("enemyFootY: %.2f obstacleTopY: %.2f", traversalState_.selectedEnemyFootY, traversalState_.selectedObstacleTopY);
			ImGui::Text("接触中の障害物あり: %s", contactObstacleState_.hasContact ? "はい" : "いいえ");
			ImGui::Text("接触障害物は乗り越え可能: %s", contactObstacleState_.climbable ? "はい" : "いいえ");
			ImGui::Text("接触障害物は高さ的に乗れる: %s", contactObstacleState_.climbableByHeight ? "はい" : "いいえ");
			ImGui::Text("接触障害物は幅/奥行きで除外された: %s", contactObstacleState_.rejectedByWidthDepth ? "はい" : "いいえ");
			ImGui::Text("AABB全体サイズで除外したか: %s", contactObstacleState_.rejectedByAABBSize ? "はい" : "いいえ");
			ImGui::Text("接触面基準で判定したか: %s", contactObstacleState_.judgedByContactFace ? "はい" : "いいえ");
			ImGui::Text("最終的な乗り越え可否: %s", contactObstacleState_.climbable ? "可能" : "不可");
			ImGui::Text("接触ジャンプ関数が呼ばれたか: %s", contactJumpDebugState_.calledThisFrame ? "はい(このフレーム)" : (contactJumpDebugState_.everCalled ? "過去に呼ばれた" : "いいえ"));
			ImGui::Text("最後にジャンプしなかった理由: %s", contactJumpDebugState_.lastReason.c_str());
			ImGui::Text("grounded: %s", grounded_ ? "true" : "false");
			ImGui::Text("isOverlappingWallObstacle: %s", collision_.isOverlappingWallObstacle ? "true" : "false");
			ImGui::Text("velocity.y: %.2f", GetVelocity().y);
			ImGui::Text("接触障害物Index: %d", contactObstacleState_.obstacleIndex);
			ImGui::Text("接触障害物の高さ: %.2f", contactObstacleState_.obstacleHeightFromFoot);
			ImGui::Text("接触障害物の幅: %.2f", contactObstacleState_.obstacleWidth);
			ImGui::Text("接触障害物の奥行き: %.2f", contactObstacleState_.obstacleDepth);
			ImGui::Text("接触方向厚み: %.2f", contactObstacleState_.obstacleForwardThickness);
			ImGui::Text("接触面までの距離: %.2f", contactObstacleState_.contactFaceDistance);
			ImGui::Text("足元Y: %.2f", contactObstacleState_.enemyFootY);
			ImGui::Text("接触障害物の上面Y: %.2f", contactObstacleState_.obstacleTopY);
			ImGui::Text("障害物上面Y: %.2f", contactObstacleState_.obstacleTopY);
			ImGui::Text("足元から見た上面高さ: %.2f", contactObstacleState_.obstacleHeightFromFoot);
			ImGui::Text("最大乗り越え高さ: %.2f", traversal_.maxClimbHeight);
			ImGui::Text("ジャンプクールダウン残り: %.2f", jumpState_.cooldownTimer);
			ImGui::Text("適用予定ジャンプ力: %.2f", contactJumpDebugState_.plannedJumpVelocity);
			ImGui::Text("接触障害物の判定理由: %s", contactObstacleState_.reason.c_str());
			ImGui::Text("乗り越え不可理由: %s", contactObstacleState_.notClimbableReason.c_str());
			ImGui::Text("最後の可能理由: %s", contactObstacleState_.possibleReason.c_str());
			ImGui::Text("最後のジャンプ理由: %s", jumpState_.lastReason.c_str());
			ImGui::Text("最後の乗り越え理由: %s", traversalState_.lastReason.c_str());
		}
if (ImGui::CollapsingHeader("スタック", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::SliderFloat("判定時間", &stuckSettings_.checkTime, 0.1f, 3.0f);
			ImGui::SliderFloat("判定距離", &stuckSettings_.distance, 0.01f, 2.0f);
			ImGui::SliderFloat("移動閾値", &stuckSettings_.moveThreshold, 0.03f, 1.2f);
		}
		if (ImGui::CollapsingHeader("攻撃パターン", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::SliderFloat("攻撃ロック時間", &attackSettings_.lockTime, 0.0f, 1.0f);
			int attackSelect = static_cast<int>(attackSettings_.selectedAttackType);
			const char* items[] = { "ひっかき", "ワンツー" };
			if (ImGui::Combo("選択攻撃", &attackSelect, items, IM_ARRAYSIZE(items))) { attackSettings_.selectedAttackType = static_cast<MeleeAttackType>(attackSelect); }
			if (MeleeAttackPattern* scratch = attackController_.FindPattern(MeleeAttackType::Scratch)) { MeleeAttackStep& st = scratch->steps[0]; ImGui::SliderInt("ひっかき ダメージ", &st.damage, 1, 50); ImGui::SliderFloat("ひっかき 射程", &st.range, 0.5f, 6.0f); ImGui::SliderFloat("ひっかき 半径", &st.radius, 0.1f, 3.0f); ImGui::SliderFloat("ひっかき 開始", &st.startTime, 0.01f, 1.5f); ImGui::SliderFloat("ひっかき 有効", &st.activeTime, 0.01f, 1.0f); ImGui::SliderFloat("ひっかき 硬直", &scratch->recoveryTime, 0.01f, 2.0f); ImGui::SliderFloat("ひっかき CT", &scratch->cooldown, 0.01f, 3.0f); }
			if (MeleeAttackPattern* oneTwo = attackController_.FindPattern(MeleeAttackType::OneTwo)) { MeleeAttackStep& l = oneTwo->steps[0]; MeleeAttackStep& r = oneTwo->steps[1]; ImGui::SliderInt("ワンツー左 ダメージ", &l.damage, 1, 50); ImGui::SliderFloat("ワンツー左 射程", &l.range, 0.5f, 6.0f); ImGui::SliderFloat("ワンツー左 半径", &l.radius, 0.1f, 3.0f); ImGui::SliderFloat("ワンツー左 開始", &l.startTime, 0.01f, 1.5f); ImGui::SliderFloat("ワンツー左 有効", &l.activeTime, 0.01f, 1.0f); ImGui::SliderInt("ワンツー右 ダメージ", &r.damage, 1, 50); ImGui::SliderFloat("ワンツー右 射程", &r.range, 0.5f, 6.0f); ImGui::SliderFloat("ワンツー右 半径", &r.radius, 0.1f, 3.0f); ImGui::SliderFloat("ワンツー右 開始", &r.startTime, 0.01f, 2.0f); ImGui::SliderFloat("ワンツー右 有効", &r.activeTime, 0.01f, 1.0f); ImGui::SliderFloat("ワンツー 前進速度", &oneTwo->forwardMoveSpeed, 0.0f, 5.0f); ImGui::SliderFloat("ワンツー 前進時間", &oneTwo->forwardMoveDuration, 0.0f, 2.0f); ImGui::SliderFloat("ワンツー 硬直", &oneTwo->recoveryTime, 0.01f, 2.0f); ImGui::SliderFloat("ワンツー CT", &oneTwo->cooldown, 0.01f, 3.0f); }
		}
		if (ImGui::CollapsingHeader("アニメーション", ImGuiTreeNodeFlags_DefaultOpen)) { ImGui::SliderFloat("歩行速度", &animation_.walkAnimSpeed, 1.0f, 18.0f); ImGui::SliderFloat("腕振り", &animation_.walkArmSwing, 0.0f, 1.5f); ImGui::SliderFloat("脚振り", &animation_.walkLegSwing, 0.0f, 1.5f); ImGui::SliderFloat("攻撃腕振り", &animation_.attackArmSwing, 0.0f, 2.0f); ImGui::SliderFloat("攻撃復帰速度", &animation_.attackReturnSpeed, 1.0f, 24.0f); ImGui::SliderFloat("攻撃体傾き", &animation_.attackBodyLean, 0.0f, 0.4f); }
		if (ImGui::CollapsingHeader("頭向き", ImGuiTreeNodeFlags_DefaultOpen)) { ImGui::Checkbox("頭をターゲットへ向ける", &headLookSettings_.enabled); ImGui::SliderFloat("ヨー制限", &headLookSettings_.yawLimitDeg, 10.0f, 120.0f); ImGui::SliderFloat("ピッチ最小", &headLookSettings_.pitchMinDeg, -80.0f, 0.0f); ImGui::SliderFloat("ピッチ最大", &headLookSettings_.pitchMaxDeg, 0.0f, 80.0f); ImGui::SliderFloat("補間速度", &headLookSettings_.lerpSpeed, 1.0f, 30.0f); }
		if (ImGui::CollapsingHeader("状態表示", ImGuiTreeNodeFlags_DefaultOpen)) { ImGui::Text("現在行動: %s", currentBehaviorName_); ImGui::Text("攻撃中: %s", attackController_.IsAttacking() ? "はい" : "いいえ"); ImGui::Text("ターゲット距離: %.2f", GetDistanceToTarget()); }
	}
	ImGui::End();
#endif
}

bool MeleeEnemy::HasTarget() const { return target_ != nullptr; }
bool MeleeEnemy::IsDeadCondition() const { return IsDead(); }
float MeleeEnemy::GetDistanceToTarget() const
{
	if (!target_) { return 9999.0f; }
	const Vector3 delta = target_->GetCenterPosition() - GetCenterPosition();
	return LengthXZ(delta);
}

Vector3 MeleeEnemy::GetTargetPosition() const
{
	if (!target_) { return GetCenterPosition(); }
	return target_->GetCenterPosition();
}

Vector3 MeleeEnemy::GetTargetPositionForAttack() const
{
	return GetTargetPosition();
}

bool MeleeEnemy::IsTargetInDetectRange() const { return GetDistanceToTarget() <= detection_.detectRange; }
bool MeleeEnemy::IsTargetInMeleeRange() const { return GetDistanceToTarget() <= detection_.meleeAttackRange; }
bool MeleeEnemy::IsTargetInAttackStartRange() const { return GetDistanceToTarget() <= detection_.attackStartRange; }
bool MeleeEnemy::IsTargetInAttackHoldRange() const { return GetDistanceToTarget() <= detection_.resumeChaseDistance; }
bool MeleeEnemy::IsAttackCooldownReady() const { return attackController_.CanStartAttack(); }
bool MeleeEnemy::IsMoveResumeDistanceReached() const { return GetDistanceToTarget() >= detection_.resumeChaseDistance; }

void MeleeEnemy::FaceToTarget(float deltaTime)
{
	if (!target_) { return; }
	animationState_.targetDirection = NormalizeXZ(GetTargetPosition() - GetCenterPosition());
	ApplyVisualYawFromDirection(animationState_.targetDirection, deltaTime);
}

void MeleeEnemy::FaceToMoveDirection(float deltaTime)
{
	const Vector3 moveDir = NormalizeXZ(GetVelocity());
	animationState_.movementDirection = moveDir;
	ApplyVisualYawFromDirection(moveDir, deltaTime);
}

void MeleeEnemy::ApplyVisualYawFromDirection(const Vector3& direction, float deltaTime)
{
	if (LengthXZ(direction) <= kEpsilon) { return; }
	animationState_.rawYaw = std::atan2(-direction.x, direction.z);
	const float targetVisualYaw = animationState_.rawYaw;
	float currentYaw = NormalizeAngleRad(orientation_.y);
	const float maxStep = move_.rotateSpeed * std::max(deltaTime, 0.0f);
	float deltaYaw = NormalizeAngleRad(targetVisualYaw - currentYaw);
	if (std::abs(deltaYaw) > (kPi - 0.001f))
	{
		deltaYaw = (deltaYaw >= 0.0f) ? (kPi - 0.001f) : (-kPi + 0.001f);
	}
	// Yaw差分を-π～+πに正規化し、左右ターゲット移動時の逆回転を防ぐ
	deltaYaw = std::clamp(deltaYaw, -maxStep, maxStep);
	currentYaw = NormalizeAngleRad(currentYaw + deltaYaw);
	animationState_.debugCurrentYaw = currentYaw;
	animationState_.debugTargetYaw = targetVisualYaw;
	animationState_.debugDeltaYaw = targetVisualYaw - orientation_.y;
	animationState_.debugNormalizedDeltaYaw = NormalizeAngleRad(targetVisualYaw - orientation_.y);
	animationState_.finalVisualYaw = currentYaw;
	animationState_.facingDirection = { -std::sin(animationState_.rawYaw), 0.0f, std::cos(animationState_.rawYaw) };
	animationState_.visualForward = NormalizeXZ({ -std::sin(animationState_.finalVisualYaw), 0.0f, std::cos(animationState_.finalVisualYaw) });
	animationState_.attackForward = animationState_.visualForward;
	// 最終的な人型パーツのYawへ正面補正を加え、移動方向と見た目の向きを一致させる
	SetOrientation({ 0.0f, currentYaw, 0.0f });
}

void MeleeEnemy::DeadAction()
{
	StopMove();
	animationState_.animState = AnimState::Dead;
	currentBehaviorName_ = "DeadAction";
}

void MeleeEnemy::MeleeAttackAction()
{
	FaceToTarget(1.0f / 60.0f);
	StopMove();
	if (!attackController_.IsAttacking() && attackController_.CanStartAttack())
	{
		// 攻撃パターンをデータとして扱い、ScratchとOneTwoを同じ制御経路で実行する
		attackController_.StartAttack(attackSettings_.selectedAttackType);
		attackState_.lockTimer = attackSettings_.lockTime;
		animationState_.animState = (attackSettings_.selectedAttackType == MeleeAttackType::OneTwo) ? AnimState::OneTwo : AnimState::Scratch;
	}
	currentBehaviorName_ = (attackSettings_.selectedAttackType == MeleeAttackType::OneTwo) ? "OneTwoAttack" : "ScratchAttack";
}

void MeleeEnemy::CombatIdleAction()
{
	// 攻撃範囲内では追跡を止め、AttackとChaseの細かい切り替わりによる震えを防ぐ
	StopMove();
	FaceToTarget(1.0f / 60.0f);
	animationState_.animState = AnimState::Idle;
	currentBehaviorName_ = "CombatIdle";
}

void MeleeEnemy::ChaseTargetAction()
{
	UpdateStuckState(1.0f / 60.0f);
	UpdateTraversalObstacleClassification();
	if (TryDirectClimbOverObstacleToTarget(1.0f / 60.0f))
	{
		FaceToMoveDirection(1.0f / 60.0f);
		animationState_.animState = AnimState::Walk;
		currentBehaviorName_ = "DirectClimbApproach";
		return;
	}
	TryJumpForTraversal(1.0f / 60.0f);
	const float distance = GetDistanceToTarget();
	if (distance <= detection_.stopDistance)
	{
		StopMove();
		FaceToTarget(1.0f / 60.0f);
		animationState_.animState = AnimState::Idle;
		currentBehaviorName_ = "ChaseStopNearTarget";
		return;
	}
	if (pathSettings_.enabled && !attackController_.IsAttacking() && distance > detection_.attackStartRange && MoveAlongPath(1.0f / 60.0f))
	{
		FaceToMoveDirection(1.0f / 60.0f);
		animationState_.animState = AnimState::Walk;
		currentBehaviorName_ = "ChasePathMove";
		return;
	}
	if (pathSettings_.enabled)
	{
		pathState_.failedWaitTimer = std::max(pathState_.failedWaitTimer, pathSettings_.repathInterval);
		StopMove();
		currentBehaviorName_ = "PathFailedWait";
		return;
	}
	const Vector3 moveDir = NormalizeXZ(GetTargetPosition() - GetCenterPosition());
	if (LengthXZ(moveDir) <= kEpsilon)
	{
		StopMove();
		currentBehaviorName_ = "ChaseNoMove";
		return;
	}
	SetVelocity({ moveDir.x * move_.moveSpeed, GetVelocity().y, moveDir.z * move_.moveSpeed });
	FaceToMoveDirection(1.0f / 60.0f);
	animationState_.animState = AnimState::Walk;
	currentBehaviorName_ = "ChaseTargetAction";
}

bool MeleeEnemy::MoveAlongPath(float deltaTime)
{
	EnemyAStarNavigator::Settings s = navigator_.GetSettings();
	s.cellSize = pathSettings_.gridSize;
	s.agentRadius = pathSettings_.obstacleExpandRadius + pathSettings_.stuckRepathExpandBonus;
	s.searchRangeCells = static_cast<int>(pathSettings_.searchRadius);
	s.repathIntervalSec = pathSettings_.repathInterval;
	s.waypointReachDistance = pathSettings_.waypointReachDistance;
	s.disableCornerCutting = pathSettings_.cornerCuttingDisabled;
	navigator_.SetSettings(s);
	// 敵半径ぶん障害物を膨張して経路探索し、見た目上通れない隙間へ進まないようにする
	// 乗り越え可能な障害物を除外し、回避対象だけをA*へ渡す
	UpdateTraversalObstacleClassification();
	navigator_.SetWorldAABBs(&pathBlockingObstacleAABBs_);
	pathState_.lastRepathTimer += deltaTime;
	navigator_.TickTemporaryBlocks(deltaTime);
	const Vector3 targetPos = GetTargetPosition();
	pathState_.targetMovedDistanceForRepath = LengthXZ(targetPos - pathState_.lastPathTargetPos);
	if (pathState_.targetMovedDistanceForRepath >= pathSettings_.targetRepathThreshold || stuck_.isStuck)
	{
		navigator_.Reset();
		pathState_.lastRepathReason = stuck_.isStuck ? "StuckForceRepath" : "TargetMoved";
	}
	// 障害物を考慮した経路を使い、MeleeEnemyが直線移動で引っかからないようにする
	if (navigator_.GetNextWaypoint(GetCenterPosition(), targetPos, GetCenterPosition().y, deltaTime, pathState_.currentWaypoint))
	{
		pathState_.found = true;
		pathState_.failureReason = "None";
		pathState_.lastRepathReason = "Periodic";
		pathState_.lastPathTargetPos = targetPos;
		pathState_.retryTimer = 0.0f;
		pathState_.lineBlocked = false;
		pathState_.blockedWaypointIndex = navigator_.GetCurrentPathIndex();
		pathState_.blockedObstacleName = "None";
		int blockedIdx = -1;
		if (navigator_.IsSegmentBlockedByObstacle(GetCenterPosition(), pathState_.currentWaypoint, GetCenterPosition().y, &blockedIdx))
		{
			pathState_.lineBlocked = true;
			pathState_.blockedWaypointIndex = navigator_.GetCurrentPathIndex();
			pathState_.blockedObstacleName = (blockedIdx >= 0) ? ("Obstacle[" + std::to_string(blockedIdx) + "]") : "Unknown";
			pathState_.blockedSegmentFrom = GetCenterPosition();
			pathState_.blockedSegmentTo = pathState_.currentWaypoint;
			navigator_.Reset();
			navigator_.AddTemporaryBlockedArea(pathState_.currentWaypoint, pathSettings_.temporaryBlockRadius, pathSettings_.temporaryBlockDuration, "WaypointSegmentBlocked");
			pathState_.lastRepathReason = "WaypointSegmentBlocked";
			StopMove();
			return false;
		}
		const Vector3 dir = NormalizeXZ(pathState_.currentWaypoint - GetCenterPosition());
		SetVelocity({ dir.x * move_.moveSpeed, GetVelocity().y, dir.z * move_.moveSpeed });
		// 経路追跡中はターゲット段差ジャンプと障害物乗り越えジャンプを両方判定する
		TryJumpForTraversal(deltaTime);
		return true;
	}
	pathState_.found = false;
	pathState_.failureReason = "PathNotFound";
	pathState_.retryTimer += deltaTime;
	pathState_.failedWaitTimer = std::max(0.0f, pathState_.failedWaitTimer - deltaTime);
	if (pathState_.failedWaitTimer > 0.0f)
	{
		pathState_.lastRepathReason = "PathFailedWait";
		StopMove();
		return false;
	}
	if (pathState_.retryTimer >= pathSettings_.repathInterval)
	{
		navigator_.Reset();
		pathState_.retryTimer = 0.0f;
		pathState_.lastRepathReason = "RetryAfterFailure";
	}
	StopMove();
	return false;
}



bool MeleeEnemy::IsObstacleClimbable(const K4E::AABB& obstacle, const K4E::Vector3& selfPos, const K4E::Vector3& moveOrTargetDir) const
{
	if (!traversal_.enabled || !traversal_.allowJumpOverLowObstacles) { return false; }
	const float footY = selfPos.y - 2.0f;
	const float obstacleHeight = obstacle.max.y - footY;
	if (obstacleHeight <= 0.0f || obstacleHeight > traversal_.maxClimbHeight) { return false; }
	// 床面付近に張り付いたAABBは床扱いとして除外する
	if (std::abs(obstacle.max.y - obstacle.min.y) <= 0.05f) { return false; }
	const float width = obstacle.max.x - obstacle.min.x;
	const float depth = obstacle.max.z - obstacle.min.z;
	const K4E::Vector3 moveDir = LengthXZ(moveOrTargetDir) > kEpsilon ? NormalizeXZ(moveOrTargetDir) : K4E::Vector3{ 0.0f, 0.0f, 1.0f };
	const float forwardThickness = std::abs(moveDir.x) >= std::abs(moveDir.z) ? width : depth;
	if (forwardThickness > traversal_.maxClimbObstacleDepth * 4.0f) { return false; }
	const K4E::Vector3 center = (obstacle.min + obstacle.max) * 0.5f;
	const K4E::Vector3 toObs = center - selfPos;
	const float horizontalDistance = LengthXZ(toObs);
	if (horizontalDistance > traversal_.climbHorizontalDistanceMax) { return false; }
	const K4E::Vector3 dirToObs = NormalizeXZ(toObs);
	const float dirDot = dirToObs.x * moveOrTargetDir.x + dirToObs.z * moveOrTargetDir.z;
	return dirDot >= 0.15f || horizontalDistance <= traversal_.climbJumpTriggerDistance;
}

void MeleeEnemy::UpdateTraversalObstacleClassification()
{
	pathBlockingObstacleAABBs_.clear();
	climbableObstacleAABBs_.clear();
	traversalState_.selectedClimbObstacleIndex = -1;
	traversalState_.directClimbCandidateFound = false;
	traversalState_.selectedObstacleJudgeReason = "None";
	traversalState_.selectedObstacleRejectReason = "None";
	const auto* wallSrc = wallObstacleAABBs_;
	const auto* navSrc = GetResolvedNavigationObstacleAABBs();
	if (!wallSrc && !navSrc) { traversalState_.climbableObstacleCount = 0; traversalState_.blockingObstacleCount = 0; return; }
	const K4E::Vector3 selfPos = GetCenterPosition();
	K4E::Vector3 moveOrTargetDir = NormalizeXZ(GetVelocity());
	if (LengthXZ(moveOrTargetDir) <= kEpsilon) { moveOrTargetDir = NormalizeXZ(GetTargetPosition() - selfPos); }
	auto classify = [&](const K4E::AABB& obstacle)
	{
		// 低くて小さい障害物は乗り越え候補、それ以外は経路探索の回避対象に分類する
		if (IsObstacleClimbable(obstacle, selfPos, moveOrTargetDir)) { climbableObstacleAABBs_.push_back(obstacle); }
		else { pathBlockingObstacleAABBs_.push_back(obstacle); }
	};
	if (wallSrc) { for (const auto& obstacle : *wallSrc) { classify(obstacle); } }
	if (navSrc) { for (const auto& obstacle : *navSrc) { classify(obstacle); } }
	traversalState_.climbableObstacleCount = static_cast<int>(climbableObstacleAABBs_.size());
	traversalState_.blockingObstacleCount = static_cast<int>(pathBlockingObstacleAABBs_.size());
}

bool MeleeEnemy::TryDirectClimbOverObstacleToTarget(float deltaTime)
{
	(void)deltaTime;
	if (!traversal_.enabled || !traversal_.preferDirectClimb) { traversalState_.lastReason = "DirectDisabled"; return false; }
	if (!jump_.enabled || !grounded_) { traversalState_.lastReason = "DirectNeedGrounded"; return false; }
	if (!HasTarget() || !IsTargetInDetectRange()) { traversalState_.lastReason = "DirectNoTarget"; return false; }
	const Vector3 selfPos = GetCenterPosition();
	const Vector3 targetPos = GetTargetPosition();
	const float distance = LengthXZ(targetPos - selfPos);
	if (distance > traversal_.directClimbDistanceMax) { traversalState_.lastReason = "DirectTargetFar"; return false; }
	if (climbableObstacleAABBs_.empty()) { traversalState_.lastReason = "DirectNoClimbable"; return false; }
	int best = -1;
	float bestDist = 9999.0f;
	for (size_t i = 0; i < climbableObstacleAABBs_.size(); ++i)
	{
		const auto& o = climbableObstacleAABBs_[i];
		const Vector3 center = (o.min + o.max) * 0.5f;
		const float lineDist = DistancePointToSegmentXZ(center, selfPos, targetPos);
		if (lineDist > traversal_.directLineWidth) { continue; }
		const float d = LengthXZ(center - selfPos);
		if (d < bestDist) { bestDist = d; best = static_cast<int>(i); }
	}
	if (best < 0) { traversalState_.directClimbCandidateFound = false; traversalState_.lastReason = "DirectNoLineCandidate"; return false; }
	traversalState_.directClimbCandidateFound = true;
	traversalState_.selectedClimbObstacleIndex = best;
	const auto& obstacle = climbableObstacleAABBs_[best];
	// 直線乗り越えで選ばれた障害物の判定値をImGuiへ出す
	traversalState_.selectedEnemyFootY = selfPos.y - 2.0f;
	traversalState_.selectedObstacleTopY = obstacle.max.y;
	traversalState_.selectedObstacleHeight = obstacle.max.y - traversalState_.selectedEnemyFootY;
	traversalState_.selectedObstacleWidth = obstacle.max.x - obstacle.min.x;
	traversalState_.selectedObstacleDepth = obstacle.max.z - obstacle.min.z;
	traversalState_.selectedObstacleClimbResult = true;
	traversalState_.selectedObstacleJudgeReason = "DirectLineCandidate";
	traversalState_.selectedObstacleRejectReason = "None";
	const Vector3 center = (obstacle.min + obstacle.max) * 0.5f;
	const Vector3 dir = NormalizeXZ(center - selfPos);
	SetVelocity({ dir.x * move_.moveSpeed, GetVelocity().y, dir.z * move_.moveSpeed });
	traversalState_.lastReason = "DirectClimbOverObstacle";
	// 直線乗り越え中のジャンプ遷移は既存処理を使って高さ挙動を統一する
	TryJumpOverClimbableObstacle(deltaTime);
	return true;
}

bool MeleeEnemy::TryJumpOverClimbableObstacle(float)
{
	if (!traversal_.enabled || !traversal_.allowJumpOverLowObstacles) { traversalState_.lastReason = "Disabled"; jumpState_.lastReason = "NoClimbableObstacle"; return false; }
	if (!jump_.enabled) { traversalState_.lastReason = "Disabled"; jumpState_.lastReason = "Disabled"; return false; }
	if (!grounded_) { traversalState_.lastReason = "Airborne"; jumpState_.lastReason = "Airborne"; return false; }
	if (jumpState_.cooldownTimer > 0.0f) { traversalState_.lastReason = "Cooldown"; jumpState_.lastReason = "Cooldown"; return false; }
	if (climbableObstacleAABBs_.empty()) { traversalState_.nearClimbableObstacle = false; traversalState_.lastReason = "NoClimbable"; return false; }
	const K4E::Vector3 selfPos = GetCenterPosition();
	const K4E::Vector3 targetPos = GetTargetPosition();
	K4E::Vector3 moveOrTargetDir = NormalizeXZ(GetVelocity());
	if (LengthXZ(moveOrTargetDir) <= kEpsilon) { moveOrTargetDir = NormalizeXZ(GetTargetPosition() - selfPos); }
	float bestDist = 9999.0f;
	int selected = -1;
	for (size_t i = 0; i < climbableObstacleAABBs_.size(); ++i)
	{
		const auto& obstacle = climbableObstacleAABBs_[i];
		const K4E::Vector3 center = (obstacle.min + obstacle.max) * 0.5f;
		const K4E::Vector3 toObs = center - selfPos;
		const float horizontalDistance = LengthXZ(toObs);
		if (horizontalDistance > traversal_.climbJumpTriggerDistance) { traversalState_.lastReason = "TooFarFromObstacle"; continue; }
		const K4E::Vector3 dirToObs = NormalizeXZ(toObs);
		const float dirDot = dirToObs.x * moveOrTargetDir.x + dirToObs.z * moveOrTargetDir.z;
		const bool inMoveDirection = dirDot >= 0.1f;
		const float toTargetLineDistance = DistancePointToSegmentXZ(center, selfPos, targetPos);
		const bool betweenSelfAndTarget = toTargetLineDistance <= traversal_.directLineWidth;
		// 進行方向上か、敵-ターゲット間の障害物のみジャンプ対象とする
		if (!inMoveDirection && !betweenSelfAndTarget) { continue; }
		if (horizontalDistance < bestDist) { bestDist = horizontalDistance; selected = static_cast<int>(i); }
	}
	traversalState_.selectedClimbObstacleIndex = selected;
	traversalState_.nearClimbableObstacle = selected >= 0;
	if (!traversalState_.nearClimbableObstacle) { traversalState_.lastReason = "NoNearObstacle"; return false; }
	const auto& selectedObstacle = climbableObstacleAABBs_[traversalState_.selectedClimbObstacleIndex];
	// 近接乗り越えジャンプで選ばれた障害物の値を保持する
	traversalState_.selectedEnemyFootY = selfPos.y - 2.0f;
	traversalState_.selectedObstacleTopY = selectedObstacle.max.y;
	traversalState_.selectedObstacleHeight = selectedObstacle.max.y - traversalState_.selectedEnemyFootY;
	traversalState_.selectedObstacleWidth = selectedObstacle.max.x - selectedObstacle.min.x;
	traversalState_.selectedObstacleDepth = selectedObstacle.max.z - selectedObstacle.min.z;
	traversalState_.selectedObstacleClimbResult = true;
	traversalState_.selectedObstacleJudgeReason = "NearClimbJump";
	traversalState_.selectedObstacleRejectReason = "None";
	Vector3 v = GetVelocity();
	v.y = std::clamp(jump_.baseVelocity, 0.0f, jump_.maxVelocity);
	SetVelocity(v);
	jumpState_.cooldownTimer = jump_.cooldown;
	traversalState_.lastReason = "ClimbObstacleJump";
	jumpState_.lastReason = "ClimbObstacleJump";
	jumpState_.appliedVelocity = v.y;
	return true;
}
bool MeleeEnemy::TryJumpOverContactObstacle()
{
	// 接触ジャンプ判定が呼ばれた事実を毎回記録する
	contactJumpDebugState_.calledThisFrame = true;
	contactJumpDebugState_.everCalled = true;
	const Vector3 pos = GetCenterPosition();
	const float footY = pos.y - 2.0f;
	contactJumpDebugState_.footY = footY;
	contactJumpDebugState_.obstacleTopY = contactObstacleState_.obstacleTopY;
	contactJumpDebugState_.obstacleHeightFromFoot = contactObstacleState_.obstacleTopY - footY;
	contactJumpDebugState_.plannedJumpVelocity = jump_.baseVelocity;
	if (!traversal_.enabled || !traversal_.allowJumpOverLowObstacles) { contactJumpDebugState_.lastReason = "Disabled"; jumpState_.lastReason = "Disabled"; return false; }
	if (!jump_.enabled) { contactJumpDebugState_.lastReason = "Disabled"; jumpState_.lastReason = "Disabled"; return false; }
	if (IsDeadCondition()) { contactJumpDebugState_.lastReason = "Dead"; jumpState_.lastReason = "Dead"; return false; }
	if (attackController_.IsAttacking() || attackState_.lockTimer > 0.0f) { contactJumpDebugState_.lastReason = "Attack"; return false; }
	if (jumpState_.cooldownTimer > 0.0f) { contactJumpDebugState_.lastReason = "Cooldown"; jumpState_.lastReason = "Cooldown"; return false; }
	if (!contactObstacleState_.hasContact) { contactJumpDebugState_.lastReason = "NoContactObstacle"; return false; }
	if (!contactObstacleState_.climbable) { contactJumpDebugState_.lastReason = "ContactNotClimbable"; jumpState_.lastReason = contactObstacleState_.notClimbableReason; return false; }
	const bool canContactJump = grounded_ || collision_.isOverlappingWallObstacle;
	if (!canContactJump) { contactJumpDebugState_.lastReason = "Airborne"; jumpState_.lastReason = "Airborne"; return false; }
	if (contactObstacleState_.obstacleTopY <= footY + traversal_.minClimbHeight) { contactJumpDebugState_.lastReason = "ObstacleTopTooLow"; jumpState_.lastReason = "ObstacleTopTooLow"; return false; }
	if (contactObstacleState_.obstacleHeightFromFoot > traversal_.maxClimbHeight) { contactJumpDebugState_.lastReason = "ObstacleTooHigh"; jumpState_.lastReason = "ObstacleTooHigh"; return false; }
	// 接触フォールバックは障害物高さで増減させず、固定ジャンプ力で発火する
	Vector3 v = GetVelocity();
	v.y = jump_.baseVelocity;
	SetVelocity(v);
	jumpState_.cooldownTimer = jump_.cooldown;
	jumpState_.appliedVelocity = v.y;
	jumpState_.lastReason = "ContactObstacleJump";
	traversalState_.lastReason = "ContactClimbableObstacle";
	contactObstacleState_.reason = "ClimbableByTopHeight";
	contactObstacleState_.possibleReason = "ContactClimbableObstacle";
	contactJumpDebugState_.lastReason = "Jump";
	grounded_ = false;
	currentBehaviorName_ = "ContactObstacleJump";
	return true;
}
void MeleeEnemy::TryJumpForTraversal(float)
{
	if (!jump_.enabled) { jumpState_.lastReason = "Disabled"; jumpState_.appliedVelocity = 0.0f; return; }
	if (IsDeadCondition()) { jumpState_.lastReason = "Dead"; jumpState_.appliedVelocity = 0.0f; return; }
	if (attackController_.IsAttacking() || attackState_.lockTimer > 0.0f) { jumpState_.lastReason = "Attack"; jumpState_.appliedVelocity = 0.0f; return; }
	if (!grounded_) { jumpState_.lastReason = "Airborne"; jumpState_.appliedVelocity = 0.0f; return; }
	if (jumpState_.cooldownTimer > 0.0f) { jumpState_.lastReason = "Cooldown"; jumpState_.appliedVelocity = 0.0f; return; }

	// ターゲット高度ではなく乗り越え可能障害物の有無だけで通常近接雑魚のジャンプ可否を決める。
	if (!traversalState_.directClimbCandidateFound && !traversalState_.nearClimbableObstacle)
	{
		jumpState_.lastReason = "NoClimbableObstacle";
		jumpState_.appliedVelocity = 0.0f;
		return;
	}

	if (!TryJumpOverClimbableObstacle(0.0f) && !TryJumpOverContactObstacle())
	{
		jumpState_.appliedVelocity = 0.0f;
	}
}

void MeleeEnemy::UpdateStuckState(float deltaTime)
{
	stuck_.timer += deltaTime;
	if (stuck_.timer < stuckSettings_.checkTime) { return; }
	const float moved = LengthXZ(GetCenterPosition() - pathState_.lastStuckCheckPosition);
	const bool attackingOrInRange = attackController_.IsAttacking() || GetDistanceToTarget() <= detection_.attackStartRange;
	const bool tryingToMove = LengthXZ(GetVelocity()) > 0.05f || std::string(currentBehaviorName_).find("Chase") != std::string::npos;
	stuck_.isStuck = !attackingOrInRange && tryingToMove && moved <= stuckSettings_.moveThreshold && GetDistanceToTarget() > detection_.stopDistance;
	pathState_.lastMovedDistance = moved;
	if (stuck_.isStuck)
	{
		// 詰まった地点を一時的に通行不可へ追加し、再探索時に同じ障害物へ押し込み続けないようにする
		navigator_.AddTemporaryBlockedArea(GetCenterPosition(), pathSettings_.temporaryBlockRadius, pathSettings_.temporaryBlockDuration, "StuckPosition");
		navigator_.Reset();
		pathSettings_.stuckRepathExpandBonus = std::min(pathSettings_.stuckRepathExpandBonus + 0.1f, pathSettings_.maxStuckRepathExpandBonus);
		pathState_.lastRepathReason = "RepathFromStuck";
		if (collision_.blockedByObstacle || pathState_.lineBlocked)
		{
			const Vector3 tangent = NormalizeXZ(Vector3{ -collision_.lastWallResolvePush.z, 0.0f, collision_.lastWallResolvePush.x });
			if (LengthXZ(tangent) > kEpsilon)
			{
				const Vector3 toTarget = NormalizeXZ(GetTargetPosition() - GetCenterPosition());
				const Vector3 invTangent = tangent * -1.0f;
				const float dotA = tangent.x * toTarget.x + tangent.z * toTarget.z;
				const float dotB = invTangent.x * toTarget.x + invTangent.z * toTarget.z;
				const Vector3 sideEscape = (dotA >= dotB) ? tangent : invTangent;
				SetVelocity({ sideEscape.x * (move_.moveSpeed * 0.45f), GetVelocity().y, sideEscape.z * (move_.moveSpeed * 0.45f) });
			}
		}
		else
		{
			StopMove();
		}
	}
	else
	{
		pathSettings_.stuckRepathExpandBonus = std::max(0.0f, pathSettings_.stuckRepathExpandBonus - 0.05f);
	}
	pathState_.lastStuckCheckPosition = GetCenterPosition();
	stuck_.timer = 0.0f;
}

void MeleeEnemy::WanderAction(float deltaTime)
{
	wander_.timer -= deltaTime;
	if (wander_.timer <= 0.0f)
	{
		wander_.timer = 1.4f;
		const float angle = static_cast<float>((std::rand() % 360) * (kPi / 180.0f));
		wander_.direction = { std::sin(angle), 0.0f, std::cos(angle) };
	}
	SetVelocity({ wander_.direction.x * (move_.moveSpeed * 0.45f), GetVelocity().y, wander_.direction.z * (move_.moveSpeed * 0.45f) });
	FaceToMoveDirection(1.0f / 60.0f);
	currentBehaviorName_ = "WanderAction";
	animationState_.animState = AnimState::Walk;
}

void MeleeEnemy::ApplyAttackMove(const Vector3& horizontalVelocity)
{
	if (attackSettings_.selectedAttackType == MeleeAttackType::OneTwo && GetDistanceToTarget() > detection_.minOneTwoForwardDistance)
	{
		SetVelocity({ horizontalVelocity.x, GetVelocity().y, horizontalVelocity.z });
	}
	else
	{
		StopMove();
	}
}

void MeleeEnemy::NotifyAttackHit(int, const Vector3&)
{
	// TODO: Playerへの実ダメージ処理の接続先が確定したらここで適用する
}

void MeleeEnemy::EvaluateBehavior(float deltaTime)
{
	if (IsDeadCondition()) { DeadAction(); return; }
	if (!HasTarget()) { WanderAction(deltaTime); return; }

	if (attackController_.IsAttacking() || attackState_.lockTimer > 0.0f)
	{
		MeleeAttackAction();
		return;
	}

	const float distance = GetDistanceToTarget();
	stuck_.isStuck = (distance <= detection_.stopDistance && LengthXZ(GetVelocity()) < 0.05f);

	if (distance <= detection_.attackStartRange)
	{
		if (IsAttackCooldownReady()) { MeleeAttackAction(); return; }
		CombatIdleAction();
		return;
	}

	if (IsTargetInAttackHoldRange() && !IsAttackCooldownReady())
	{
		CombatIdleAction();
		return;
	}

	if (distance <= detection_.meleeAttackRange)
	{
		// 攻撃射程内では押し込みを止める
		CombatIdleAction();
		return;
	}

	attackState_.shouldChase = attackState_.shouldChase ? !IsTargetInAttackHoldRange() : IsMoveResumeDistanceReached();
	if (HasTarget() && IsTargetInDetectRange() && (attackState_.shouldChase || distance > detection_.resumeChaseDistance))
	{
		ChaseTargetAction();
		return;
	}
	WanderAction(deltaTime);
}

void MeleeEnemy::StopMove()
{
	SetVelocity({ 0.0f, GetVelocity().y, 0.0f });
}

void MeleeEnemy::ForceAttack(MeleeAttackType type)
{
	if (type == MeleeAttackType::Scratch) { scratchArmState_.wasScratchAttacking = false; }
	attackSettings_.selectedAttackType = type;
	attackController_.ResetCooldown();
	attackController_.StopAttack();
	attackController_.StartAttack(type);
	attackState_.lockTimer = attackSettings_.lockTime;
}

void MeleeEnemy::StopAttack()
{
	attackController_.StopAttack();
	attackState_.lockTimer = 0.0f;
}

void MeleeEnemy::ResetAttackCooldown()
{
	attackController_.ResetCooldown();
}

void MeleeEnemy::UpdateVisualAnimation(float deltaTime)
{
	if (parts_.size() < 5 || !body_.object) { return; }
	const uint32_t lArm = partIndices_.leftArm;
	const uint32_t rArm = partIndices_.rightArm;
	const uint32_t lLeg = partIndices_.leftLeg;
	const uint32_t rLeg = partIndices_.rightLeg;
	const uint32_t head = partIndices_.head;
	const float speedRate = std::min(1.5f, LengthXZ(GetVelocity()) / std::max(move_.moveSpeed, kEpsilon));
	animationState_.walkAnimTime += deltaTime * (animation_.walkAnimSpeed * std::max(0.2f, speedRate));
	float armTarget = 0.0f;
	float legTarget = 0.0f;
	float lAttack = 0.0f;
	float rAttack = 0.0f;
	if (attackController_.IsAttacking())
	{
		const bool isScratch = attackController_.GetCurrentAttackType() == MeleeAttackType::Scratch;
		// Scratch攻撃の開始フレームでだけ使用腕を切り替えて固定する
		if (isScratch && !scratchArmState_.wasScratchAttacking)
		{
			scratchArmState_.useLeftArm = !scratchArmState_.useLeftArm;
		}
		scratchArmState_.wasScratchAttacking = isScratch;
		if (isScratch)
		{
			if (scratchArmState_.useLeftArm) { lAttack = animation_.attackArmSwing; }
			else { rAttack = animation_.attackArmSwing; }
		}
		if (attackController_.GetCurrentAttackType() == MeleeAttackType::OneTwo && attackController_.GetCurrentStepIndex() == 0) { lAttack = animation_.attackArmSwing; }
		if (attackController_.GetCurrentAttackType() == MeleeAttackType::OneTwo && attackController_.GetCurrentStepIndex() == 1) { rAttack = animation_.attackArmSwing; }
	}
	else
	{
		scratchArmState_.wasScratchAttacking = false;
		if (animationState_.animState == AnimState::Walk)
		{
			armTarget = std::sin(animationState_.walkAnimTime) * animation_.walkArmSwing * speedRate;
			legTarget = std::sin(animationState_.walkAnimTime) * animation_.walkLegSwing * speedRate;
		}
	}
	const float ret = std::min(1.0f, animation_.attackReturnSpeed * deltaTime);
	parts_[lArm].transform.rotate_.x += ((armTarget - lAttack) - parts_[lArm].transform.rotate_.x) * ret;
	parts_[rArm].transform.rotate_.x += ((-armTarget - rAttack) - parts_[rArm].transform.rotate_.x) * ret;
	parts_[lLeg].transform.rotate_.x += ((-legTarget) - parts_[lLeg].transform.rotate_.x) * ret;
	parts_[rLeg].transform.rotate_.x += ((legTarget)-parts_[rLeg].transform.rotate_.x) * ret;
	// 頭だけをターゲット方向に向ける（bodyに対する相対Yaw/Pitch）
	if (head < parts_.size())
	{
		// 頭向き制御のデフォルトは「正面へ戻す」にして、必要時だけ注視を有効化する
		headLookState_.targetYaw = 0.0f;
		headLookState_.targetPitch = 0.0f;
		headLookState_.targetVisible = false;
		headLookState_.reason = "Disabled";
		if (headLookSettings_.enabled)
		{
			headLookState_.reason = "NoTarget";
			if (HasTarget())
			{
				const Vector3 toTarget = GetTargetPosition() - GetCenterPosition();
				const float targetDistance = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z);
				if (targetDistance <= detection_.detectRange)
				{
					// detectRange内でのみ頭の注視ターゲットを有効化し、範囲外では正面へ戻す
					headLookState_.targetVisible = true;
					headLookState_.reason = "InRange";
					const float desiredYaw = std::atan2(-toTarget.x, toTarget.z);
					const float bodyYaw = orientation_.y;
					const float yawDelta = NormalizeAngleRad(desiredYaw - bodyYaw);
					headLookState_.targetYaw = Clamp(yawDelta * (180.0f / kPi), -headLookSettings_.yawLimitDeg, headLookSettings_.yawLimitDeg);
					const float horizontalDistance = std::max(LengthXZ(toTarget), kEpsilon);
					const float pitchDeg = -std::atan2(toTarget.y, horizontalDistance) * (180.0f / kPi);
					headLookState_.targetPitch = Clamp(pitchDeg, headLookSettings_.pitchMinDeg, headLookSettings_.pitchMaxDeg);
				}
				else
				{
					headLookState_.reason = "OutOfRange";
				}
			}
		}
		// 体の向きは既存処理に任せ、頭だけを補間で自然に回頭/復帰させる
		const float headLerp = std::min(1.0f, headLookSettings_.lerpSpeed * deltaTime);
		headLookState_.currentYaw += (headLookState_.targetYaw - headLookState_.currentYaw) * headLerp;
		headLookState_.currentPitch += (headLookState_.targetPitch - headLookState_.currentPitch) * headLerp;
		parts_[head].transform.rotate_.y = headLookState_.currentYaw * (kPi / 180.0f);
		parts_[head].transform.rotate_.x = headLookState_.currentPitch * (kPi / 180.0f);
	}
	body_.transform.rotate_.x = (lAttack + rAttack) * animation_.attackBodyLean;
	UpdateVisualHierarchy();
}

bool MeleeEnemy::IsInsideStageBounds(const Vector3& position) const
{
	if (!collision_.hasStageBounds) { return true; }
	return position.x >= collision_.stageBoundsMin.x && position.x <= collision_.stageBoundsMax.x && position.z >= collision_.stageBoundsMin.z && position.z <= collision_.stageBoundsMax.z;
}

const char* MeleeEnemy::GetAnimStateName() const
{
	switch (animationState_.animState)
	{
	case AnimState::Idle: return "Idle";
	case AnimState::Walk: return "Walk";
	case AnimState::Scratch: return "Scratch";
	case AnimState::OneTwo: return "OneTwo";
	case AnimState::Dead: return "Dead";
	default: return "Unknown";
	}
}

bool MeleeEnemy::LoadTuningFromJson(const std::filesystem::path& path, std::string* outMessage)
{
	try
	{
		// 読み込み前に保存先ディレクトリを作成して、Project外へResourcesが作られる経路を使わないようにする。
		std::filesystem::create_directories(path.parent_path());
		std::ifstream ifs(path);
		if (!ifs.is_open())
		{
			if (outMessage) { *outMessage = "ファイルなし: デフォルト値を使用"; }
			return false;
		}
		nlohmann::json j;
		ifs >> j;
		// JSONの値を調整パラメータへ反映する
		detection_.detectRange = j.value("detectRange", detection_.detectRange);
		detection_.meleeAttackRange = j.value("meleeAttackRange", detection_.meleeAttackRange);
		detection_.stopDistance = j.value("stopDistance", detection_.stopDistance);
		detection_.attackStartRange = j.value("attackStartRange", detection_.attackStartRange);
		detection_.resumeChaseDistance = j.value("resumeChaseDistance", detection_.resumeChaseDistance);
		detection_.minOneTwoForwardDistance = j.value("minOneTwoForwardDistance", detection_.minOneTwoForwardDistance);
		move_.moveSpeed = j.value("moveSpeed", move_.moveSpeed);
		move_.rotateSpeed = j.value("rotateSpeed", move_.rotateSpeed);
		move_.maxResolvePushPerFrame = j.value("maxResolvePushPerFrame", move_.maxResolvePushPerFrame);
		move_.maxHorizontalPushPerFrame = j.value("maxHorizontalPushPerFrame", move_.maxHorizontalPushPerFrame);
		jump_.enabled = j.value("jumpEnabled", jump_.enabled);
		jump_.baseVelocity = j.value("jumpBaseVelocity", jump_.baseVelocity);
		jump_.cooldown = j.value("jumpCooldown", jump_.cooldown);
		traversal_.enabled = j.value("traversalEnabled", traversal_.enabled);
		traversal_.preferDirectClimb = j.value("traversalPrioritizeDirectClimb", traversal_.preferDirectClimb);
		traversal_.maxClimbHeight = j.value("traversalMaxClimbHeight", traversal_.maxClimbHeight);
		traversal_.minClimbHeight = j.value("traversalMinClimbHeight", traversal_.minClimbHeight);
		traversal_.maxClimbObstacleWidth = j.value("traversalMaxClimbObstacleWidth", traversal_.maxClimbObstacleWidth);
		traversal_.maxClimbObstacleDepth = j.value("traversalMaxClimbObstacleDepth", traversal_.maxClimbObstacleDepth);
		traversal_.climbJumpTriggerDistance = j.value("traversalClimbJumpTriggerDistance", traversal_.climbJumpTriggerDistance);
		traversal_.climbHorizontalDistanceMax = j.value("traversalClimbHorizontalDistanceMax", traversal_.climbHorizontalDistanceMax);
		traversal_.directClimbDistanceMax = j.value("traversalDirectClimbMaxTargetDistance", traversal_.directClimbDistanceMax);
		traversal_.directLineWidth = j.value("traversalDirectClimbLineWidth", traversal_.directLineWidth);
		traversal_.allowJumpOverLowObstacles = j.value("traversalAllowJumpOverLowObstacles", traversal_.allowJumpOverLowObstacles);
		pathSettings_.enabled = j.value("pathFindEnabled", pathSettings_.enabled);
		pathSettings_.repathInterval = j.value("repathInterval", pathSettings_.repathInterval);
		pathSettings_.waypointReachDistance = j.value("waypointReachDistance", pathSettings_.waypointReachDistance);
		pathSettings_.gridSize = j.value("pathGridSize", pathSettings_.gridSize);
		pathSettings_.searchRadius = j.value("pathSearchRadius", pathSettings_.searchRadius);
		pathSettings_.obstacleExpandRadius = j.value("obstacleExpandRadius", pathSettings_.obstacleExpandRadius);
		pathSettings_.temporaryBlockDuration = j.value("temporaryBlockDuration", pathSettings_.temporaryBlockDuration);
		pathSettings_.temporaryBlockRadius = j.value("temporaryBlockRadius", pathSettings_.temporaryBlockRadius);
		pathSettings_.cornerCuttingDisabled = j.value("cornerCuttingDisabled", pathSettings_.cornerCuttingDisabled);
		stuckSettings_.checkTime = j.value("stuckCheckTime", stuckSettings_.checkTime);
		stuckSettings_.distance = j.value("stuckDistance", stuckSettings_.distance);
		stuckSettings_.moveThreshold = j.value("stuckMoveThreshold", stuckSettings_.moveThreshold);
		attackSettings_.lockTime = j.value("attackLockTime", attackSettings_.lockTime);
		attackSettings_.selectedAttackType = static_cast<MeleeAttackType>(j.value("selectedAttackType", static_cast<int>(attackSettings_.selectedAttackType)));
		animation_.walkAnimSpeed = j.value("walkAnimSpeed", animation_.walkAnimSpeed);
		animation_.walkArmSwing = j.value("walkArmSwing", animation_.walkArmSwing);
		animation_.walkLegSwing = j.value("walkLegSwing", animation_.walkLegSwing);
		animation_.attackArmSwing = j.value("attackArmSwing", animation_.attackArmSwing);
		animation_.attackReturnSpeed = j.value("attackReturnSpeed", animation_.attackReturnSpeed);
		animation_.attackBodyLean = j.value("attackBodyLean", animation_.attackBodyLean);
		headLookSettings_.enabled = j.value("headLookEnabled", headLookSettings_.enabled);
		headLookSettings_.yawLimitDeg = j.value("headYawLimitDeg", headLookSettings_.yawLimitDeg);
		headLookSettings_.pitchMinDeg = j.value("headPitchMinDeg", headLookSettings_.pitchMinDeg);
		headLookSettings_.pitchMaxDeg = j.value("headPitchMaxDeg", headLookSettings_.pitchMaxDeg);
		headLookSettings_.lerpSpeed = j.value("headLookLerpSpeed", headLookSettings_.lerpSpeed);
		if (auto* s = attackController_.FindPattern(MeleeAttackType::Scratch))
		{
			auto& st = s->steps[0];
			st.damage = j.value("scratchDamage", st.damage);
			st.range = j.value("scratchRange", st.range);
			st.radius = j.value("scratchRadius", st.radius);
			st.startTime = j.value("scratchStartTime", st.startTime);
			st.activeTime = j.value("scratchActiveTime", st.activeTime);
			s->recoveryTime = j.value("scratchRecoveryTime", s->recoveryTime);
			s->cooldown = j.value("scratchCooldown", s->cooldown);
		}
		if (auto* o = attackController_.FindPattern(MeleeAttackType::OneTwo))
		{
			auto& l = o->steps[0]; auto& r = o->steps[1];
			l.damage = j.value("oneTwoLeftDamage", l.damage); r.damage = j.value("oneTwoRightDamage", r.damage);
			l.range = j.value("oneTwoLeftRange", l.range); r.range = j.value("oneTwoRightRange", r.range);
			l.radius = j.value("oneTwoLeftRadius", l.radius); r.radius = j.value("oneTwoRightRadius", r.radius);
			l.startTime = j.value("oneTwoLeftStartTime", l.startTime); r.startTime = j.value("oneTwoRightStartTime", r.startTime);
			l.activeTime = j.value("oneTwoLeftActiveTime", l.activeTime); r.activeTime = j.value("oneTwoRightActiveTime", r.activeTime);
			o->forwardMoveSpeed = j.value("oneTwoForwardMoveSpeed", o->forwardMoveSpeed);
			o->forwardMoveDuration = j.value("oneTwoForwardMoveDuration", o->forwardMoveDuration);
			o->recoveryTime = j.value("oneTwoRecoveryTime", o->recoveryTime);
			o->cooldown = j.value("oneTwoCooldown", o->cooldown);
		}
		navigator_.Reset();
		if (outMessage) { *outMessage = "読み込み成功"; }
		return true;
	}
	catch (const std::exception& e)
	{
		if (outMessage) { *outMessage = std::string("読み込み失敗: ") + e.what(); }
		return false;
	}
}

bool MeleeEnemy::SaveTuningToJson(const std::filesystem::path& path, std::string* outMessage) const
{
	try
	{
		nlohmann::json j;
		j["detectRange"] = detection_.detectRange;
		j["meleeAttackRange"] = detection_.meleeAttackRange;
		j["stopDistance"] = detection_.stopDistance;
		j["attackStartRange"] = detection_.attackStartRange;
		j["resumeChaseDistance"] = detection_.resumeChaseDistance;
		j["minOneTwoForwardDistance"] = detection_.minOneTwoForwardDistance;
		j["moveSpeed"] = move_.moveSpeed;
		j["rotateSpeed"] = move_.rotateSpeed;
		j["maxResolvePushPerFrame"] = move_.maxResolvePushPerFrame;
		j["maxHorizontalPushPerFrame"] = move_.maxHorizontalPushPerFrame;
		j["jumpEnabled"] = jump_.enabled;
		j["jumpBaseVelocity"] = jump_.baseVelocity;
		j["jumpCooldown"] = jump_.cooldown;
		j["traversalEnabled"] = traversal_.enabled;
		j["traversalPrioritizeDirectClimb"] = traversal_.preferDirectClimb;
		j["traversalMaxClimbHeight"] = traversal_.maxClimbHeight;
		j["traversalMinClimbHeight"] = traversal_.minClimbHeight;
		j["traversalMaxClimbObstacleWidth"] = traversal_.maxClimbObstacleWidth;
		j["traversalMaxClimbObstacleDepth"] = traversal_.maxClimbObstacleDepth;
		j["traversalClimbJumpTriggerDistance"] = traversal_.climbJumpTriggerDistance;
		j["traversalClimbHorizontalDistanceMax"] = traversal_.climbHorizontalDistanceMax;
		j["traversalDirectClimbMaxTargetDistance"] = traversal_.directClimbDistanceMax;
		j["traversalDirectClimbLineWidth"] = traversal_.directLineWidth;
		j["traversalAllowJumpOverLowObstacles"] = traversal_.allowJumpOverLowObstacles;
		j["pathFindEnabled"] = pathSettings_.enabled;
		j["repathInterval"] = pathSettings_.repathInterval;
		j["waypointReachDistance"] = pathSettings_.waypointReachDistance;
		j["pathGridSize"] = pathSettings_.gridSize;
		j["pathSearchRadius"] = pathSettings_.searchRadius;
		j["obstacleExpandRadius"] = pathSettings_.obstacleExpandRadius;
		j["temporaryBlockDuration"] = pathSettings_.temporaryBlockDuration;
		j["temporaryBlockRadius"] = pathSettings_.temporaryBlockRadius;
		j["cornerCuttingDisabled"] = pathSettings_.cornerCuttingDisabled;
		j["stuckCheckTime"] = stuckSettings_.checkTime;
		j["stuckDistance"] = stuckSettings_.distance;
		j["stuckMoveThreshold"] = stuckSettings_.moveThreshold;
		j["attackLockTime"] = attackSettings_.lockTime;
		j["selectedAttackType"] = static_cast<int>(attackSettings_.selectedAttackType);
		j["walkAnimSpeed"] = animation_.walkAnimSpeed;
		j["walkArmSwing"] = animation_.walkArmSwing;
		j["walkLegSwing"] = animation_.walkLegSwing;
		j["attackArmSwing"] = animation_.attackArmSwing;
		j["attackReturnSpeed"] = animation_.attackReturnSpeed;
		j["attackBodyLean"] = animation_.attackBodyLean;
		j["headLookEnabled"] = headLookSettings_.enabled;
		j["headYawLimitDeg"] = headLookSettings_.yawLimitDeg;
		j["headPitchMinDeg"] = headLookSettings_.pitchMinDeg;
		j["headPitchMaxDeg"] = headLookSettings_.pitchMaxDeg;
		j["headLookLerpSpeed"] = headLookSettings_.lerpSpeed;
		if (const auto* s = attackController_.FindPattern(MeleeAttackType::Scratch)) { const auto& st = s->steps[0]; j["scratchDamage"] = st.damage; j["scratchRange"] = st.range; j["scratchRadius"] = st.radius; j["scratchStartTime"] = st.startTime; j["scratchActiveTime"] = st.activeTime; j["scratchRecoveryTime"] = s->recoveryTime; j["scratchCooldown"] = s->cooldown; }
		if (const auto* o = attackController_.FindPattern(MeleeAttackType::OneTwo)) { const auto& l = o->steps[0]; const auto& r = o->steps[1]; j["oneTwoLeftDamage"] = l.damage; j["oneTwoRightDamage"] = r.damage; j["oneTwoLeftRange"] = l.range; j["oneTwoRightRange"] = r.range; j["oneTwoLeftRadius"] = l.radius; j["oneTwoRightRadius"] = r.radius; j["oneTwoLeftStartTime"] = l.startTime; j["oneTwoRightStartTime"] = r.startTime; j["oneTwoLeftActiveTime"] = l.activeTime; j["oneTwoRightActiveTime"] = r.activeTime; j["oneTwoForwardMoveSpeed"] = o->forwardMoveSpeed; j["oneTwoForwardMoveDuration"] = o->forwardMoveDuration; j["oneTwoRecoveryTime"] = o->recoveryTime; j["oneTwoCooldown"] = o->cooldown; }
		// 保存先ディレクトリはResources/DataAssets/Enemy/MeleeEnemy配下に限定して作成する。
		std::filesystem::create_directories(path.parent_path());
		std::ofstream ofs(path);
		ofs << j.dump(4);
		if (outMessage) { *outMessage = "保存成功"; }
		return true;
	}
	catch (const std::exception& e)
	{
		if (outMessage) { *outMessage = std::string("保存失敗: ") + e.what(); }
		return false;
	}
}

void MeleeEnemy::ResetTuningToDefault()
{
	// 調整値を既定値へ戻すために設定構造体を再初期化する
	detection_ = DetectionSettings{};
	move_ = MoveSettings{};
	jump_ = JumpSettings{};
	traversal_ = TraversalSettings{};
	pathSettings_ = PathSettings{};
	stuckSettings_ = StuckSettings{};
	attackSettings_ = AttackSettings{};
	animation_ = AnimationSettings{};
	headLookSettings_ = HeadLookSettings{};
	attackController_.Initialize();
	navigator_.Reset();
}
