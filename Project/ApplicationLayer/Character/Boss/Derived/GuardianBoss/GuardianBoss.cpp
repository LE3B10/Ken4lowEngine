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
	Vector3 to{ GetTargetPosition().x - GetPosition().x, 0.0f, GetTargetPosition().z - GetPosition().z };
	const float lenSq = to.x * to.x + to.z * to.z;
	if (lenSq <= 0.0001f) { return; }
	const float len = std::sqrt(lenSq);
	to.x /= len; to.z /= len;
	const float speed = (runtime_.currentActionName == "Charge") ? GetCurrentMoveSpeed() * 2.3f : GetCurrentMoveSpeed();
	SetPosition({ GetPosition().x + to.x * speed * deltaTime, GetPosition().y, GetPosition().z + to.z * speed * deltaTime });
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
