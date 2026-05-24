#define NOMINMAX
#include "MeleeEnemy.h"

#include <cmath>
#include <algorithm>
#include <cstdlib>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace Ken4lowEngine;

namespace
{
	constexpr float kEpsilon = 0.0001f;
	constexpr float kPi = 3.1415926535f;

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
}

void MeleeEnemy::Update(float deltaTime)
{
	attackController_.Update(*this, deltaTime);
	attackLockTimer_ = std::max(0.0f, attackLockTimer_ - deltaTime);

	// 優先度の高い行動から順に評価して近接敵の行動を決定する
	EvaluateBehavior(deltaTime);
	EnemyBase::Update(deltaTime);
}

void MeleeEnemy::Draw()
{
	EnemyBase::Draw();
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
		ImGui::Text("Path Node Count: %d", pathFound_ ? 1 : 0);
		ImGui::Text("Current Waypoint Index: %d", pathFound_ ? 0 : -1);
		ImGui::Text("Last Repath Timer: %.2f", lastRepathTimer_);
		ImGui::Text("Path Failure Reason: %s", pathFailureReason_.c_str());
		ImGui::Text("Grounded: %s", grounded_ ? "true" : "false");
		const Vector3 tgt = GetTargetPosition();
		ImGui::Text("Target: (%.2f, %.2f, %.2f)", tgt.x, tgt.y, tgt.z);
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

void MeleeEnemy::FaceToTarget()
{
	if (!target_) { return; }
	Vector3 dir = NormalizeXZ(GetTargetPosition() - GetCenterPosition());
	if (LengthXZ(dir) <= kEpsilon) { return; }
	const float yaw = std::atan2(dir.x, dir.z);
	SetOrientation({ 0.0f, yaw, 0.0f });
}

void MeleeEnemy::DeadAction()
{
	StopMove();
	animState_ = AnimState::Dead;
	currentBehaviorName_ = "DeadAction";
}

void MeleeEnemy::MeleeAttackAction()
{
	FaceToTarget();
	StopMove();
	if (!attackController_.IsAttacking() && attackController_.CanStartAttack())
	{
		// 攻撃パターンをデータとして扱い、ScratchとOneTwoを同じ制御経路で実行する
		attackController_.StartAttack(selectedAttackType_);
		attackLockTimer_ = attackLockTime_;
		animState_ = AnimState::Attack;
	}
	currentBehaviorName_ = (selectedAttackType_ == MeleeAttackType::OneTwo) ? "OneTwoAttack" : "ScratchAttack";
}

void MeleeEnemy::CombatIdleAction()
{
	// 攻撃範囲内では追跡を止め、AttackとChaseの細かい切り替わりによる震えを防ぐ
	StopMove();
	FaceToTarget();
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
		FaceToTarget();
		animState_ = AnimState::Idle;
		currentBehaviorName_ = "ChaseStopNearTarget";
		return;
	}
	if (pathFindEnabled_ && MoveAlongPath(1.0f / 60.0f))
	{
		FaceToTarget();
		animState_ = AnimState::Move;
		currentBehaviorName_ = "ChasePathMove";
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
	FaceToTarget();
	animState_ = AnimState::Move;
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
	navigator_.SetWorldAABBs(GetResolvedWorldAABBs());
	lastRepathTimer_ += deltaTime;
	// 障害物を考慮した経路を使い、MeleeEnemyが直線移動で引っかからないようにする
	if (navigator_.GetNextWaypoint(GetCenterPosition(), GetTargetPosition(), GetCenterPosition().y, deltaTime, currentPathWaypoint_))
	{
		pathFound_ = true;
		pathFailureReason_ = "None";
		const Vector3 dir = NormalizeXZ(currentPathWaypoint_ - GetCenterPosition());
		SetVelocity({ dir.x * moveSpeed_, GetVelocity().y, dir.z * moveSpeed_ });
		return true;
	}
	pathFound_ = false;
	pathFailureReason_ = "PathNotFound";
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
	currentBehaviorName_ = "WanderAction";
	animState_ = AnimState::Move;
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
