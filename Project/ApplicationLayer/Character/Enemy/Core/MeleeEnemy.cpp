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

void MeleeEnemy::BeginSeparationFrame()
{
	// 毎フレームの個体間分離デバッグ状態を初期化する。
	separationState_.overlappingEnemyCount = 0;
	separationState_.lastSeparationPush = { 0.0f, 0.0f, 0.0f };
}

Vector3 MeleeEnemy::ApplySeparationPushXZ(const Vector3& desiredPush, float pushScale)
{
	// 個体間分離はXZ平面のみ反映し、Y方向の速度と位置は変更しない。
	if (!separationSettings_.enabled) { return { 0.0f, 0.0f, 0.0f }; }
	const float maxPush = std::max(0.0f, separationSettings_.maxPushPerFrame);
	Vector3 push = { desiredPush.x * pushScale, 0.0f, desiredPush.z * pushScale };
	const float pushLen = LengthXZ(push);
	if (pushLen > maxPush && pushLen > kEpsilon)
	{
		const float s = maxPush / pushLen;
		push.x *= s;
		push.z *= s;
	}
	Vector3 pos = GetCenterPosition();
	Vector3 nextPos = pos + push;
	nextPos.y = pos.y;
	if (collision_.hasStageBounds && !IsInsideStageBounds(nextPos))
	{
		// ステージ外へ出る方向の押し出しは破棄して、暴発を防ぐ。
		return { 0.0f, 0.0f, 0.0f };
	}
	SetCenterPosition(nextPos);
	separationState_.lastSeparationPush = push;
	return push;
}

void MeleeEnemy::AddSeparationOverlapCount(int count)
{
	// 個体間分離で重なり相手が何体いたかをImGui表示用に蓄積する。
	separationState_.overlappingEnemyCount += std::max(0, count);
}
void MeleeEnemy::Draw()
{
	EnemyBase::Draw();
	if (!detailDebugDrawEnabled_) { return; }
	const float colorPhase = pathDebugColorOffset_;
	const Vector4 pathLineColor = pathDebugSelected_ ? Vector4{ 0.15f, 1.0f, 0.75f, 1.0f } : Vector4{ 0.2f + 0.25f * std::sin(colorPhase), 0.75f, 0.7f + 0.2f * std::cos(colorPhase), 0.9f };
	const Vector3 origin = GetCenterPosition() + Vector3{ 0.0f, 1.0f, 0.0f };
	Wireframe::GetInstance()->DrawLine(origin, origin + animationState_.movementDirection * 1.8f, { 0.2f, 0.8f, 1.0f, 1.0f });
	Wireframe::GetInstance()->DrawLine(origin, origin + animationState_.targetDirection * 2.0f, { 1.0f, 1.0f, 0.2f, 1.0f });
	Wireframe::GetInstance()->DrawLine(origin, origin + animationState_.visualForward * 2.2f, { 1.0f, 0.4f, 1.0f, 1.0f });
	Wireframe::GetInstance()->DrawLine(origin, origin + animationState_.attackForward * 2.4f, { 1.0f, 0.2f, 0.2f, 1.0f });
	const auto& path = navigator_.GetCurrentPath();
	for (size_t i = 1; i < path.size(); ++i)
	{
		Wireframe::GetInstance()->DrawLine(path[i - 1] + Vector3{ 0.0f, 0.15f, 0.0f }, path[i] + Vector3{ 0.0f, 0.15f, 0.0f }, pathLineColor);
	}
	if (pathDebugDrawEnabled_)
	for (size_t i = 0; i < path.size(); ++i)
	{
		const Vector4 c = (static_cast<int>(i) == navigator_.GetCurrentPathIndex()) ? Vector4{ 1.0f, 0.3f, 0.1f, 1.0f } : Vector4{ 0.1f, 0.9f, 1.0f, 1.0f };
		Wireframe::GetInstance()->DrawSphere(path[i] + Vector3{ 0.0f, 0.2f, 0.0f }, (static_cast<int>(i) == navigator_.GetCurrentPathIndex()) ? 0.26f : 0.16f, c);
	}
	if (pathDetailDebugDrawEnabled_ && pathState_.lineBlocked)
	{
		Wireframe::GetInstance()->DrawLine(pathState_.blockedSegmentFrom + Vector3{ 0.0f, 0.2f, 0.0f }, pathState_.blockedSegmentTo + Vector3{ 0.0f, 0.2f, 0.0f }, { 1.0f, 0.15f, 0.1f, 1.0f });
	}
	if (pathDetailDebugDrawEnabled_ && HasTarget())
	{
		// ジャンプ追跡判定の可視化として、敵からターゲットへの線を専用色で表示する
		Wireframe::GetInstance()->DrawLine(origin + Vector3{ 0.0f, 0.2f, 0.0f }, GetTargetPosition() + Vector3{ 0.0f, 0.2f, 0.0f }, { 0.5f, 1.0f, 0.1f, 1.0f });
	}
	if (pathDetailDebugDrawEnabled_)
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
			Wireframe::GetInstance()->DrawAABB({ contactObstacleState_.obstacleAABB.min - Vector3{ 0.03f, 0.03f, 0.03f }, contactObstacleState_.obstacleAABB.max + Vector3{ 0.03f, 0.03f, 0.03f } }, { 0.6f, 1.0f, 0.6f, 0.95f });
		}
		const Vector3 foot = GetCenterPosition() + Vector3{ 0.0f, -2.0f, 0.0f };
		const Vector3 top = Vector3{ foot.x, contactObstacleState_.obstacleTopY, foot.z };
		Wireframe::GetInstance()->DrawLine(foot, top, { 1.0f, 1.0f, 0.2f, 0.9f });
	}
	for (const auto& blocked : navigator_.GetTemporaryBlockedAreas())
	{
		Wireframe::GetInstance()->DrawSphere(blocked.center + Vector3{ 0.0f, 0.2f, 0.0f }, blocked.radius, { 1.0f, 0.2f, 0.8f, 0.35f });
	}
	// 選択中個体の分離半径と押し出し方向を可視化して、重なり解消の挙動を確認しやすくする。
	Wireframe::GetInstance()->DrawSphere(GetCenterPosition() + Vector3{ 0.0f, 0.15f, 0.0f }, separationSettings_.radius, { 0.2f, 0.8f, 1.0f, 0.35f });
	if (separationState_.overlappingEnemyCount > 0)
	{
		const Vector3 lineStart = GetCenterPosition() + Vector3{ 0.0f, 1.1f, 0.0f };
		const Vector3 lineEnd = lineStart + separationState_.lastSeparationPush * 12.0f;
		Wireframe::GetInstance()->DrawLine(lineStart, lineEnd, { 0.0f, 1.0f, 0.35f, 1.0f });
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
			// 調整JSONの運用情報をデバッグ表示する。
			ImGui::Text("現在のJSON形式バージョン: v%d", tuningIo_.jsonFormatVersion);
			ImGui::Text("保存対象カテゴリ数: %d", tuningIo_.savedCategoryCount);
		}
		if (ImGui::CollapsingHeader("検知・攻撃距離", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::SliderFloat("検知範囲", &detection_.detectRange, 1.0f, 50.0f);
			ImGui::SliderFloat("近接攻撃距離", &detection_.meleeAttackRange, 0.5f, 10.0f);
			ImGui::SliderFloat("停止距離", &detection_.stopDistance, 0.5f, 6.0f);
			ImGui::SliderFloat("攻撃開始距離", &detection_.attackStartRange, 0.5f, 8.0f);
			ImGui::SliderFloat("追跡再開距離", &detection_.resumeChaseDistance, 0.5f, 10.0f);
			ImGui::SliderFloat("踏み込み前進最小距離", &detection_.minLungeForwardDistance, 0.1f, 5.0f);
		}
		if (ImGui::CollapsingHeader("移動", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::SliderFloat("移動速度", &move_.moveSpeed, 0.1f, 10.0f);
			ImGui::SliderFloat("回転速度", &move_.rotateSpeed, 0.1f, 20.0f);
			ImGui::SliderFloat("最大押し戻し量", &move_.maxResolvePushPerFrame, 0.05f, 2.0f);
			ImGui::SliderFloat("水平押し戻し量", &move_.maxHorizontalPushPerFrame, 0.05f, 2.0f);
			ImGui::Checkbox("上面着地を有効", &move_.obstacleTopLandingEnabled);
			ImGui::SliderFloat("上面着地高さ許容", &move_.obstacleTopLandingTolerance, 0.01f, 1.5f);
			ImGui::SliderFloat("上面着地最大高さ", &move_.obstacleTopLandingMaxHeight, 0.1f, 8.0f);
			ImGui::SliderFloat("上面着地最小重なり", &move_.obstacleTopLandingMinHorizontalOverlap, 0.01f, 1.0f);
		}
		if (ImGui::CollapsingHeader("個体間分離", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Checkbox("個体間分離を使う", &separationSettings_.enabled);
			ImGui::SliderFloat("分離半径", &separationSettings_.radius, 0.2f, 4.0f);
			ImGui::SliderFloat("分離強度", &separationSettings_.strength, 0.0f, 2.0f);
			ImGui::SliderFloat("1フレーム最大押し出し", &separationSettings_.maxPushPerFrame, 0.01f, 0.6f);
			ImGui::SliderFloat("攻撃中の押し出し倍率", &separationSettings_.attackPushScale, 0.0f, 1.0f);
			ImGui::Checkbox("ターゲット付近の横ずれ補正", &separationSettings_.targetNearLateralEnabled);
			ImGui::SliderFloat("横ずれ補正の強さ", &separationSettings_.targetNearLateralStrength, 0.0f, 1.5f);
			ImGui::SliderFloat("横ずれ補正距離", &separationSettings_.targetNearLateralOffset, 0.0f, 1.0f);
			ImGui::Text("重なっている敵数: %d", separationState_.overlappingEnemyCount);
			ImGui::Text("最後の分離押し出し量: (%.3f, %.3f, %.3f)", separationState_.lastSeparationPush.x, separationState_.lastSeparationPush.y, separationState_.lastSeparationPush.z);
		}
		if (ImGui::CollapsingHeader("ジャンプ", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Checkbox("ジャンプを使う", &jump_.enabled);
			ImGui::SliderFloat("ジャンプ力", &jump_.baseVelocity, 2.0f, 18.0f);
			ImGui::SliderFloat("ジャンプ最大速度", &jump_.maxVelocity, 2.0f, 28.0f);
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
			ImGui::SliderFloat("再探索ターゲット閾値", &pathSettings_.targetRepathThreshold, 0.1f, 8.0f);
			ImGui::SliderFloat("スタック再探索拡張", &pathSettings_.stuckRepathExpandBonus, 0.0f, 3.0f);
			ImGui::SliderFloat("スタック再探索拡張最大", &pathSettings_.maxStuckRepathExpandBonus, 0.0f, 6.0f);
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
			ImGui::SliderFloat("乗り越え水平距離上限", &traversal_.climbHorizontalDistanceMax, 0.5f, 10.0f);
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
		if (ImGui::CollapsingHeader("攻撃選択", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Checkbox("ランダム攻撃選択を使う", &attackSelectSettings_.randomSelectEnabled);
			ImGui::SliderFloat("踏み込みひっかき基本確率", &attackSelectSettings_.lungeBaseChance, 0.0f, 1.0f);
			ImGui::SliderFloat("踏み込みひっかき優先確率", &attackSelectSettings_.lungePreferredChance, 0.0f, 1.0f);
			ImGui::SliderFloat("踏み込み優先最小距離", &attackSelectSettings_.lungePreferredMinDistance, 0.1f, 8.0f);
			ImGui::SliderFloat("踏み込み優先最大距離", &attackSelectSettings_.lungePreferredMaxDistance, 0.1f, 10.0f);
			ImGui::Text("最後の乱数: %.3f", attackSelectState_.lastRoll);
			ImGui::Text("最後の踏み込み確率: %.3f", attackSelectState_.lastLungeChance);
			ImGui::Text("最後の攻撃選択理由: %s", attackSelectState_.lastReason.c_str());
			const char* attackAnimName = "なし";
			if (attackController_.IsAttacking())
			{
				attackAnimName = (attackController_.GetCurrentAttackType() == MeleeAttackType::LungeScratch) ? "踏み込みひっかき" : "ひっかき";
			}
			ImGui::Text("現在攻撃アニメーション: %s", attackAnimName);
			ImGui::Text("攻撃進行度: %.2f", attackController_.GetCurrentAttackNormalizedTime());
		}
		if (ImGui::CollapsingHeader("攻撃パターン", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::SliderFloat("攻撃ロック時間", &attackSettings_.lockTime, 0.0f, 1.0f);
			int attackSelect = static_cast<int>(attackSettings_.selectedAttackType);
			const char* items[] = { "ひっかき", "踏み込みひっかき" };
			if (ImGui::Combo("選択攻撃", &attackSelect, items, IM_ARRAYSIZE(items))) { attackSettings_.selectedAttackType = static_cast<MeleeAttackType>(attackSelect); }
			if (MeleeAttackPattern* scratch = attackController_.FindPattern(MeleeAttackType::Scratch)) { MeleeAttackStep& st = scratch->steps[0]; ImGui::SliderInt("ひっかき ダメージ", &st.damage, 1, 50); ImGui::SliderFloat("ひっかき 射程", &st.range, 0.5f, 6.0f); ImGui::SliderFloat("ひっかき 半径", &st.radius, 0.1f, 3.0f); ImGui::SliderFloat("ひっかき 開始", &st.startTime, 0.01f, 1.5f); ImGui::SliderFloat("ひっかき 有効", &st.activeTime, 0.01f, 1.0f); ImGui::SliderFloat("ひっかき 硬直", &scratch->recoveryTime, 0.01f, 2.0f); ImGui::SliderFloat("ひっかき CT", &scratch->cooldown, 0.01f, 3.0f); }
			if (MeleeAttackPattern* lunge = attackController_.FindPattern(MeleeAttackType::LungeScratch)) { MeleeAttackStep& st = lunge->steps[0]; ImGui::SliderInt("踏み込みひっかき ダメージ", &st.damage, 1, 50); ImGui::SliderFloat("踏み込みひっかき 射程", &st.range, 0.5f, 8.0f); ImGui::SliderFloat("踏み込みひっかき 半径", &st.radius, 0.1f, 3.0f); ImGui::SliderFloat("踏み込みひっかき 開始", &st.startTime, 0.01f, 2.0f); ImGui::SliderFloat("踏み込みひっかき 有効", &st.activeTime, 0.01f, 1.2f); ImGui::SliderFloat("踏み込みひっかき 前進速度", &lunge->forwardMoveSpeed, 0.0f, 8.0f); ImGui::SliderFloat("踏み込みひっかき 前進時間", &lunge->forwardMoveDuration, 0.0f, 2.0f); ImGui::SliderFloat("踏み込みひっかき 硬直", &lunge->recoveryTime, 0.01f, 3.0f); ImGui::SliderFloat("踏み込みひっかき CT", &lunge->cooldown, 0.01f, 4.0f); }
		}
		if (ImGui::CollapsingHeader("アニメーション", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::SliderFloat("歩行速度", &animation_.walkAnimSpeed, 1.0f, 18.0f);
			ImGui::SliderFloat("腕振り", &animation_.walkArmSwing, 0.0f, 1.5f);
			ImGui::SliderFloat("脚振り", &animation_.walkLegSwing, 0.0f, 1.5f);
			// 通常ひっかきは 構え→振り下ろし→戻り の3段階パラメータを個別調整する。
			ImGui::SliderFloat("通常ひっかき 構え腕X", &animation_.scratch.prepareArmX, -2.5f, 2.5f);
			ImGui::SliderFloat("通常ひっかき 構え腕Y", &animation_.scratch.prepareArmY, -2.5f, 2.5f);
			ImGui::SliderFloat("通常ひっかき 構え腕Z", &animation_.scratch.prepareArmZ, -2.5f, 2.5f);
			ImGui::SliderFloat("通常ひっかき 振り下ろし腕X", &animation_.scratch.strikeArmX, -2.5f, 2.5f);
			ImGui::SliderFloat("通常ひっかき 振り下ろし腕Y", &animation_.scratch.strikeArmY, -2.5f, 2.5f);
			ImGui::SliderFloat("通常ひっかき 振り下ろし腕Z", &animation_.scratch.strikeArmZ, -2.5f, 2.5f);
			ImGui::SliderFloat("通常ひっかき 構え終了割合", &animation_.scratch.prepareEndRate, 0.05f, 0.9f);
			ImGui::SliderFloat("通常ひっかき 振り終了割合", &animation_.scratch.strikeEndRate, 0.1f, 0.98f);
			ImGui::SliderFloat("通常ひっかき 構え体傾き", &animation_.scratch.bodyPrepareLean, -0.6f, 0.6f);
			ImGui::SliderFloat("通常ひっかき 振り体傾き", &animation_.scratch.bodyStrikeLean, -0.6f, 0.6f);
			ImGui::SliderFloat("通常ひっかき戻り速度", &animation_.scratch.returnSpeed, 1.0f, 30.0f);
			// 踏み込みひっかきは振りかぶり/振り下ろしを段階的に調整できるようにする。
			ImGui::SliderFloat("踏み込み 構え腕X", &animation_.lunge.prepareArmX, -2.5f, 2.5f);
			ImGui::SliderFloat("踏み込み 構え腕Y", &animation_.lunge.prepareArmY, -2.5f, 2.5f);
			ImGui::SliderFloat("踏み込み 構え腕Z", &animation_.lunge.prepareArmZ, -2.5f, 2.5f);
			ImGui::SliderFloat("踏み込み 振り下ろし腕X", &animation_.lunge.strikeArmX, -2.5f, 2.5f);
			ImGui::SliderFloat("踏み込み 振り下ろし腕Y", &animation_.lunge.strikeArmY, -2.5f, 2.5f);
			ImGui::SliderFloat("踏み込み 振り下ろし腕Z", &animation_.lunge.strikeArmZ, -2.5f, 2.5f);
			ImGui::SliderFloat("踏み込み 構え体傾き", &animation_.lunge.bodyPrepareLean, -0.8f, 0.8f);
			ImGui::SliderFloat("踏み込み 振り体傾き", &animation_.lunge.bodyStrikeLean, -0.8f, 0.8f);
			ImGui::SliderFloat("踏み込み 構え終了割合", &animation_.lunge.prepareEndRate, 0.05f, 0.9f);
			ImGui::SliderFloat("踏み込み 振り終了割合", &animation_.lunge.strikeEndRate, 0.1f, 0.98f);
			ImGui::SliderFloat("踏み込み 戻り速度", &animation_.lunge.returnSpeed, 1.0f, 24.0f);
			ImGui::SliderFloat("踏み込み 脚の踏み込み量", &animation_.lunge.legStepAmount, 0.0f, 0.8f);
			const float p = attackController_.GetCurrentAttackNormalizedTime();
			const bool isScratchAttack = attackController_.IsAttacking() && attackController_.GetCurrentAttackType() == MeleeAttackType::Scratch;
			const float prepareEnd = isScratchAttack ? animation_.scratch.prepareEndRate : animation_.lunge.prepareEndRate;
			const float strikeEnd = isScratchAttack ? animation_.scratch.strikeEndRate : animation_.lunge.strikeEndRate;
			const char* phase = !attackController_.IsAttacking() ? "なし" : (p < prepareEnd ? "構え" : (p < strikeEnd ? "振り下ろし" : "戻り"));
			ImGui::Text("現在攻撃: %s", attackController_.IsAttacking() ? ((attackController_.GetCurrentAttackType() == MeleeAttackType::LungeScratch) ? "踏み込みひっかき" : "通常ひっかき") : "なし");
			ImGui::Text("攻撃進行度: %.2f", animationState_.attackAnimProgress);
			ImGui::Text("現在フェーズ: %s", phase);
			ImGui::Text("Scratch使用腕: %s", scratchArmState_.useLeftArm ? "左" : "右");
		}
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
		// 距離と確率に応じて攻撃を選び、通常ひっかきと踏み込みひっかきを使い分ける
		const MeleeAttackType selectedType = SelectAttackTypeByDistanceAndChance(GetDistanceToTarget());
		attackSettings_.selectedAttackType = selectedType;
		attackController_.StartAttack(selectedType);
		attackState_.lockTimer = attackSettings_.lockTime;
		animationState_.animState = (selectedType == MeleeAttackType::LungeScratch) ? AnimState::LungeScratch : AnimState::Scratch;
	}
	currentBehaviorName_ = (attackSettings_.selectedAttackType == MeleeAttackType::LungeScratch) ? "LungeScratchAttack" : "ScratchAttack";
}

MeleeAttackType MeleeEnemy::SelectAttackTypeByDistanceAndChance(float distance)
{
	const auto* lungePattern = attackController_.FindPattern(MeleeAttackType::LungeScratch);
	if (!lungePattern || !attackController_.CanStartAttack())
	{
		attackSelectState_.lastReason = "踏み込み不可";
		return MeleeAttackType::Scratch;
	}
	float lungeChance = attackSelectSettings_.lungeBaseChance;
	const bool preferredDistance = distance >= attackSelectSettings_.lungePreferredMinDistance && distance <= attackSelectSettings_.lungePreferredMaxDistance;
	if (preferredDistance) { lungeChance = attackSelectSettings_.lungePreferredChance; }
	lungeChance = Clamp(lungeChance, 0.0f, 1.0f);
	attackSelectState_.lastLungeChance = lungeChance;
	if (!attackSelectSettings_.randomSelectEnabled)
	{
		attackSelectState_.lastRoll = 0.0f;
		attackSelectState_.lastReason = preferredDistance ? "距離優先で踏み込み" : "近距離でひっかき";
		return preferredDistance ? MeleeAttackType::LungeScratch : MeleeAttackType::Scratch;
	}
	attackSelectState_.lastRoll = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
	const bool chooseLunge = preferredDistance ? (attackSelectState_.lastRoll < lungeChance) : (attackSelectState_.lastRoll < attackSelectSettings_.lungeBaseChance);
	attackSelectState_.lastReason = chooseLunge ? "乱数で踏み込みひっかき" : "乱数でひっかき";
	return chooseLunge ? MeleeAttackType::LungeScratch : MeleeAttackType::Scratch;
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
	if (attackController_.GetCurrentAttackType() == MeleeAttackType::LungeScratch && GetDistanceToTarget() > detection_.minLungeForwardDistance)
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
	Vector3 lAttack{};
	Vector3 rAttack{};
	float bodyLean = 0.0f;
	animationState_.attackAnimProgress = 0.0f;
	if (attackController_.IsAttacking())
	{
		animationState_.attackAnimProgress = attackController_.GetCurrentAttackNormalizedTime();
		const bool isScratch = attackController_.GetCurrentAttackType() == MeleeAttackType::Scratch;
		// Scratch攻撃の開始フレームでだけ使用腕を切り替えて固定する
		if (isScratch && !scratchArmState_.wasScratchAttacking)
		{
			scratchArmState_.useLeftArm = !scratchArmState_.useLeftArm;
		}
		scratchArmState_.wasScratchAttacking = isScratch;
		if (isScratch)
		{
			// 通常ひっかきは 構え→振り下ろし→戻り の3段階で片腕だけを動かす。
			const float p = attackController_.GetCurrentAttackNormalizedTime();
			const float prepareEnd = std::clamp(animation_.scratch.prepareEndRate, 0.05f, 0.95f);
			const float strikeEnd = std::clamp(std::max(animation_.scratch.strikeEndRate, prepareEnd + 0.01f), prepareEnd + 0.01f, 0.99f);
			Vector3 scratchAngles{};
			if (p < prepareEnd)
			{
				const float t = p / std::max(prepareEnd, 0.0001f);
				scratchAngles = { animation_.scratch.prepareArmX * t, animation_.scratch.prepareArmY * t, animation_.scratch.prepareArmZ * t };
				bodyLean = animation_.scratch.bodyPrepareLean * t;
			}
			else if (p < strikeEnd)
			{
				const float t = (p - prepareEnd) / std::max(strikeEnd - prepareEnd, 0.0001f);
				scratchAngles = {
					animation_.scratch.prepareArmX + (animation_.scratch.strikeArmX - animation_.scratch.prepareArmX) * t,
					animation_.scratch.prepareArmY + (animation_.scratch.strikeArmY - animation_.scratch.prepareArmY) * t,
					animation_.scratch.prepareArmZ + (animation_.scratch.strikeArmZ - animation_.scratch.prepareArmZ) * t
				};
				bodyLean = animation_.scratch.bodyPrepareLean + (animation_.scratch.bodyStrikeLean - animation_.scratch.bodyPrepareLean) * t;
			}
			else
			{
				const float t = (p - strikeEnd) / std::max(1.0f - strikeEnd, 0.0001f);
				scratchAngles = {
					animation_.scratch.strikeArmX * (1.0f - t),
					animation_.scratch.strikeArmY * (1.0f - t),
					animation_.scratch.strikeArmZ * (1.0f - t)
				};
				bodyLean = animation_.scratch.bodyStrikeLean * (1.0f - t);
			}
			if (scratchArmState_.useLeftArm) { lAttack = scratchAngles; }
			else { rAttack = scratchAngles; }
		}
		if (attackController_.GetCurrentAttackType() == MeleeAttackType::LungeScratch)
		{
			// 踏み込みひっかきは両腕で溜め→振り下ろし→復帰を進行度で表現する。
			const float p = attackController_.GetCurrentAttackNormalizedTime();
			const float prepareEnd = std::clamp(animation_.lunge.prepareEndRate, 0.05f, 0.95f);
			const float strikeEnd = std::clamp(std::max(animation_.lunge.strikeEndRate, prepareEnd + 0.01f), prepareEnd + 0.01f, 0.99f);
			Vector3 lungeAngles{};
			if (p < prepareEnd)
			{
				const float t = p / std::max(prepareEnd, 0.0001f);
				lungeAngles = {
					animation_.lunge.prepareArmX * t,
					animation_.lunge.prepareArmY * t,
					animation_.lunge.prepareArmZ * t
				};
				bodyLean = animation_.lunge.bodyPrepareLean * t;
			}
			else if (p < strikeEnd)
			{
				const float t = (p - prepareEnd) / std::max(strikeEnd - prepareEnd, 0.0001f);
				lungeAngles = {
					animation_.lunge.prepareArmX + (animation_.lunge.strikeArmX - animation_.lunge.prepareArmX) * t,
					animation_.lunge.prepareArmY + (animation_.lunge.strikeArmY - animation_.lunge.prepareArmY) * t,
					animation_.lunge.prepareArmZ + (animation_.lunge.strikeArmZ - animation_.lunge.prepareArmZ) * t
				};
				bodyLean = animation_.lunge.bodyPrepareLean + (animation_.lunge.bodyStrikeLean - animation_.lunge.bodyPrepareLean) * t;
			}
			else
			{
				const float t = (p - strikeEnd) / std::max(1.0f - strikeEnd, 0.0001f);
				lungeAngles = {
					animation_.lunge.strikeArmX * (1.0f - t),
					animation_.lunge.strikeArmY * (1.0f - t),
					animation_.lunge.strikeArmZ * (1.0f - t)
				};
				bodyLean = animation_.lunge.bodyStrikeLean * (1.0f - t);
			}
			lAttack = lungeAngles;
			rAttack = lungeAngles;
			legTarget = animation_.lunge.legStepAmount * std::sin(std::clamp(p, 0.0f, 1.0f) * kPi);
		}
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
	const bool isLungeAttacking = attackController_.IsAttacking() && attackController_.GetCurrentAttackType() == MeleeAttackType::LungeScratch;
	// 攻撃種類ごとに復帰速度を切り替える。
	const float returnSpeed = isLungeAttacking ? animation_.lunge.returnSpeed : animation_.scratch.returnSpeed;
	const float ret = std::min(1.0f, returnSpeed * deltaTime);
	parts_[lArm].transform.rotate_.x += ((armTarget - lAttack.x) - parts_[lArm].transform.rotate_.x) * ret;
	parts_[lArm].transform.rotate_.y += ((-lAttack.y) - parts_[lArm].transform.rotate_.y) * ret;
	parts_[lArm].transform.rotate_.z += ((-lAttack.z) - parts_[lArm].transform.rotate_.z) * ret;
	parts_[rArm].transform.rotate_.x += ((-armTarget - rAttack.x) - parts_[rArm].transform.rotate_.x) * ret;
	parts_[rArm].transform.rotate_.y += ((rAttack.y) - parts_[rArm].transform.rotate_.y) * ret;
	parts_[rArm].transform.rotate_.z += ((rAttack.z) - parts_[rArm].transform.rotate_.z) * ret;
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
			if (pathDetailDebugDrawEnabled_ && HasTarget())
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
	// 攻撃の種類に応じた体傾きを適用する。
	body_.transform.rotate_.x = bodyLean;
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
	case AnimState::LungeScratch: return "LungeScratch";
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
		// 新形式カテゴリJSONを優先し、旧flat形式も後方互換で読む。
		const nlohmann::json& detectionJ = j.contains("detection") ? j["detection"] : j;
		const nlohmann::json& moveJ = j.contains("move") ? j["move"] : j;
		const nlohmann::json& jumpJ = j.contains("jump") ? j["jump"] : j;
		const nlohmann::json& traversalJ = j.contains("traversal") ? j["traversal"] : j;
		const nlohmann::json& pathJ = j.contains("path") ? j["path"] : j;
		const nlohmann::json& stuckJ = j.contains("stuck") ? j["stuck"] : j;
		const nlohmann::json& attackJ = j.contains("attack") ? j["attack"] : j;
		const nlohmann::json& animationJ = j.contains("animation") ? j["animation"] : j;
		const nlohmann::json& headLookJ = j.contains("headLook") ? j["headLook"] : j;
		const nlohmann::json& separationJ = j.contains("separation") ? j["separation"] : j;
		const nlohmann::json& attackPatternsJ = j.contains("attackPatterns") ? j["attackPatterns"] : j;
		const nlohmann::json& scratchJ = attackPatternsJ.contains("scratch") ? attackPatternsJ["scratch"] : j;
		const nlohmann::json& lungeScratchJ = attackPatternsJ.contains("lungeScratch") ? attackPatternsJ["lungeScratch"] : (attackPatternsJ.contains("oneTwo") ? attackPatternsJ["oneTwo"] : j);

		detection_.detectRange = detectionJ.value("detectRange", detection_.detectRange);
		detection_.meleeAttackRange = detectionJ.value("meleeAttackRange", detection_.meleeAttackRange);
		detection_.stopDistance = detectionJ.value("stopDistance", detection_.stopDistance);
		detection_.attackStartRange = detectionJ.value("attackStartRange", detection_.attackStartRange);
		detection_.resumeChaseDistance = detectionJ.value("resumeChaseDistance", detection_.resumeChaseDistance);
		detection_.minLungeForwardDistance = detectionJ.value("minLungeForwardDistance", detection_.minLungeForwardDistance);
		move_.moveSpeed = moveJ.value("moveSpeed", move_.moveSpeed);
		move_.rotateSpeed = moveJ.value("rotateSpeed", move_.rotateSpeed);
		move_.maxResolvePushPerFrame = moveJ.value("maxResolvePushPerFrame", move_.maxResolvePushPerFrame);
		move_.maxHorizontalPushPerFrame = moveJ.value("maxHorizontalPushPerFrame", move_.maxHorizontalPushPerFrame);
		move_.obstacleTopLandingEnabled = moveJ.value("obstacleTopLandingEnabled", move_.obstacleTopLandingEnabled);
		move_.obstacleTopLandingTolerance = moveJ.value("obstacleTopLandingTolerance", move_.obstacleTopLandingTolerance);
		move_.obstacleTopLandingMaxHeight = moveJ.value("obstacleTopLandingMaxHeight", move_.obstacleTopLandingMaxHeight);
		move_.obstacleTopLandingMinHorizontalOverlap = moveJ.value("obstacleTopLandingMinHorizontalOverlap", move_.obstacleTopLandingMinHorizontalOverlap);
		jump_.enabled = jumpJ.value("enabled", jumpJ.value("jumpEnabled", jump_.enabled));
		jump_.baseVelocity = jumpJ.value("baseVelocity", jumpJ.value("jumpBaseVelocity", jump_.baseVelocity));
		jump_.maxVelocity = jumpJ.value("maxVelocity", jump_.maxVelocity);
		jump_.cooldown = jumpJ.value("cooldown", jumpJ.value("jumpCooldown", jump_.cooldown));
		traversal_.enabled = traversalJ.value("enabled", traversalJ.value("traversalEnabled", traversal_.enabled));
		traversal_.preferDirectClimb = traversalJ.value("preferDirectClimb", traversalJ.value("traversalPrioritizeDirectClimb", traversal_.preferDirectClimb));
		traversal_.maxClimbHeight = traversalJ.value("maxClimbHeight", traversalJ.value("traversalMaxClimbHeight", traversal_.maxClimbHeight));
		traversal_.minClimbHeight = traversalJ.value("minClimbHeight", traversalJ.value("traversalMinClimbHeight", traversal_.minClimbHeight));
		traversal_.maxClimbObstacleWidth = traversalJ.value("maxClimbObstacleWidth", traversalJ.value("traversalMaxClimbObstacleWidth", traversal_.maxClimbObstacleWidth));
		traversal_.maxClimbObstacleDepth = traversalJ.value("maxClimbObstacleDepth", traversalJ.value("traversalMaxClimbObstacleDepth", traversal_.maxClimbObstacleDepth));
		traversal_.climbJumpTriggerDistance = traversalJ.value("climbJumpTriggerDistance", traversalJ.value("traversalClimbJumpTriggerDistance", traversal_.climbJumpTriggerDistance));
		traversal_.climbHorizontalDistanceMax = traversalJ.value("climbHorizontalDistanceMax", traversalJ.value("traversalClimbHorizontalDistanceMax", traversal_.climbHorizontalDistanceMax));
		traversal_.directClimbDistanceMax = traversalJ.value("directClimbDistanceMax", traversalJ.value("traversalDirectClimbMaxTargetDistance", traversal_.directClimbDistanceMax));
		traversal_.directLineWidth = traversalJ.value("directLineWidth", traversalJ.value("traversalDirectClimbLineWidth", traversal_.directLineWidth));
		traversal_.allowJumpOverLowObstacles = traversalJ.value("allowJumpOverLowObstacles", traversalJ.value("traversalAllowJumpOverLowObstacles", traversal_.allowJumpOverLowObstacles));
		pathSettings_.enabled = pathJ.value("enabled", pathJ.value("pathFindEnabled", pathSettings_.enabled));
		pathSettings_.repathInterval = pathJ.value("repathInterval", pathSettings_.repathInterval);
		pathSettings_.waypointReachDistance = pathJ.value("waypointReachDistance", pathSettings_.waypointReachDistance);
		pathSettings_.gridSize = pathJ.value("gridSize", pathJ.value("pathGridSize", pathSettings_.gridSize));
		pathSettings_.searchRadius = pathJ.value("searchRadius", pathJ.value("pathSearchRadius", pathSettings_.searchRadius));
		pathSettings_.obstacleExpandRadius = pathJ.value("obstacleExpandRadius", pathSettings_.obstacleExpandRadius);
		pathSettings_.temporaryBlockDuration = pathJ.value("temporaryBlockDuration", pathSettings_.temporaryBlockDuration);
		pathSettings_.temporaryBlockRadius = pathJ.value("temporaryBlockRadius", pathSettings_.temporaryBlockRadius);
		pathSettings_.cornerCuttingDisabled = pathJ.value("cornerCuttingDisabled", pathSettings_.cornerCuttingDisabled);
		pathSettings_.targetRepathThreshold = pathJ.value("targetRepathThreshold", pathSettings_.targetRepathThreshold);
		pathSettings_.stuckRepathExpandBonus = pathJ.value("stuckRepathExpandBonus", pathSettings_.stuckRepathExpandBonus);
		pathSettings_.maxStuckRepathExpandBonus = pathJ.value("maxStuckRepathExpandBonus", pathSettings_.maxStuckRepathExpandBonus);
		stuckSettings_.checkTime = stuckJ.value("checkTime", stuckJ.value("stuckCheckTime", stuckSettings_.checkTime));
		stuckSettings_.distance = stuckJ.value("distance", stuckJ.value("stuckDistance", stuckSettings_.distance));
		stuckSettings_.moveThreshold = stuckJ.value("moveThreshold", stuckJ.value("stuckMoveThreshold", stuckSettings_.moveThreshold));
		attackSettings_.lockTime = attackJ.value("lockTime", attackJ.value("attackLockTime", attackSettings_.lockTime));
		attackSettings_.selectedAttackType = static_cast<MeleeAttackType>(attackJ.value("selectedAttackType", static_cast<int>(attackSettings_.selectedAttackType)));
		attackSelectSettings_.randomSelectEnabled = attackJ.value("randomSelectEnabled", attackSelectSettings_.randomSelectEnabled);
		attackSelectSettings_.lungeBaseChance = attackJ.value("lungeBaseChance", attackSelectSettings_.lungeBaseChance);
		attackSelectSettings_.lungePreferredChance = attackJ.value("lungePreferredChance", attackSelectSettings_.lungePreferredChance);
		attackSelectSettings_.lungePreferredMinDistance = attackJ.value("lungePreferredMinDistance", attackSelectSettings_.lungePreferredMinDistance);
		attackSelectSettings_.lungePreferredMaxDistance = attackJ.value("lungePreferredMaxDistance", attackSelectSettings_.lungePreferredMaxDistance);
		animation_.visualYawOffset = animationJ.value("visualYawOffset", animation_.visualYawOffset);
		animation_.walkAnimSpeed = animationJ.value("walkAnimSpeed", animation_.walkAnimSpeed);
		animation_.walkArmSwing = animationJ.value("walkArmSwing", animation_.walkArmSwing);
		animation_.walkLegSwing = animationJ.value("walkLegSwing", animation_.walkLegSwing);
		// 旧キー互換を維持しつつ、通常ひっかきと踏み込みひっかきの詳細設定を読み込む。
		animation_.scratch.prepareArmX = animationJ.value("scratchPrepareArmX", animationJ.value("scratchArmX", animation_.scratch.prepareArmX));
		animation_.scratch.prepareArmY = animationJ.value("scratchPrepareArmY", animationJ.value("scratchArmY", animation_.scratch.prepareArmY));
		animation_.scratch.prepareArmZ = animationJ.value("scratchPrepareArmZ", animationJ.value("scratchArmZ", animation_.scratch.prepareArmZ));
		animation_.scratch.strikeArmX = animationJ.value("scratchStrikeArmX", animationJ.value("scratchArmSwing", animation_.scratch.strikeArmX));
		animation_.scratch.strikeArmY = animationJ.value("scratchStrikeArmY", animation_.scratch.strikeArmY);
		animation_.scratch.strikeArmZ = animationJ.value("scratchStrikeArmZ", animation_.scratch.strikeArmZ);
		animation_.scratch.prepareEndRate = animationJ.value("scratchPrepareEndRate", animation_.scratch.prepareEndRate);
		animation_.scratch.strikeEndRate = animationJ.value("scratchStrikeEndRate", animation_.scratch.strikeEndRate);
		animation_.scratch.bodyPrepareLean = animationJ.value("scratchBodyPrepareLean", animationJ.value("scratchBodyLean", animation_.scratch.bodyPrepareLean));
		animation_.scratch.bodyStrikeLean = animationJ.value("scratchBodyStrikeLean", animation_.scratch.bodyStrikeLean);
		animation_.scratch.returnSpeed = animationJ.value("scratchReturnSpeed", animation_.scratch.returnSpeed);
		animation_.lunge.prepareArmX = animationJ.value("lungePrepareArmX", animationJ.value("lungeRaiseArmAngle", animation_.lunge.prepareArmX));
		animation_.lunge.prepareArmY = animationJ.value("lungePrepareArmY", animation_.lunge.prepareArmY);
		animation_.lunge.prepareArmZ = animationJ.value("lungePrepareArmZ", animation_.lunge.prepareArmZ);
		animation_.lunge.strikeArmX = animationJ.value("lungeStrikeArmX", animationJ.value("lungeSwingArmX", animationJ.value("lungeSwingDownAngle", animation_.lunge.strikeArmX)));
		animation_.lunge.strikeArmY = animationJ.value("lungeStrikeArmY", animationJ.value("lungeSwingArmY", animation_.lunge.strikeArmY));
		animation_.lunge.strikeArmZ = animationJ.value("lungeStrikeArmZ", animationJ.value("lungeSwingArmZ", animation_.lunge.strikeArmZ));
		animation_.lunge.bodyPrepareLean = animationJ.value("lungeBodyPrepareLean", animation_.lunge.bodyPrepareLean);
		animation_.lunge.bodyStrikeLean = animationJ.value("lungeBodyStrikeLean", animationJ.value("lungeBodySwingLean", animationJ.value("lungeBodyLean", animation_.lunge.bodyStrikeLean)));
		animation_.lunge.prepareEndRate = animationJ.value("lungePrepareEndRate", animation_.lunge.prepareEndRate);
		animation_.lunge.strikeEndRate = animationJ.value("lungeStrikeEndRate", animationJ.value("lungeSwingEndRate", animation_.lunge.strikeEndRate));
		animation_.lunge.returnSpeed = animationJ.value("lungeReturnSpeed", animation_.lunge.returnSpeed);
		animation_.lunge.legStepAmount = animationJ.value("lungeLegStepAmount", animation_.lunge.legStepAmount);
		headLookSettings_.enabled = headLookJ.value("enabled", headLookJ.value("headLookEnabled", headLookSettings_.enabled));
		headLookSettings_.yawLimitDeg = headLookJ.value("yawLimitDeg", headLookJ.value("headYawLimitDeg", headLookSettings_.yawLimitDeg));
		headLookSettings_.pitchMinDeg = headLookJ.value("pitchMinDeg", headLookJ.value("headPitchMinDeg", headLookSettings_.pitchMinDeg));
		headLookSettings_.pitchMaxDeg = headLookJ.value("pitchMaxDeg", headLookJ.value("headPitchMaxDeg", headLookSettings_.pitchMaxDeg));
		headLookSettings_.lerpSpeed = headLookJ.value("lerpSpeed", headLookJ.value("headLookLerpSpeed", headLookSettings_.lerpSpeed));
		separationSettings_.enabled = separationJ.value("enabled", separationSettings_.enabled);
		separationSettings_.radius = separationJ.value("radius", separationSettings_.radius);
		separationSettings_.strength = separationJ.value("strength", separationSettings_.strength);
		separationSettings_.maxPushPerFrame = separationJ.value("maxPushPerFrame", separationSettings_.maxPushPerFrame);
		separationSettings_.attackPushScale = separationJ.value("attackPushScale", separationSettings_.attackPushScale);
		separationSettings_.targetNearLateralEnabled = separationJ.value("targetNearLateralEnabled", separationSettings_.targetNearLateralEnabled);
		separationSettings_.targetNearLateralOffset = separationJ.value("targetNearLateralOffset", separationSettings_.targetNearLateralOffset);
		separationSettings_.targetNearLateralStrength = separationJ.value("targetNearLateralStrength", separationSettings_.targetNearLateralStrength);
		if (auto* s = attackController_.FindPattern(MeleeAttackType::Scratch))
		{
			auto& st = s->steps[0];
			st.damage = scratchJ.value("damage", scratchJ.value("scratchDamage", st.damage));
			st.range = scratchJ.value("range", scratchJ.value("scratchRange", st.range));
			st.radius = scratchJ.value("radius", scratchJ.value("scratchRadius", st.radius));
			st.startTime = scratchJ.value("startTime", scratchJ.value("scratchStartTime", st.startTime));
			st.activeTime = scratchJ.value("activeTime", scratchJ.value("scratchActiveTime", st.activeTime));
			s->recoveryTime = scratchJ.value("recoveryTime", scratchJ.value("scratchRecoveryTime", s->recoveryTime));
			s->cooldown = scratchJ.value("cooldown", scratchJ.value("scratchCooldown", s->cooldown));
		}
		if (auto* o = attackController_.FindPattern(MeleeAttackType::LungeScratch))
		{
			auto& st = o->steps[0];
			st.damage = lungeScratchJ.value("damage", lungeScratchJ.value("leftDamage", lungeScratchJ.value("oneTwoLeftDamage", st.damage)));
			st.range = lungeScratchJ.value("range", lungeScratchJ.value("leftRange", lungeScratchJ.value("oneTwoLeftRange", st.range)));
			st.radius = lungeScratchJ.value("radius", lungeScratchJ.value("leftRadius", lungeScratchJ.value("oneTwoLeftRadius", st.radius)));
			st.startTime = lungeScratchJ.value("startTime", lungeScratchJ.value("leftStartTime", lungeScratchJ.value("oneTwoLeftStartTime", st.startTime)));
			st.activeTime = lungeScratchJ.value("activeTime", lungeScratchJ.value("leftActiveTime", lungeScratchJ.value("oneTwoLeftActiveTime", st.activeTime)));
			o->forwardMoveSpeed = lungeScratchJ.value("forwardMoveSpeed", lungeScratchJ.value("oneTwoForwardMoveSpeed", o->forwardMoveSpeed));
			o->forwardMoveDuration = lungeScratchJ.value("forwardMoveDuration", lungeScratchJ.value("oneTwoForwardMoveDuration", o->forwardMoveDuration));
			o->recoveryTime = lungeScratchJ.value("recoveryTime", lungeScratchJ.value("oneTwoRecoveryTime", o->recoveryTime));
			o->cooldown = lungeScratchJ.value("cooldown", lungeScratchJ.value("oneTwoCooldown", o->cooldown));
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
		j["jsonVersion"] = tuningIo_.jsonFormatVersion;
		j["detection"] = {
			{ "detectRange", detection_.detectRange }, { "meleeAttackRange", detection_.meleeAttackRange }, { "stopDistance", detection_.stopDistance },
			{ "attackStartRange", detection_.attackStartRange }, { "resumeChaseDistance", detection_.resumeChaseDistance }, { "minLungeForwardDistance", detection_.minLungeForwardDistance }
		};
		j["move"] = {
			{ "moveSpeed", move_.moveSpeed }, { "rotateSpeed", move_.rotateSpeed }, { "maxResolvePushPerFrame", move_.maxResolvePushPerFrame }, { "maxHorizontalPushPerFrame", move_.maxHorizontalPushPerFrame },
			{ "obstacleTopLandingEnabled", move_.obstacleTopLandingEnabled }, { "obstacleTopLandingTolerance", move_.obstacleTopLandingTolerance }, { "obstacleTopLandingMaxHeight", move_.obstacleTopLandingMaxHeight },
			{ "obstacleTopLandingMinHorizontalOverlap", move_.obstacleTopLandingMinHorizontalOverlap }
		};
		j["jump"] = { { "enabled", jump_.enabled }, { "baseVelocity", jump_.baseVelocity }, { "maxVelocity", jump_.maxVelocity }, { "cooldown", jump_.cooldown } };
		j["traversal"] = {
			{ "enabled", traversal_.enabled }, { "preferDirectClimb", traversal_.preferDirectClimb }, { "maxClimbHeight", traversal_.maxClimbHeight }, { "minClimbHeight", traversal_.minClimbHeight },
			{ "maxClimbObstacleWidth", traversal_.maxClimbObstacleWidth }, { "maxClimbObstacleDepth", traversal_.maxClimbObstacleDepth }, { "climbJumpTriggerDistance", traversal_.climbJumpTriggerDistance },
			{ "climbHorizontalDistanceMax", traversal_.climbHorizontalDistanceMax }, { "directClimbDistanceMax", traversal_.directClimbDistanceMax }, { "directLineWidth", traversal_.directLineWidth },
			{ "allowJumpOverLowObstacles", traversal_.allowJumpOverLowObstacles }
		};
		j["path"] = {
			{ "enabled", pathSettings_.enabled }, { "repathInterval", pathSettings_.repathInterval }, { "waypointReachDistance", pathSettings_.waypointReachDistance }, { "gridSize", pathSettings_.gridSize },
			{ "searchRadius", pathSettings_.searchRadius }, { "obstacleExpandRadius", pathSettings_.obstacleExpandRadius }, { "temporaryBlockDuration", pathSettings_.temporaryBlockDuration },
			{ "temporaryBlockRadius", pathSettings_.temporaryBlockRadius }, { "cornerCuttingDisabled", pathSettings_.cornerCuttingDisabled }, { "targetRepathThreshold", pathSettings_.targetRepathThreshold },
			{ "stuckRepathExpandBonus", pathSettings_.stuckRepathExpandBonus }, { "maxStuckRepathExpandBonus", pathSettings_.maxStuckRepathExpandBonus }
		};
		j["stuck"] = { { "checkTime", stuckSettings_.checkTime }, { "distance", stuckSettings_.distance }, { "moveThreshold", stuckSettings_.moveThreshold } };
		j["attack"] = { { "selectedAttackType", static_cast<int>(attackSettings_.selectedAttackType) }, { "lockTime", attackSettings_.lockTime }, { "randomSelectEnabled", attackSelectSettings_.randomSelectEnabled }, { "lungeBaseChance", attackSelectSettings_.lungeBaseChance }, { "lungePreferredChance", attackSelectSettings_.lungePreferredChance }, { "lungePreferredMinDistance", attackSelectSettings_.lungePreferredMinDistance }, { "lungePreferredMaxDistance", attackSelectSettings_.lungePreferredMaxDistance } };
		j["animation"] = {
			{ "visualYawOffset", animation_.visualYawOffset },
			{ "walkAnimSpeed", animation_.walkAnimSpeed },
			{ "walkArmSwing", animation_.walkArmSwing },
			{ "walkLegSwing", animation_.walkLegSwing },
			{ "scratchPrepareArmX", animation_.scratch.prepareArmX },
			{ "scratchPrepareArmY", animation_.scratch.prepareArmY },
			{ "scratchPrepareArmZ", animation_.scratch.prepareArmZ },
			{ "scratchStrikeArmX", animation_.scratch.strikeArmX },
			{ "scratchStrikeArmY", animation_.scratch.strikeArmY },
			{ "scratchStrikeArmZ", animation_.scratch.strikeArmZ },
			{ "scratchPrepareEndRate", animation_.scratch.prepareEndRate },
			{ "scratchStrikeEndRate", animation_.scratch.strikeEndRate },
			{ "scratchBodyPrepareLean", animation_.scratch.bodyPrepareLean },
			{ "scratchBodyStrikeLean", animation_.scratch.bodyStrikeLean },
			{ "scratchReturnSpeed", animation_.scratch.returnSpeed },
			{ "lungePrepareArmX", animation_.lunge.prepareArmX },
			{ "lungePrepareArmY", animation_.lunge.prepareArmY },
			{ "lungePrepareArmZ", animation_.lunge.prepareArmZ },
			{ "lungeStrikeArmX", animation_.lunge.strikeArmX },
			{ "lungeStrikeArmY", animation_.lunge.strikeArmY },
			{ "lungeStrikeArmZ", animation_.lunge.strikeArmZ },
			{ "lungeBodyPrepareLean", animation_.lunge.bodyPrepareLean },
			{ "lungeBodyStrikeLean", animation_.lunge.bodyStrikeLean },
			{ "lungePrepareEndRate", animation_.lunge.prepareEndRate },
			{ "lungeStrikeEndRate", animation_.lunge.strikeEndRate },
			{ "lungeReturnSpeed", animation_.lunge.returnSpeed },
			{ "lungeLegStepAmount", animation_.lunge.legStepAmount }
		};
		j["headLook"] = { { "enabled", headLookSettings_.enabled }, { "yawLimitDeg", headLookSettings_.yawLimitDeg }, { "pitchMinDeg", headLookSettings_.pitchMinDeg }, { "pitchMaxDeg", headLookSettings_.pitchMaxDeg }, { "lerpSpeed", headLookSettings_.lerpSpeed } };
		j["separation"] = { { "enabled", separationSettings_.enabled }, { "radius", separationSettings_.radius }, { "strength", separationSettings_.strength }, { "maxPushPerFrame", separationSettings_.maxPushPerFrame }, { "attackPushScale", separationSettings_.attackPushScale }, { "targetNearLateralEnabled", separationSettings_.targetNearLateralEnabled }, { "targetNearLateralOffset", separationSettings_.targetNearLateralOffset }, { "targetNearLateralStrength", separationSettings_.targetNearLateralStrength } };
		// 保存対象は設定値のみで、ランタイム状態は書き出さない。
		if (const auto* s = attackController_.FindPattern(MeleeAttackType::Scratch)) { const auto& st = s->steps[0]; j["attackPatterns"]["scratch"] = { {"damage", st.damage}, {"range", st.range}, {"radius", st.radius}, {"startTime", st.startTime}, {"activeTime", st.activeTime}, {"recoveryTime", s->recoveryTime}, {"cooldown", s->cooldown} }; }
		if (const auto* o = attackController_.FindPattern(MeleeAttackType::LungeScratch)) { const auto& st = o->steps[0]; j["attackPatterns"]["lungeScratch"] = { {"damage", st.damage}, {"range", st.range}, {"radius", st.radius}, {"startTime", st.startTime}, {"activeTime", st.activeTime}, {"forwardMoveSpeed", o->forwardMoveSpeed}, {"forwardMoveDuration", o->forwardMoveDuration}, {"recoveryTime", o->recoveryTime}, {"cooldown", o->cooldown} }; }
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
	separationSettings_ = SeparationSettings{};
	attackController_.Initialize();
	navigator_.Reset();
}
