#define NOMINMAX
#include "GuardianBoss.h"
#include "Attacks/BossPunchAttack.h"
#include "Attacks/BossHeavyPunchAttack.h"
#include "BehaviorTree/BTActionNode.h"
#include "BehaviorTree/BTConditionNode.h"
#include "BehaviorTree/BTSelectorNode.h"
#include "BehaviorTree/BTSequenceNode.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace Ken4lowEngine;

namespace {
float WrapAngle(float angle)
{
	while (angle > 3.14159265f) { angle -= 6.28318530f; }
	while (angle < -3.14159265f) { angle += 6.28318530f; }
	return angle;
}

float LengthXZ(const Vector3& v)
{
	return std::sqrt(v.x * v.x + v.z * v.z);
}
}

void GuardianBoss::SetupBoss()
{
	HumanoidBossBase::SetupBoss();
	SetPhase(BossPhase::Phase1);
	runtime_ = {};
	chargeTimer_ = 0.0f;
	ChangeBossState(BossState::Idle);
	BuildBehaviorTree(); // 平原ボスの意思決定はBTに集約する。
	ApplySkinToAllParts("Characters/zombie.dds");
}

void GuardianBoss::OnDamaged(float damage)
{
	if (GetState() == BossState::Dead) { return; }
	BossBase::OnDamaged(damage);
}

void GuardianBoss::OnDead() { ChangeBossState(BossState::Dead); }
void GuardianBoss::OnCollision(Collider* other) { (void)other; }

void GuardianBoss::UpdateState(float deltaTime)
{
	runtime_.stateTimer += deltaTime;
	runtime_.attackCooldownTimer = std::max(0.0f, runtime_.attackCooldownTimer - deltaTime);
	chargeTimer_ = std::max(0.0f, chargeTimer_ - deltaTime);
	CheckDeath();
	if (GetState() == BossState::Dead) { return; }
	UpdatePhaseTransition();
	FaceTarget(deltaTime);
	TickBehaviorTree(deltaTime);
}

void GuardianBoss::UpdateMovement(float deltaTime)
{
	if (GetState() != BossState::Move && runtime_.currentActionName != "Charge") { return; }
	UpdateStuckState(deltaTime);
	const float speed = (runtime_.currentActionName == "Charge") ? GetCurrentMoveSpeed() * 2.3f : GetCurrentMoveSpeed();
	if (!MoveAlongPath(deltaTime))
	{
		Vector3 to{ GetTargetPosition().x - GetPosition().x, 0.0f, GetTargetPosition().z - GetPosition().z };
		const float lenSq = to.x * to.x + to.z * to.z;
		if (lenSq <= 0.0001f) { return; }
		const float len = std::sqrt(lenSq);
		to.x /= len; to.z /= len;
		movementVelocity_ = Vector3{ to.x * speed, 0.0f, to.z * speed };
	}
	Vector3 nextPos = GetPosition();
	nextPos.x += movementVelocity_.x * deltaTime;
	nextPos.z += movementVelocity_.z * deltaTime;
	SetPosition(nextPos);
	const bool landedOnObstacleTop = TryLandOnObstacleTop(deltaTime);
	if (!landedOnObstacleTop) { ResolveObstaclePenetrationXZ(deltaTime); }
}

void GuardianBoss::UpdateAttack(float deltaTime) { BossBase::UpdateAttack(deltaTime); (void)deltaTime; }
void GuardianBoss::CheckDeath() { if (IsDead() && GetState() != BossState::Dead) { OnDead(); } }
void GuardianBoss::SetupAttacks() { RegisterAttack(std::make_unique<BossPunchAttack>()); RegisterAttack(std::make_unique<BossHeavyPunchAttack>()); }
void GuardianBoss::SetupPhaseData() {}
void GuardianBoss::SetupWeakPoints() {}

void GuardianBoss::FaceTarget(float deltaTime)
{
	Vector3 to{ GetTargetPosition().x - GetPosition().x, 0.0f, GetTargetPosition().z - GetPosition().z };
	const float lenSq = to.x * to.x + to.z * to.z;
	if (lenSq <= 0.0001f) { return; }
	const float desiredYaw = std::atan2(-to.x, to.z);
	float currentYaw = GetYaw();
	float diff = WrapAngle(desiredYaw - currentYaw);
	const float maxStep = rotateSpeed_ * deltaTime;
	diff = std::clamp(diff, -maxStep, maxStep);
	SetYaw(currentYaw + diff);
}

void GuardianBoss::ChangeBossState(BossState newState)
{
	if (GetStateMachine()) { GetStateMachine()->ChangeState(*this, newState); }
	else { SetState(newState); }
}

void GuardianBoss::EnterIdle() { ChangeBossState(BossState::Idle); runtime_.stateTimer = 0.0f; runtime_.currentActionName = "Idle"; }
void GuardianBoss::EnterMove() { ChangeBossState(BossState::Move); runtime_.stateTimer = 0.0f; runtime_.currentActionName = "Move"; }
void GuardianBoss::EnterAttack(const char* actionName) { ChangeBossState(BossState::Attack); runtime_.stateTimer = 0.0f; runtime_.currentActionName = actionName; }

void GuardianBoss::UpdatePhaseTransition()
{
	if (!runtime_.isPhase2 && GetHPRate() <= phaseSettings_.phase2HpRate)
	{
		runtime_.isPhase2 = true;
		SetPhase(BossPhase::Phase2);
	}
}

bool GuardianBoss::StartAttackByDistance()
{
	if (!GetAttackComponent() || GetAttackComponent()->IsAttacking()) { return false; }
	const float d = GetDistanceToTargetXZ();
	if (d <= attackSettings_.meleeRange)
	{
		if (GetAttackComponent()->StartAttackByName("HeavyPunch") || GetAttackComponent()->StartAttackByName("Punch"))
		{ EnterAttack("StompSmash"); return true; }
	}
	if (d <= attackSettings_.chargeRange && chargeTimer_ <= 0.0f)
	{
		EnterAttack("Charge");
		runtime_.attackCooldownTimer = attackSettings_.chargeCooldown;
		chargeTimer_ = runtime_.isPhase2 ? 1.2f : 0.8f;
		return true;
	}
	if (d <= attackSettings_.shockwaveRange)
	{
		if (GetAttackComponent()->StartAttackByName("Punch")) { EnterAttack("Shockwave"); return true; }
	}
	return false;
}

BTNodeResult GuardianBoss::TickBehaviorTree(float deltaTime)
{
	if (!behaviorRoot_) { return BTNodeResult::Failure; }
	return behaviorRoot_->Tick(deltaTime);
}

void GuardianBoss::BuildBehaviorTree()
{
	auto root = std::make_unique<BTSelectorNode>();
	auto attackSeq = std::make_unique<BTSequenceNode>();
	attackSeq->AddChild(std::make_unique<BTConditionNode>([this]() { return runtime_.attackCooldownTimer <= 0.0f; }));
	attackSeq->AddChild(std::make_unique<BTConditionNode>([this]() { return GetState() != BossState::Attack; }));
	attackSeq->AddChild(std::make_unique<BTActionNode>([this](float) { return StartAttackByDistance() ? BTNodeResult::Success : BTNodeResult::Failure; }));
	root->AddChild(std::move(attackSeq));
	root->AddChild(std::make_unique<BTActionNode>([this](float) {
		const float d = GetDistanceToTargetXZ();
		if (runtime_.currentActionName == "Charge")
		{
			if (chargeTimer_ <= 0.0f) { runtime_.attackCooldownTimer = attackSettings_.attackCooldown * (runtime_.isPhase2 ? phaseSettings_.phase2CooldownMultiplier : 1.0f); EnterIdle(); }
			return BTNodeResult::Running;
		}
		if (GetState() == BossState::Attack)
		{
			if (GetAttackComponent() && !GetAttackComponent()->IsAttacking()) { runtime_.attackCooldownTimer = attackSettings_.attackCooldown * (runtime_.isPhase2 ? phaseSettings_.phase2CooldownMultiplier : 1.0f); EnterIdle(); }
			return BTNodeResult::Running;
		}
		if (d > attackSettings_.moveStartDistance) { EnterMove(); }
		else if (d <= attackSettings_.moveStopDistance) { EnterIdle(); }
		return BTNodeResult::Success;
	}));
	behaviorRoot_ = std::move(root);
}

float GuardianBoss::GetCurrentMoveSpeed() const
{
	const float base = 3.2f;
	return runtime_.isPhase2 ? base * phaseSettings_.phase2MoveSpeedMultiplier : base;
}

void GuardianBoss::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Begin("PlainsGuardianBoss");
	ImGui::Text("State: %d", static_cast<int>(GetState()));
	ImGui::Text("Phase2: %s", runtime_.isPhase2 ? "true" : "false");
	ImGui::Text("Action: %s", runtime_.currentActionName.c_str());
	ImGui::Text("HP: %.1f/%.1f", GetHP(), GetMaxHP());
	ImGui::Text("Cooldown: %.2f", runtime_.attackCooldownTimer);
	ImGui::End();
#endif
}


bool GuardianBoss::MoveAlongPath(float deltaTime)
{
	if (!pathSettings_.enabled || !wallObstacleAABBs_) { return false; }
	EnemyAStarNavigator::Settings s = navigator_.GetSettings();
	s.cellSize = pathSettings_.gridSize;
	s.agentRadius = pathSettings_.obstacleExpandRadius + pathSettings_.stuckRepathExpandBonus;
	s.searchRangeCells = static_cast<int>(pathSettings_.searchRadius);
	s.repathIntervalSec = pathSettings_.repathInterval;
	s.waypointReachDistance = pathSettings_.waypointReachDistance;
	s.disableCornerCutting = pathSettings_.cornerCuttingDisabled;
	navigator_.SetSettings(s);
	pathBlockingObstacleAABBs_ = *wallObstacleAABBs_;
	navigator_.SetWorldAABBs(&pathBlockingObstacleAABBs_);
	pathState_.lastRepathTimer += deltaTime;
	navigator_.TickTemporaryBlocks(deltaTime);
	const Vector3 targetPos = GetTargetPosition();
	pathState_.targetMovedDistanceForRepath = LengthXZ(targetPos - pathState_.lastPathTargetPos);
	if (pathState_.targetMovedDistanceForRepath >= pathSettings_.targetRepathThreshold || isStuck_) { navigator_.Reset(); }
	const Vector3 selfPos = GetPosition();
	if (navigator_.GetNextWaypoint(selfPos, targetPos, selfPos.y, deltaTime, pathState_.currentWaypoint))
	{
		pathState_.found = true;
		pathState_.failureReason = "None";
		pathState_.lastPathTargetPos = targetPos;
		Vector3 dir = pathState_.currentWaypoint - selfPos;
		dir.y = 0.0f;
		const float dirLenSq = dir.x * dir.x + dir.z * dir.z;
		if (dirLenSq <= 0.0001f) { movementVelocity_ = Vector3{ 0.0f, 0.0f, 0.0f }; return false; }
		const float dirLen = std::sqrt(dirLenSq);
		dir.x /= dirLen;
		dir.z /= dirLen;
		const float speed = (runtime_.currentActionName == "Charge") ? GetCurrentMoveSpeed() * 2.3f : GetCurrentMoveSpeed();
		movementVelocity_ = Vector3{ dir.x * speed, 0.0f, dir.z * speed };
		return true;
	}
	pathState_.found = false;
	pathState_.failureReason = "PathNotFound";
	movementVelocity_ = Vector3{ 0.0f, 0.0f, 0.0f };
	return false;
}

void GuardianBoss::UpdateStuckState(float deltaTime)
{
	stuckTimer_ += deltaTime;
	if (stuckTimer_ < 0.8f) { return; }
	const float moved = LengthXZ(GetPosition() - pathState_.lastStuckCheckPosition);
	isStuck_ = (GetState() == BossState::Move) && moved <= 0.2f && GetDistanceToTargetXZ() > attackSettings_.moveStopDistance;
	pathState_.lastMovedDistance = moved;
	if (isStuck_)
	{
		navigator_.AddTemporaryBlockedArea(GetPosition(), pathSettings_.temporaryBlockRadius, pathSettings_.temporaryBlockDuration, "StuckPosition");
		navigator_.Reset();
		pathSettings_.stuckRepathExpandBonus = std::min(pathSettings_.stuckRepathExpandBonus + 0.1f, pathSettings_.maxStuckRepathExpandBonus);
	}
	else
	{
		pathSettings_.stuckRepathExpandBonus = std::max(0.0f, pathSettings_.stuckRepathExpandBonus - 0.05f);
	}
	pathState_.lastStuckCheckPosition = GetPosition();
	stuckTimer_ = 0.0f;
}

bool GuardianBoss::TryLandOnObstacleTop(float)
{
	return false;
}

bool GuardianBoss::ResolveObstaclePenetrationXZ(float)
{
	if (!wallObstacleAABBs_) { return false; }
	Vector3 pos = GetPosition();
	const float half = 1.1f;
	bool resolved = false;
	for (const auto& o : *wallObstacleAABBs_)
	{
		if (pos.x + half <= o.min.x || pos.x - half >= o.max.x || pos.z + half <= o.min.z || pos.z - half >= o.max.z) { continue; }
		const float pushLeft = o.min.x - (pos.x + half);
		const float pushRight = o.max.x - (pos.x - half);
		const float pushBack = o.min.z - (pos.z + half);
		const float pushFront = o.max.z - (pos.z - half);
		float minAbs = std::abs(pushLeft);
		Vector3 push{ pushLeft, 0.0f, 0.0f };
		if (std::abs(pushRight) < minAbs) { minAbs = std::abs(pushRight); push = { pushRight, 0.0f, 0.0f }; }
		if (std::abs(pushBack) < minAbs) { minAbs = std::abs(pushBack); push = { 0.0f, 0.0f, pushBack }; }
		if (std::abs(pushFront) < minAbs) { push = { 0.0f, 0.0f, pushFront }; }
		pos.x += push.x; pos.z += push.z;
		resolved = true;
	}
	if (resolved) { SetPosition(pos); }
	return resolved;
}
