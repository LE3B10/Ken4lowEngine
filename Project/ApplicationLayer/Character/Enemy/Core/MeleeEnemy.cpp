#define NOMINMAX
#include "MeleeEnemy.h"

#include <cmath>
#include <algorithm>
#include <cstdlib>
#include "Wireframe.h"
#include "CollisionTypeIdDef.h"

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
	const auto* obstacleAabbs = wallObstacleAABBs_ ? wallObstacleAABBs_ : GetResolvedNavigationObstacleAABBs();
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
		float pushX = 0.0f, pushZ = 0.0f;
		if (overlapX < overlapZ) { pushX = (pos.x < (o.min.x + o.max.x) * 0.5f ? -overlapX : overlapX); }
		else { pushZ = (pos.z < (o.min.z + o.max.z) * 0.5f ? -overlapZ : overlapZ); }
		pushX = std::clamp(pushX, -maxPush, maxPush);
		pushZ = std::clamp(pushZ, -maxPush, maxPush);
		pos.x += pushX;
		pos.z += pushZ;
		collision_.lastWallResolvePush.x += pushX;
		collision_.lastWallResolvePush.z += pushZ;
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
		ImGui::SliderFloat("detectRange", &detection_.detectRange, 1.0f, 50.0f);
		ImGui::SliderFloat("meleeAttackRange", &detection_.meleeAttackRange, 0.5f, 10.0f);
		ImGui::SliderFloat("moveSpeed", &move_.moveSpeed, 0.1f, 10.0f);
		ImGui::SliderFloat("stopDistance", &detection_.stopDistance, 0.5f, 6.0f);
		ImGui::SliderFloat("attackStartRange", &detection_.attackStartRange, 0.5f, 8.0f);
		ImGui::SliderFloat("resumeChaseDistance", &detection_.resumeChaseDistance, 0.5f, 10.0f);
		ImGui::SliderFloat("attackLockTime", &attackSettings_.lockTime, 0.0f, 1.0f);
		ImGui::SliderFloat("rotateSpeed", &move_.rotateSpeed, 0.1f, 20.0f);
		ImGui::Text("visualYawOffset(rad): %.3f (fixed)", animation_.visualYawOffset);
		ImGui::SliderFloat("walkAnimSpeed", &animation_.walkAnimSpeed, 1.0f, 18.0f);
		ImGui::SliderFloat("walkArmSwing", &animation_.walkArmSwing, 0.0f, 1.5f);
		ImGui::SliderFloat("walkLegSwing", &animation_.walkLegSwing, 0.0f, 1.5f);
		ImGui::SliderFloat("attackArmSwing", &animation_.attackArmSwing, 0.0f, 2.0f);
		ImGui::SliderFloat("attackReturnSpeed", &animation_.attackReturnSpeed, 1.0f, 24.0f);
		ImGui::SliderFloat("attackBodyLean", &animation_.attackBodyLean, 0.0f, 0.4f);
		ImGui::Checkbox("headLookEnabled", &headLookSettings_.enabled);
		ImGui::SliderFloat("headYawLimitDeg", &headLookSettings_.yawLimitDeg, 10.0f, 120.0f);
		ImGui::SliderFloat("headPitchMinDeg", &headLookSettings_.pitchMinDeg, -80.0f, 0.0f);
		ImGui::SliderFloat("headPitchMaxDeg", &headLookSettings_.pitchMaxDeg, 0.0f, 80.0f);
		ImGui::SliderFloat("headLookLerpSpeed", &headLookSettings_.lerpSpeed, 1.0f, 30.0f);
		ImGui::SliderFloat("maxResolvePushPerFrame", &move_.maxResolvePushPerFrame, 0.05f, 2.0f);
		ImGui::Checkbox("obstacleTopLandingEnabled", &move_.obstacleTopLandingEnabled);
		ImGui::SliderFloat("obstacleTopLandingTolerance", &move_.obstacleTopLandingTolerance, 0.01f, 1.0f);
		ImGui::SliderFloat("obstacleTopLandingMaxHeight", &move_.obstacleTopLandingMaxHeight, 0.3f, 8.0f);
		ImGui::SliderFloat("obstacleTopLandingMinHorizontalOverlap", &move_.obstacleTopLandingMinHorizontalOverlap, 0.01f, 1.5f);
		ImGui::SliderFloat("stuckCheckTime", &stuckSettings_.checkTime, 0.1f, 3.0f);
		ImGui::SliderFloat("stuckDistance", &stuckSettings_.distance, 0.01f, 2.0f);
		ImGui::SliderFloat("stuckMoveThreshold", &stuckSettings_.moveThreshold, 0.03f, 1.2f);
		ImGui::SliderFloat("repathInterval", &pathSettings_.repathInterval, 0.05f, 2.0f);
		ImGui::SliderFloat("waypointReachDistance", &pathSettings_.waypointReachDistance, 0.5f, 1.5f);
		ImGui::Checkbox("pathFindEnabled", &pathSettings_.enabled);
		ImGui::Checkbox("jumpEnabled", &jump_.enabled);
		ImGui::SliderFloat("baseJumpVelocity", &jump_.baseVelocity, 2.0f, 18.0f);
		ImGui::SliderFloat("jumpExtraBoost", &jump_.extraBoost, 0.0f, 8.0f);
		ImGui::SliderFloat("jumpGravityEstimate", &jump_.gravityEstimate, 1.0f, 40.0f);
		ImGui::SliderFloat("maxJumpVelocity", &jump_.maxVelocity, 1.0f, 30.0f);
		ImGui::SliderFloat("jumpTargetHeightThreshold", &jump_.targetHeightThreshold, 0.1f, 5.0f);
		ImGui::SliderFloat("jumpHorizontalDistanceMax", &jump_.horizontalDistanceMax, 0.5f, 20.0f);
		ImGui::SliderFloat("jumpCooldown", &jump_.cooldown, 0.0f, 3.0f);
		const float prevGridSize = pathSettings_.gridSize;
		ImGui::SliderFloat("pathGridSize", &pathSettings_.gridSize, 0.5f, 1.0f);
		if (std::abs(prevGridSize - pathSettings_.gridSize) > kEpsilon) { navigator_.Reset(); pathState_.lastRepathReason = "GridSizeChanged"; }
		ImGui::SliderFloat("pathSearchRadius", &pathSettings_.searchRadius, 6.0f, 80.0f);
		ImGui::SliderFloat("obstacleExpandRadius", &pathSettings_.obstacleExpandRadius, 0.7f, 1.6f);
		ImGui::SliderFloat("temporaryBlockDuration", &pathSettings_.temporaryBlockDuration, 0.3f, 4.0f);
		ImGui::SliderFloat("temporaryBlockRadius", &pathSettings_.temporaryBlockRadius, 0.4f, 2.2f);
		ImGui::Checkbox("cornerCuttingDisabled", &pathSettings_.cornerCuttingDisabled);

		int attackSelect = static_cast<int>(attackSettings_.selectedAttackType);
		const char* attackItems[] = { "Scratch", "OneTwo" };
		if (ImGui::Combo("AttackPattern", &attackSelect, attackItems, IM_ARRAYSIZE(attackItems)))
		{
			attackSettings_.selectedAttackType = static_cast<MeleeAttackType>(attackSelect);
		}
		ImGui::Text("Selected Attack: %s", attackItems[attackSelect]);
		ImGui::Text("Current BT: %s", currentBehaviorName_);
		ImGui::Text("Current Attack Name: %s", attackController_.GetCurrentAttackName());
		ImGui::Text("Distance To Target: %.2f", GetDistanceToTarget());
		ImGui::Text("Is Attacking: %s", attackController_.IsAttacking() ? "true" : "false");
		ImGui::Text("Stuck: %s", stuck_.isStuck ? "true" : "false");
		ImGui::Text("Position: (%.2f, %.2f, %.2f)", GetCenterPosition().x, GetCenterPosition().y, GetCenterPosition().z);
		ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", GetVelocity().x, GetVelocity().y, GetVelocity().z);
		ImGui::Text("Attack Elapsed: %.2f", attackController_.GetAttackElapsed());
		ImGui::Text("Attack Step Index: %d", attackController_.GetCurrentStepIndex());
		ImGui::Text("Cooldown Remaining: %.2f", attackController_.GetCooldownRemaining());
		ImGui::Text("attackLockTimer: %.2f", attackState_.lockTimer);
		ImGui::Text("Attack Active: %s", attackController_.IsCurrentStepActive() ? "true" : "false");
		ImGui::Text("Last Hit: %s", attackController_.WasLastHitSuccess() ? "Hit" : "Miss");
		ImGui::Text("Scratch Arm: %s", scratchArmState_.useLeftArm ? "Left" : "Right");
		ImGui::Text("Path Found: %s", pathState_.found ? "true" : "false");
		ImGui::Text("Obstacle Count: %zu", GetResolvedWorldAABBs() ? GetResolvedWorldAABBs()->size() : 0);
		ImGui::Text("isCollidingWithStage: %s", collision_.isCollidingWithStage ? "true" : "false");
		ImGui::Text("lastStageCollisionType: %s", collision_.lastStageCollisionType.c_str());
		ImGui::Text("lastStageCollisionName: %s", collision_.lastStageCollisionName.c_str());
		ImGui::Text("usingWorldAABBCount: %d", collision_.usingWorldAABBCount);
		ImGui::Text("floorAABBCount: %d", floorAABBs_ ? static_cast<int>(floorAABBs_->size()) : collision_.usingWorldAABBCount);
		ImGui::Text("wallObstacleAABBCount: %d", wallObstacleAABBs_ ? static_cast<int>(wallObstacleAABBs_->size()) : collision_.usingObstacleAABBCount);
		ImGui::Text("usingObstacleAABBCount: %d", collision_.usingObstacleAABBCount);
		ImGui::Text("collisionManagerRegistered: %s", collision_.collisionManagerRegistered ? "true" : "false");
		ImGui::Text("lastCollisionCount: %d", collision_.lastCollisionCount);
		ImGui::Text("blockedByObstacle: %s", collision_.blockedByObstacle ? "true" : "false");
		ImGui::Text("isOnFloor: %s", collision_.isOnFloor ? "true" : "false");
		ImGui::Text("isOverlappingWallObstacle: %s", collision_.isOverlappingWallObstacle ? "true" : "false");
		ImGui::Text("lastWallObstacleName: %s", collision_.lastWallObstacleName.c_str());
		ImGui::Text("lastObstacleTopLandingName: %s", collision_.lastObstacleTopLandingName.c_str());
		ImGui::Text("landedOnObstacleTop: %s", collision_.landedOnObstacleTop ? "true" : "false");
		ImGui::Text("lastWallResolvePush: (%.2f, %.2f, %.2f)", collision_.lastWallResolvePush.x, collision_.lastWallResolvePush.y, collision_.lastWallResolvePush.z);
		ImGui::Text("lastBlockedObstacleName: %s", collision_.lastBlockedObstacleName.c_str());
		ImGui::Text("Path Node Count: %d", static_cast<int>(navigator_.GetCurrentPath().size()));
		ImGui::Text("Current Waypoint Index: %d", navigator_.GetCurrentPathIndex());
		ImGui::Text("Current Waypoint: (%.2f, %.2f, %.2f)", pathState_.currentWaypoint.x, pathState_.currentWaypoint.y, pathState_.currentWaypoint.z);
		ImGui::Text("Last Repath Timer: %.2f", pathState_.lastRepathTimer);
		ImGui::Text("Repath Timer: %.2f", navigator_.GetRepathTimer());
		ImGui::Text("TargetMovedDistanceForRepath: %.2f", pathState_.targetMovedDistanceForRepath);
		ImGui::Text("Path Failure Reason: %s", pathState_.failureReason.c_str());
		ImGui::Text("Last Repath Reason: %s", pathState_.lastRepathReason.c_str());
		ImGui::Text("lineBlocked: %s", pathState_.lineBlocked ? "true" : "false");
		ImGui::Text("blockedObstacleName: %s", pathState_.blockedObstacleName.c_str());
		ImGui::Text("blockedWaypointIndex: %d", pathState_.blockedWaypointIndex);
		ImGui::Text("stuckTimer: %.2f", stuck_.timer);
		ImGui::Text("lastMovedDistance: %.3f", pathState_.lastMovedDistance);
		ImGui::Text("temporaryBlockedCellCount: %d", static_cast<int>(navigator_.GetTemporaryBlockedAreas().size()));
		ImGui::Text("cornerCuttingDisabled: %s", pathSettings_.cornerCuttingDisabled ? "true" : "false");
		ImGui::Text("Grounded: %s", grounded_ ? "true" : "false");
		ImGui::Text("jumpCooldownTimer: %.2f", jumpState_.cooldownTimer);
		ImGui::Text("targetHeightDelta: %.2f", jumpState_.targetHeightDelta);
		ImGui::Text("calculatedJumpVelocity: %.2f", jumpState_.calculatedVelocity);
		ImGui::Text("appliedJumpVelocity: %.2f", jumpState_.appliedVelocity);
		ImGui::Text("lastJumpReason: %s", jumpState_.lastReason.c_str());
		ImGui::Text("AnimState: %s", GetAnimStateName());
		ImGui::Text("lastSafePosition: (%.2f, %.2f, %.2f)", collision_.lastSafePosition.x, collision_.lastSafePosition.y, collision_.lastSafePosition.z);
		ImGui::Text("isOutsideStage: %s", collision_.isOutsideStage ? "true" : "false");
		ImGui::Text("lastResolvePush: (%.2f, %.2f, %.2f)", collision_.lastResolvePush.x, collision_.lastResolvePush.y, collision_.lastResolvePush.z);
		ImGui::Text("pushedByWallThisFrame: %s", collision_.pushedThisFrame ? "true" : "false");
		ImGui::Text("restoredToSafePosition: %s", collision_.restoredToSafePosition ? "true" : "false");
		const Vector3 tgt = GetTargetPosition();
		ImGui::Text("Target: (%.2f, %.2f, %.2f)", tgt.x, tgt.y, tgt.z);
		ImGui::Text("rawYaw(rad): %.3f", animationState_.rawYaw);
		ImGui::Text("finalVisualYaw(rad): %.3f", animationState_.finalVisualYaw);
		ImGui::Text("rawYaw(deg): %.1f", animationState_.rawYaw * (180.0f / kPi));
		ImGui::Text("finalVisualYaw(deg): %.1f", animationState_.finalVisualYaw * (180.0f / kPi));
		ImGui::Text("currentYaw(deg): %.1f", animationState_.debugCurrentYaw * (180.0f / kPi));
		ImGui::Text("targetYaw(deg): %.1f", animationState_.debugTargetYaw * (180.0f / kPi));
		ImGui::Text("deltaYaw(deg): %.1f", animationState_.debugDeltaYaw * (180.0f / kPi));
		ImGui::Text("normalizedDeltaYaw(deg): %.1f", animationState_.debugNormalizedDeltaYaw * (180.0f / kPi));
		ImGui::Text("rotateSpeed: %.2f", move_.rotateSpeed);
		ImGui::Text("facingDirection: (%.2f, %.2f, %.2f)", animationState_.facingDirection.x, animationState_.facingDirection.y, animationState_.facingDirection.z);
		ImGui::Text("movementDir: (%.2f, %.2f, %.2f)", animationState_.movementDirection.x, animationState_.movementDirection.y, animationState_.movementDirection.z);
		ImGui::Text("targetDirection: (%.2f, %.2f, %.2f)", animationState_.targetDirection.x, animationState_.targetDirection.y, animationState_.targetDirection.z);
		ImGui::Text("visualForward: (%.2f, %.2f, %.2f)", animationState_.visualForward.x, animationState_.visualForward.y, animationState_.visualForward.z);
		ImGui::Text("attackForward: (%.2f, %.2f, %.2f)", animationState_.attackForward.x, animationState_.attackForward.y, animationState_.attackForward.z);
		ImGui::Text("headCurrentYaw(deg): %.1f", headLookState_.currentYaw);
		ImGui::Text("headCurrentPitch(deg): %.1f", headLookState_.currentPitch);
		ImGui::Text("headTargetYaw(deg): %.1f", headLookState_.targetYaw);
		ImGui::Text("headTargetPitch(deg): %.1f", headLookState_.targetPitch);
		ImGui::Text("headLookTargetVisible: %s", headLookState_.targetVisible ? "true" : "false");
		ImGui::Text("headLookReason: %s", headLookState_.reason.c_str());
		if (ImGui::Button("Force Scratch Attack")) { ForceAttack(MeleeAttackType::Scratch); }
		ImGui::SameLine();
		if (ImGui::Button("Force OneTwo Attack")) { ForceAttack(MeleeAttackType::OneTwo); }
		if (ImGui::Button("Stop Attack")) { StopAttack(); }
		ImGui::SameLine();
		if (ImGui::Button("Reset Cooldown")) { ResetAttackCooldown(); }
		if (ImGui::Button("Teleport Near Target"))
		{
			Vector3 p = GetTargetPosition();
			p.z -= 1.2f;
			SetCenterPosition(p);
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset Position")) { SetCenterPosition(spawnPosition_); }

		if (MeleeAttackPattern* scratch = attackController_.FindPattern(MeleeAttackType::Scratch))
		{
			if (ImGui::TreeNode("Scratch Params"))
			{
				MeleeAttackStep& step = scratch->steps[0];
				ImGui::SliderInt("Scratch Damage", &step.damage, 1, 50);
				ImGui::SliderFloat("Scratch Range", &step.range, 0.5f, 6.0f);
				ImGui::SliderFloat("Scratch Radius", &step.radius, 0.1f, 3.0f);
				ImGui::SliderFloat("Scratch Startup", &step.startTime, 0.01f, 1.5f);
				ImGui::SliderFloat("Scratch Active", &step.activeTime, 0.01f, 1.0f);
				ImGui::SliderFloat("Scratch Recovery", &scratch->recoveryTime, 0.01f, 2.0f);
				ImGui::SliderFloat("Scratch Cooldown", &scratch->cooldown, 0.01f, 3.0f);
				ImGui::TreePop();
			}
		}

		if (MeleeAttackPattern* oneTwo = attackController_.FindPattern(MeleeAttackType::OneTwo))
		{
			if (ImGui::TreeNode("OneTwo Params"))
			{
				MeleeAttackStep& left = oneTwo->steps[0];
				MeleeAttackStep& right = oneTwo->steps[1];
				ImGui::SliderInt("OneTwo Left Damage", &left.damage, 1, 50);
				ImGui::SliderFloat("OneTwo Left Start", &left.startTime, 0.01f, 1.5f);
				ImGui::SliderFloat("OneTwo Left Active", &left.activeTime, 0.01f, 1.0f);
				ImGui::SliderFloat("OneTwo Left Range", &left.range, 0.5f, 6.0f);
				ImGui::SliderFloat("OneTwo Left Radius", &left.radius, 0.1f, 3.0f);
				ImGui::SliderInt("OneTwo Right Damage", &right.damage, 1, 50);
				ImGui::SliderFloat("OneTwo Right Start", &right.startTime, 0.01f, 2.0f);
				ImGui::SliderFloat("OneTwo Right Active", &right.activeTime, 0.01f, 1.0f);
				ImGui::SliderFloat("OneTwo Right Range", &right.range, 0.5f, 6.0f);
				ImGui::SliderFloat("OneTwo Right Radius", &right.radius, 0.1f, 3.0f);
				ImGui::SliderFloat("OneTwo Forward Speed", &oneTwo->forwardMoveSpeed, 0.0f, 5.0f);
				ImGui::SliderFloat("OneTwo Forward Duration", &oneTwo->forwardMoveDuration, 0.0f, 2.0f);
				ImGui::SliderFloat("OneTwo MinForwardDistance", &detection_.minOneTwoForwardDistance, 0.1f, 5.0f);
				ImGui::SliderFloat("OneTwo Recovery", &oneTwo->recoveryTime, 0.01f, 2.0f);
				ImGui::SliderFloat("OneTwo Cooldown", &oneTwo->cooldown, 0.01f, 3.0f);
				ImGui::TreePop();
			}
		}
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
	TryJumpToTarget(1.0f / 60.0f);
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
	navigator_.SetWorldAABBs(GetResolvedNavigationObstacleAABBs());
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
		// 経路追跡中でも段差追従のためにジャンプ可否を判定する
		TryJumpToTarget(deltaTime);
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

float MeleeEnemy::CalculateJumpVelocityForHeight(float heightDelta) const
{
	// 高さ差に必要な初速を重力推定値から算出し、段差追従ジャンプの不足を防ぐ
	const float clampedHeight = std::max(0.0f, heightDelta);
	return std::sqrt(2.0f * std::max(jump_.gravityEstimate, 0.0f) * clampedHeight);
}

void MeleeEnemy::TryJumpToTarget(float)
{
	if (!jump_.enabled) { jumpState_.lastReason = "Disabled"; return; }
	if (!HasTarget()) { jumpState_.lastReason = "NoTarget"; return; }
	if (IsDeadCondition()) { jumpState_.lastReason = "Dead"; return; }
	if (attackController_.IsAttacking() || attackState_.lockTimer > 0.0f) { jumpState_.lastReason = "Attack"; return; }
	if (!grounded_) { jumpState_.lastReason = "Airborne"; return; }
	if (jumpState_.cooldownTimer > 0.0f) { jumpState_.lastReason = "Cooldown"; return; }

	const Vector3 selfPos = GetCenterPosition();
	const Vector3 targetPos = GetTargetPosition();
	const Vector3 toTarget = targetPos - selfPos;
	const float heightDelta = toTarget.y;
	const float horizontalDistance = LengthXZ(toTarget);
	jumpState_.targetHeightDelta = heightDelta;
	jumpState_.calculatedVelocity = CalculateJumpVelocityForHeight(heightDelta);

	if (heightDelta < jump_.targetHeightThreshold) { jumpState_.lastReason = "TargetNotHigher"; jumpState_.appliedVelocity = 0.0f; return; }
	if (horizontalDistance > jump_.horizontalDistanceMax) { jumpState_.lastReason = "TooFar"; return; }

	const bool obstacleJumpCandidate = collision_.blockedByObstacle || pathState_.lineBlocked || stuck_.isStuck || collision_.isOverlappingWallObstacle;
	if (obstacleJumpCandidate)
	{
		jumpState_.lastReason = "TargetHigherBlocked";
	}
	else
	{
		jumpState_.lastReason = "TargetHigher";
	}
	// 水平速度は維持しつつY速度だけを上書きし、段差追跡ジャンプを行う
	Vector3 v = GetVelocity();
	const float requested = std::max(jump_.baseVelocity, jumpState_.calculatedVelocity + jump_.extraBoost);
	jumpState_.appliedVelocity = std::clamp(requested, 0.0f, jump_.maxVelocity);
	v.y = jumpState_.appliedVelocity;
	SetVelocity(v);
	jumpState_.cooldownTimer = jump_.cooldown;
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
