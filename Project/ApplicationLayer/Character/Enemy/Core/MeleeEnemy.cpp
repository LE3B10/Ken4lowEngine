#define NOMINMAX
#include "MeleeEnemy.h"

#include <cmath>
#include <algorithm>
#include <cstdlib>
#include "Wireframe.h"

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
}

void MeleeEnemy::Initialize()
{
	EnemyBase::Initialize();
	SetMaxHp(160);
	SetCenterPosition({ 0.0f, 2.0f, 10.0f });
	wanderTimer_ = 0.0f;
	currentBehaviorName_ = "Wander";
	animState_ = AnimState::Idle;
	attackController_.Initialize();
	spawnPosition_ = GetCenterPosition();
	lastStuckCheckPosition_ = spawnPosition_;
	lastSafePosition_ = spawnPosition_;
	visualYawOffset_ = 0.0f;
	visualYawOffsetDeg_ = 0.0f;
}

void MeleeEnemy::Update(float deltaTime)
{
	const Vector3 beforePos = GetCenterPosition();
	attackController_.Update(*this, deltaTime);
	attackLockTimer_ = std::max(0.0f, attackLockTimer_ - deltaTime);

	// 優先度の高い行動から順に評価して近接敵の行動を決定する
	EvaluateBehavior(deltaTime);
	EnemyBase::Update(deltaTime);

	Vector3 afterPos = GetCenterPosition();
	Vector3 push = afterPos - (beforePos + GetVelocity() * deltaTime);
	// 押し出し補正の暴発でステージ外へ飛ばされるのを防ぐため、フレーム補正量を制限する
	const float pushLen = std::sqrt(push.x * push.x + push.y * push.y + push.z * push.z);
	const float horizontalPushLen = LengthXZ(push);
	if (pushLen > maxResolvePushPerFrame_ || horizontalPushLen > maxHorizontalPushPerFrame_)
	{
		const float clampScale = std::min(maxResolvePushPerFrame_ / std::max(pushLen, kEpsilon), maxHorizontalPushPerFrame_ / std::max(horizontalPushLen, kEpsilon));
		afterPos = beforePos + (afterPos - beforePos) * std::max(0.0f, std::min(1.0f, clampScale));
		SetCenterPosition(afterPos);
	}
	lastResolvePush_ = afterPos - beforePos;

	if (const auto* aabbs = GetResolvedWorldAABBs(); aabbs && !aabbs->empty())
	{
		for (const auto& aabb : *aabbs)
		{
			const float sizeX = aabb.max.x - aabb.min.x;
			const float sizeZ = aabb.max.z - aabb.min.z;
			const float sizeY = aabb.max.y - aabb.min.y;
			if (sizeX > 2.0f && sizeZ > 2.0f && sizeY <= 3.5f)
			{
				if (!hasStageBounds_)
				{
					stageBoundsMin_ = aabb.min;
					stageBoundsMax_ = aabb.max;
					hasStageBounds_ = true;
				}
				else
				{
					stageBoundsMin_.x = std::min(stageBoundsMin_.x, aabb.min.x);
					stageBoundsMin_.z = std::min(stageBoundsMin_.z, aabb.min.z);
					stageBoundsMax_.x = std::max(stageBoundsMax_.x, aabb.max.x);
					stageBoundsMax_.z = std::max(stageBoundsMax_.z, aabb.max.z);
				}
			}
		}
	}

	isOutsideStage_ = hasStageBounds_ && !IsInsideStageBounds(GetCenterPosition());
	if (grounded_ && !isOutsideStage_)
	{
		lastSafePosition_ = GetCenterPosition();
	}
	else if (isOutsideStage_)
	{
		// 押し出し補正が大きすぎる場合は安全位置へ戻し、ステージ外へ弾き出されるのを防ぐ
		SetCenterPosition(lastSafePosition_);
		SetVelocity({ 0.0f, GetVelocity().y, 0.0f });
	}

	UpdateVisualAnimation(deltaTime);
}

void MeleeEnemy::Draw()
{
	EnemyBase::Draw();
	const Vector3 origin = GetCenterPosition() + Vector3{ 0.0f, 1.0f, 0.0f };
	Wireframe::GetInstance()->DrawLine(origin, origin + movementDirection_ * 1.8f, { 0.2f, 0.8f, 1.0f, 1.0f });
	Wireframe::GetInstance()->DrawLine(origin, origin + targetDirection_ * 2.0f, { 1.0f, 1.0f, 0.2f, 1.0f });
	Wireframe::GetInstance()->DrawLine(origin, origin + visualForward_ * 2.2f, { 1.0f, 0.4f, 1.0f, 1.0f });
	Wireframe::GetInstance()->DrawLine(origin, origin + attackForward_ * 2.4f, { 1.0f, 0.2f, 0.2f, 1.0f });
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
}

void MeleeEnemy::DrawImGui()
{
#ifdef USE_IMGUI
	if (ImGui::Begin("MeleeEnemy Debug"))
	{
		EnemyBase::DrawImGui();
		ImGui::SliderFloat("detectRange", &detectRange_, 1.0f, 50.0f);
		ImGui::SliderFloat("meleeAttackRange", &meleeAttackRange_, 0.5f, 10.0f);
		ImGui::SliderFloat("moveSpeed", &moveSpeed_, 0.1f, 10.0f);
		ImGui::SliderFloat("stopDistance", &stopDistance_, 0.5f, 6.0f);
		ImGui::SliderFloat("attackStartRange", &attackStartRange_, 0.5f, 8.0f);
		ImGui::SliderFloat("resumeChaseDistance", &resumeChaseDistance_, 0.5f, 10.0f);
		ImGui::SliderFloat("attackLockTime", &attackLockTime_, 0.0f, 1.0f);
		ImGui::SliderFloat("rotateSpeed", &rotateSpeed_, 0.1f, 20.0f);
		ImGui::Text("visualYawOffset(rad): %.3f (fixed)", visualYawOffset_);
		ImGui::SliderFloat("walkAnimSpeed", &walkAnimSpeed_, 1.0f, 18.0f);
		ImGui::SliderFloat("walkArmSwing", &walkArmSwing_, 0.0f, 1.5f);
		ImGui::SliderFloat("walkLegSwing", &walkLegSwing_, 0.0f, 1.5f);
		ImGui::SliderFloat("attackArmSwing", &attackArmSwing_, 0.0f, 2.0f);
		ImGui::SliderFloat("attackReturnSpeed", &attackReturnSpeed_, 1.0f, 24.0f);
		ImGui::SliderFloat("attackBodyLean", &attackBodyLean_, 0.0f, 0.4f);
		ImGui::SliderFloat("maxResolvePushPerFrame", &maxResolvePushPerFrame_, 0.05f, 2.0f);
		ImGui::SliderFloat("stuckCheckTime", &stuckCheckTime_, 0.1f, 3.0f);
		ImGui::SliderFloat("stuckDistance", &stuckDistance_, 0.01f, 2.0f);
		ImGui::SliderFloat("repathInterval", &repathInterval_, 0.05f, 2.0f);
		ImGui::SliderFloat("waypointReachDistance", &waypointReachDistance_, 0.1f, 3.0f);
		ImGui::Checkbox("pathFindEnabled", &pathFindEnabled_);
		ImGui::SliderFloat("pathGridSize", &pathGridSize_, 0.3f, 4.0f);
		ImGui::SliderFloat("pathSearchRadius", &pathSearchRadius_, 6.0f, 80.0f);
		ImGui::SliderFloat("obstacleExpandRadius", &obstacleExpandRadius_, 0.1f, 2.0f);

		int attackSelect = static_cast<int>(selectedAttackType_);
		const char* attackItems[] = { "Scratch", "OneTwo" };
		if (ImGui::Combo("AttackPattern", &attackSelect, attackItems, IM_ARRAYSIZE(attackItems)))
		{
			selectedAttackType_ = static_cast<MeleeAttackType>(attackSelect);
		}
		ImGui::Text("Selected Attack: %s", attackItems[attackSelect]);
		ImGui::Text("Current BT: %s", currentBehaviorName_);
		ImGui::Text("Current Attack Name: %s", attackController_.GetCurrentAttackName());
		ImGui::Text("Distance To Target: %.2f", GetDistanceToTarget());
		ImGui::Text("Is Attacking: %s", attackController_.IsAttacking() ? "true" : "false");
		ImGui::Text("Stuck: %s", isStuck_ ? "true" : "false");
		ImGui::Text("Position: (%.2f, %.2f, %.2f)", GetCenterPosition().x, GetCenterPosition().y, GetCenterPosition().z);
		ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", GetVelocity().x, GetVelocity().y, GetVelocity().z);
		ImGui::Text("Attack Elapsed: %.2f", attackController_.GetAttackElapsed());
		ImGui::Text("Attack Step Index: %d", attackController_.GetCurrentStepIndex());
		ImGui::Text("Cooldown Remaining: %.2f", attackController_.GetCooldownRemaining());
		ImGui::Text("attackLockTimer: %.2f", attackLockTimer_);
		ImGui::Text("Attack Active: %s", attackController_.IsCurrentStepActive() ? "true" : "false");
		ImGui::Text("Last Hit: %s", attackController_.WasLastHitSuccess() ? "Hit" : "Miss");
		ImGui::Text("Path Found: %s", pathFound_ ? "true" : "false");
		ImGui::Text("Obstacle Count: %zu", GetResolvedWorldAABBs() ? GetResolvedWorldAABBs()->size() : 0);
		ImGui::Text("Path Node Count: %d", static_cast<int>(navigator_.GetCurrentPath().size()));
		ImGui::Text("Current Waypoint Index: %d", navigator_.GetCurrentPathIndex());
		ImGui::Text("Current Waypoint: (%.2f, %.2f, %.2f)", currentPathWaypoint_.x, currentPathWaypoint_.y, currentPathWaypoint_.z);
		ImGui::Text("Last Repath Timer: %.2f", lastRepathTimer_);
		ImGui::Text("Repath Timer: %.2f", navigator_.GetRepathTimer());
		ImGui::Text("TargetMovedDistanceForRepath: %.2f", targetMovedDistanceForRepath_);
		ImGui::Text("Path Failure Reason: %s", pathFailureReason_.c_str());
		ImGui::Text("Last Repath Reason: %s", lastRepathReason_.c_str());
		ImGui::Text("Grounded: %s", grounded_ ? "true" : "false");
		ImGui::Text("AnimState: %s", GetAnimStateName());
		ImGui::Text("lastSafePosition: (%.2f, %.2f, %.2f)", lastSafePosition_.x, lastSafePosition_.y, lastSafePosition_.z);
		ImGui::Text("isOutsideStage: %s", isOutsideStage_ ? "true" : "false");
		ImGui::Text("lastResolvePush: (%.2f, %.2f, %.2f)", lastResolvePush_.x, lastResolvePush_.y, lastResolvePush_.z);
		const Vector3 tgt = GetTargetPosition();
		ImGui::Text("Target: (%.2f, %.2f, %.2f)", tgt.x, tgt.y, tgt.z);
		ImGui::Text("rawYaw(rad): %.3f", rawYaw_);
		ImGui::Text("finalVisualYaw(rad): %.3f", finalVisualYaw_);
		ImGui::Text("rawYaw(deg): %.1f", rawYaw_ * (180.0f / kPi));
		ImGui::Text("finalVisualYaw(deg): %.1f", finalVisualYaw_ * (180.0f / kPi));
		ImGui::Text("currentYaw(deg): %.1f", debugCurrentYaw_ * (180.0f / kPi));
		ImGui::Text("targetYaw(deg): %.1f", debugTargetYaw_ * (180.0f / kPi));
		ImGui::Text("deltaYaw(deg): %.1f", debugDeltaYaw_ * (180.0f / kPi));
		ImGui::Text("normalizedDeltaYaw(deg): %.1f", debugNormalizedDeltaYaw_ * (180.0f / kPi));
		ImGui::Text("rotateSpeed: %.2f", rotateSpeed_);
		ImGui::Text("facingDirection: (%.2f, %.2f, %.2f)", facingDirection_.x, facingDirection_.y, facingDirection_.z);
		ImGui::Text("movementDir: (%.2f, %.2f, %.2f)", movementDirection_.x, movementDirection_.y, movementDirection_.z);
		ImGui::Text("targetDirection: (%.2f, %.2f, %.2f)", targetDirection_.x, targetDirection_.y, targetDirection_.z);
		ImGui::Text("visualForward: (%.2f, %.2f, %.2f)", visualForward_.x, visualForward_.y, visualForward_.z);
		ImGui::Text("attackForward: (%.2f, %.2f, %.2f)", attackForward_.x, attackForward_.y, attackForward_.z);
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
				ImGui::SliderFloat("OneTwo MinForwardDistance", &minOneTwoForwardDistance_, 0.1f, 5.0f);
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

bool MeleeEnemy::IsTargetInDetectRange() const { return GetDistanceToTarget() <= detectRange_; }
bool MeleeEnemy::IsTargetInMeleeRange() const { return GetDistanceToTarget() <= meleeAttackRange_; }
bool MeleeEnemy::IsTargetInAttackStartRange() const { return GetDistanceToTarget() <= attackStartRange_; }
bool MeleeEnemy::IsTargetInAttackHoldRange() const { return GetDistanceToTarget() <= resumeChaseDistance_; }
bool MeleeEnemy::IsAttackCooldownReady() const { return attackController_.CanStartAttack(); }
bool MeleeEnemy::IsMoveResumeDistanceReached() const { return GetDistanceToTarget() >= resumeChaseDistance_; }

void MeleeEnemy::FaceToTarget(float deltaTime)
{
	if (!target_) { return; }
	targetDirection_ = NormalizeXZ(GetTargetPosition() - GetCenterPosition());
	ApplyVisualYawFromDirection(targetDirection_, deltaTime);
}

void MeleeEnemy::FaceToMoveDirection(float deltaTime)
{
	const Vector3 moveDir = NormalizeXZ(GetVelocity());
	movementDirection_ = moveDir;
	ApplyVisualYawFromDirection(moveDir, deltaTime);
}

void MeleeEnemy::ApplyVisualYawFromDirection(const Vector3& direction, float deltaTime)
{
	if (LengthXZ(direction) <= kEpsilon) { return; }
	rawYaw_ = std::atan2(-direction.x, direction.z);
	const float targetVisualYaw = rawYaw_;
	float currentYaw = NormalizeAngleRad(orientation_.y);
	const float maxStep = rotateSpeed_ * std::max(deltaTime, 0.0f);
	float deltaYaw = NormalizeAngleRad(targetVisualYaw - currentYaw);
	if (std::abs(deltaYaw) > (kPi - 0.001f))
	{
		deltaYaw = (deltaYaw >= 0.0f) ? (kPi - 0.001f) : (-kPi + 0.001f);
	}
	// Yaw差分を-π～+πに正規化し、左右ターゲット移動時の逆回転を防ぐ
	deltaYaw = std::clamp(deltaYaw, -maxStep, maxStep);
	currentYaw = NormalizeAngleRad(currentYaw + deltaYaw);
	debugCurrentYaw_ = currentYaw;
	debugTargetYaw_ = targetVisualYaw;
	debugDeltaYaw_ = targetVisualYaw - orientation_.y;
	debugNormalizedDeltaYaw_ = NormalizeAngleRad(targetVisualYaw - orientation_.y);
	finalVisualYaw_ = currentYaw;
	facingDirection_ = { -std::sin(rawYaw_), 0.0f, std::cos(rawYaw_) };
	visualForward_ = NormalizeXZ({ -std::sin(finalVisualYaw_), 0.0f, std::cos(finalVisualYaw_) });
	attackForward_ = visualForward_;
	// 最終的な人型パーツのYawへ正面補正を加え、移動方向と見た目の向きを一致させる
	SetOrientation({ 0.0f, currentYaw, 0.0f });
}

void MeleeEnemy::DeadAction()
{
	StopMove();
	animState_ = AnimState::Dead;
	currentBehaviorName_ = "DeadAction";
}

void MeleeEnemy::MeleeAttackAction()
{
	FaceToTarget(1.0f / 60.0f);
	StopMove();
	if (!attackController_.IsAttacking() && attackController_.CanStartAttack())
	{
		// 攻撃パターンをデータとして扱い、ScratchとOneTwoを同じ制御経路で実行する
		attackController_.StartAttack(selectedAttackType_);
		attackLockTimer_ = attackLockTime_;
	animState_ = (selectedAttackType_ == MeleeAttackType::OneTwo) ? AnimState::OneTwo : AnimState::Scratch;
	}
	currentBehaviorName_ = (selectedAttackType_ == MeleeAttackType::OneTwo) ? "OneTwoAttack" : "ScratchAttack";
}

void MeleeEnemy::CombatIdleAction()
{
	// 攻撃範囲内では追跡を止め、AttackとChaseの細かい切り替わりによる震えを防ぐ
	StopMove();
	FaceToTarget(1.0f / 60.0f);
	animState_ = AnimState::Idle;
	currentBehaviorName_ = "CombatIdle";
}

void MeleeEnemy::ChaseTargetAction()
{
	UpdateStuckState(1.0f / 60.0f);
	const float distance = GetDistanceToTarget();
	if (distance <= stopDistance_)
	{
		StopMove();
		FaceToTarget(1.0f / 60.0f);
		animState_ = AnimState::Idle;
		currentBehaviorName_ = "ChaseStopNearTarget";
		return;
	}
	if (pathFindEnabled_ && !attackController_.IsAttacking() && distance > attackStartRange_ && MoveAlongPath(1.0f / 60.0f))
	{
		FaceToMoveDirection(1.0f / 60.0f);
		animState_ = AnimState::Walk;
		currentBehaviorName_ = "ChasePathMove";
		return;
	}
	if (pathFindEnabled_)
	{
		StopMove();
		currentBehaviorName_ = "PathFailed";
		return;
	}
	const Vector3 moveDir = NormalizeXZ(GetTargetPosition() - GetCenterPosition());
	if (LengthXZ(moveDir) <= kEpsilon)
	{
		StopMove();
		currentBehaviorName_ = "ChaseNoMove";
		return;
	}
	SetVelocity({ moveDir.x * moveSpeed_, GetVelocity().y, moveDir.z * moveSpeed_ });
	FaceToMoveDirection(1.0f / 60.0f);
	animState_ = AnimState::Walk;
	currentBehaviorName_ = "ChaseTargetAction";
}

bool MeleeEnemy::MoveAlongPath(float deltaTime)
{
	EnemyAStarNavigator::Settings s = navigator_.GetSettings();
	s.cellSize = pathGridSize_;
	s.agentRadius = obstacleExpandRadius_;
	s.searchRangeCells = static_cast<int>(pathSearchRadius_);
	s.repathIntervalSec = repathInterval_;
	s.waypointReachDistance = waypointReachDistance_;
	navigator_.SetSettings(s);
	// 障害物AABBを避ける経路を使い、近接敵がプレイヤーへ最短経路で詰めるようにする
	navigator_.SetWorldAABBs(GetResolvedNavigationObstacleAABBs());
	lastRepathTimer_ += deltaTime;
	const Vector3 targetPos = GetTargetPosition();
	targetMovedDistanceForRepath_ = LengthXZ(targetPos - lastPathTargetPos_);
	if (targetMovedDistanceForRepath_ >= targetRepathThreshold_ || isStuck_)
	{
		navigator_.Reset();
		lastRepathReason_ = isStuck_ ? "StuckForceRepath" : "TargetMoved";
	}
	// 障害物を考慮した経路を使い、MeleeEnemyが直線移動で引っかからないようにする
	if (navigator_.GetNextWaypoint(GetCenterPosition(), targetPos, GetCenterPosition().y, deltaTime, currentPathWaypoint_))
	{
		pathFound_ = true;
		pathFailureReason_ = "None";
		lastRepathReason_ = "Periodic";
		lastPathTargetPos_ = targetPos;
		pathRetryTimer_ = 0.0f;
		const Vector3 dir = NormalizeXZ(currentPathWaypoint_ - GetCenterPosition());
		SetVelocity({ dir.x * moveSpeed_, GetVelocity().y, dir.z * moveSpeed_ });
		return true;
	}
	pathFound_ = false;
	pathFailureReason_ = "PathNotFound";
	pathRetryTimer_ += deltaTime;
	if (pathRetryTimer_ >= repathInterval_)
	{
		navigator_.Reset();
		pathRetryTimer_ = 0.0f;
		lastRepathReason_ = "RetryAfterFailure";
	}
	StopMove();
	return false;
}

void MeleeEnemy::UpdateStuckState(float deltaTime)
{
	stuckTimer_ += deltaTime;
	if (stuckTimer_ < stuckCheckTime_) { return; }
	const float moved = LengthXZ(GetCenterPosition() - lastStuckCheckPosition_);
	isStuck_ = moved <= stuckDistance_ && GetDistanceToTarget() > stopDistance_;
	if (isStuck_)
	{
		navigator_.Reset();
		const Vector3 toTarget = NormalizeXZ(GetTargetPosition() - GetCenterPosition());
		const Vector3 side = { -toTarget.z, 0.0f, toTarget.x };
		SetVelocity({ side.x * (moveSpeed_ * 0.4f), GetVelocity().y, side.z * (moveSpeed_ * 0.4f) });
	}
	lastStuckCheckPosition_ = GetCenterPosition();
	stuckTimer_ = 0.0f;
}

void MeleeEnemy::WanderAction(float deltaTime)
{
	wanderTimer_ -= deltaTime;
	if (wanderTimer_ <= 0.0f)
	{
		wanderTimer_ = 1.4f;
		const float angle = static_cast<float>((std::rand() % 360) * (kPi / 180.0f));
		wanderDirection_ = { std::sin(angle), 0.0f, std::cos(angle) };
	}
	SetVelocity({ wanderDirection_.x * (moveSpeed_ * 0.45f), GetVelocity().y, wanderDirection_.z * (moveSpeed_ * 0.45f) });
	FaceToMoveDirection(1.0f / 60.0f);
	currentBehaviorName_ = "WanderAction";
	animState_ = AnimState::Walk;
}

void MeleeEnemy::ApplyAttackMove(const Vector3& horizontalVelocity)
{
	if (selectedAttackType_ == MeleeAttackType::OneTwo && GetDistanceToTarget() > minOneTwoForwardDistance_)
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

	if (attackController_.IsAttacking() || attackLockTimer_ > 0.0f)
	{
		MeleeAttackAction();
		return;
	}

	const float distance = GetDistanceToTarget();
	isStuck_ = (distance <= stopDistance_ && LengthXZ(GetVelocity()) < 0.05f);

	if (distance <= attackStartRange_)
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

	if (distance <= meleeAttackRange_)
	{
		// 攻撃射程内では押し込みを止める
		CombatIdleAction();
		return;
	}

	shouldChase_ = shouldChase_ ? !IsTargetInAttackHoldRange() : IsMoveResumeDistanceReached();
	if (HasTarget() && IsTargetInDetectRange() && (shouldChase_ || distance > resumeChaseDistance_))
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
	selectedAttackType_ = type;
	attackController_.ResetCooldown();
	attackController_.StopAttack();
	attackController_.StartAttack(type);
	attackLockTimer_ = attackLockTime_;
}

void MeleeEnemy::StopAttack()
{
	attackController_.StopAttack();
	attackLockTimer_ = 0.0f;
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
	const float speedRate = std::min(1.5f, LengthXZ(GetVelocity()) / std::max(moveSpeed_, kEpsilon));
	walkAnimTime_ += deltaTime * (walkAnimSpeed_ * std::max(0.2f, speedRate));
	float armTarget = 0.0f;
	float legTarget = 0.0f;
	float lAttack = 0.0f;
	float rAttack = 0.0f;
	if (attackController_.IsAttacking())
	{
		if (attackController_.GetCurrentAttackType() == MeleeAttackType::Scratch || attackController_.GetCurrentStepIndex() == 0) { lAttack = attackArmSwing_; }
		if (attackController_.GetCurrentAttackType() == MeleeAttackType::OneTwo && attackController_.GetCurrentStepIndex() == 1) { rAttack = attackArmSwing_; }
	}
	else if (animState_ == AnimState::Walk)
	{
		armTarget = std::sin(walkAnimTime_) * walkArmSwing_ * speedRate;
		legTarget = std::sin(walkAnimTime_) * walkLegSwing_ * speedRate;
	}
	const float ret = std::min(1.0f, attackReturnSpeed_ * deltaTime);
	parts_[lArm].transform.rotate_.x += ((armTarget - lAttack) - parts_[lArm].transform.rotate_.x) * ret;
	parts_[rArm].transform.rotate_.x += ((-armTarget - rAttack) - parts_[rArm].transform.rotate_.x) * ret;
	parts_[lLeg].transform.rotate_.x += ((-legTarget) - parts_[lLeg].transform.rotate_.x) * ret;
	parts_[rLeg].transform.rotate_.x += ((legTarget) - parts_[rLeg].transform.rotate_.x) * ret;
	body_.transform.rotate_.x = (lAttack + rAttack) * attackBodyLean_;
	UpdateVisualHierarchy();
}

bool MeleeEnemy::IsInsideStageBounds(const Vector3& position) const
{
	if (!hasStageBounds_) { return true; }
	return position.x >= stageBoundsMin_.x && position.x <= stageBoundsMax_.x && position.z >= stageBoundsMin_.z && position.z <= stageBoundsMax_.z;
}

const char* MeleeEnemy::GetAnimStateName() const
{
	switch (animState_)
	{
	case AnimState::Idle: return "Idle";
	case AnimState::Walk: return "Walk";
	case AnimState::Scratch: return "Scratch";
	case AnimState::OneTwo: return "OneTwo";
	case AnimState::Dead: return "Dead";
	default: return "Unknown";
	}
}
