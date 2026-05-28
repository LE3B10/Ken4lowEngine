#define NOMINMAX
#include "GuardianBoss.h"
#include "Attacks/BossPunchAttack.h"
#include "Attacks/BossHeavyPunchAttack.h"
#include "BehaviorTree/BTActionNode.h"
#include "BehaviorTree/BTSelectorNode.h"
#include "Wireframe.h"

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

Vector3 NormalizeXZSafe(const Vector3& v, const Vector3& fallback = { 0.0f, 0.0f, 1.0f })
{
	const float len = LengthXZ(v);
	if (len <= 0.0001f) { return fallback; }
	return { v.x / len, 0.0f, v.z / len };
}

const char* ToTargetTypeName(GuardianBoss::BossTargetType type)
{
	switch (type)
	{
	case GuardianBoss::BossTargetType::Player: return "Player";
	case GuardianBoss::BossTargetType::DummyTarget: return "DummyTarget";
	case GuardianBoss::BossTargetType::None:
	default: return "None";
	}
}
}

void GuardianBoss::SetupBoss()
{
	HumanoidBossBase::SetupBoss();
	SetPhase(BossPhase::Phase1);
	runtime_ = {};
	targetState_ = {};
	chargeTimer_ = 0.0f;
	phaseTransitionTimer_ = 0.0f;
	ChangeBossState(BossState::Idle);
	BuildBehaviorTree(); // 平原ボスの意思決定はBTに集約する。
	ApplySkinToAllParts("Characters/zombie.dds");
}

void GuardianBoss::SetDebugDummyTarget(const Vector3& position, bool exists)
{
	hasDebugDummyTarget_ = exists;
	debugDummyTargetPosition_ = position;
}

void GuardianBoss::OnDamaged(float damage)
{
	if (GetState() == BossState::Dead) { return; }
	BossBase::OnDamaged(damage);
}

void GuardianBoss::OnDead() { ChangeBossState(BossState::Dead); }
void GuardianBoss::OnCollision(Collider* other) { (void)other; }

void GuardianBoss::Draw()
{
	BossBase::Draw();
	UpdateNavigationObstacleList();

	if (targetState_.hasTarget)
	{
		const Vector3 boss = GetCenterPosition() + Vector3{ 0.0f, 0.25f, 0.0f };
		const Vector3 target = targetState_.position + Vector3{ 0.0f, 0.25f, 0.0f };
		Wireframe::GetInstance()->DrawSphere(targetState_.position, 0.55f, { 0.1f, 1.0f, 0.2f, 1.0f });
		Wireframe::GetInstance()->DrawLine(boss, target, targetState_.isTargetVisible ? Vector4{ 0.1f, 1.0f, 0.2f, 1.0f } : Vector4{ 1.0f, 0.15f, 0.15f, 1.0f });
	}

	if (runtime_.currentActionName == "StepAttack")
	{
		Wireframe::GetInstance()->DrawLine(
			stepAttackStartPosition_ + Vector3{ 0.0f, 0.35f, 0.0f },
			stepAttackStartPosition_ + stepAttackDirection_ * attackSettings_.stepAttackDistance + Vector3{ 0.0f, 0.35f, 0.0f },
			{ 1.0f, 0.6f, 0.1f, 1.0f });
		Wireframe::GetInstance()->DrawSphere(GetPosition(), attackSettings_.stepAttackRadius, { 1.0f, 0.45f, 0.1f, 0.35f });
	}

	if (runtime_.currentActionName == "Shockwave")
	{
		const Vector3 center = shockwaveOrigin_ + shockwaveDirection_ * (attackSettings_.shockwaveRadius * 0.65f);
		Wireframe::GetInstance()->DrawLine(shockwaveOrigin_ + Vector3{ 0.0f, 0.25f, 0.0f }, center + Vector3{ 0.0f, 0.25f, 0.0f }, { 0.3f, 0.75f, 1.0f, 1.0f });
		Wireframe::GetInstance()->DrawCircle(center + Vector3{ 0.0f, 0.05f, 0.0f }, attackSettings_.shockwaveRadius, 32, { 0.2f, 0.7f, 1.0f, 0.75f });
	}

	if (runtime_.currentActionName == "FarShot")
	{
		Wireframe::GetInstance()->DrawLine(farShotOrigin_, farShotOrigin_ + farShotDirection_ * attackSettings_.farShotSpeed, { 0.9f, 0.4f, 1.0f, 1.0f });
	}

	if (runtime_.isChangingPhase)
	{
		Wireframe::GetInstance()->DrawSphere(GetCenterPosition(), 5.5f, { 1.0f, 0.15f, 0.85f, 0.6f });
	}

	if (obstacleDebugDrawEnabled_)
	{
		for (const auto& obstacle : pathBlockingObstacleAABBs_)
		{
			Wireframe::GetInstance()->DrawAABB(obstacle, { 1.0f, 0.2f, 0.2f, 0.35f });
		}

		for (const auto& inflated : navigator_.GetInflatedObstacleAABBs())
		{
			Wireframe::GetInstance()->DrawAABB(inflated, { 1.0f, 0.7f, 0.2f, 0.35f });
		}
	}

	if (pathDebugDrawEnabled_)
	{
		const auto& path = navigator_.GetCurrentPath();
		for (size_t i = 1; i < path.size(); ++i)
		{
			Wireframe::GetInstance()->DrawLine(
				path[i - 1] + Vector3{ 0.0f, 0.15f, 0.0f },
				path[i] + Vector3{ 0.0f, 0.15f, 0.0f },
				{ 0.2f, 0.8f, 1.0f, 1.0f });
		}

		Wireframe::GetInstance()->DrawSphere(
			pathState_.currentWaypoint + Vector3{ 0.0f, 0.2f, 0.0f },
			0.4f,
			{ 1.0f, 0.3f, 0.1f, 1.0f });

		if (pathState_.lineBlocked)
		{
			Wireframe::GetInstance()->DrawLine(
				pathState_.blockedSegmentFrom + Vector3{ 0.0f, 0.25f, 0.0f },
				pathState_.blockedSegmentTo + Vector3{ 0.0f, 0.25f, 0.0f },
				{ 1.0f, 0.1f, 0.1f, 1.0f });
		}
	}
}

void GuardianBoss::UpdateState(float deltaTime)
{
	runtime_.actionTimer += deltaTime;
	UpdateCooldownTimers(deltaTime);
	UpdateNavigationObstacleList();
	UpdateTargetState(deltaTime);
	CheckDeath();
	if (GetState() == BossState::Dead) { return; }
	FaceTarget(deltaTime);
	TickBehaviorTree(deltaTime);
}

void GuardianBoss::UpdateMovement(float deltaTime)
{
	if (runtime_.isChangingPhase) { StopMove(); return; }
	if (GetState() != BossState::Move) { return; }
	UpdateStuckState(deltaTime);
	if (MoveAlongPath(deltaTime))
	{
		runtime_.walkAnimTimer += deltaTime;
	}
}

void GuardianBoss::UpdateAttack(float deltaTime) { BossBase::UpdateAttack(deltaTime); (void)deltaTime; }
void GuardianBoss::CheckDeath() { if (IsDead() && GetState() != BossState::Dead) { OnDead(); } }
void GuardianBoss::SetupAttacks() { RegisterAttack(std::make_unique<BossPunchAttack>()); RegisterAttack(std::make_unique<BossHeavyPunchAttack>()); }
void GuardianBoss::SetupPhaseData() {}
void GuardianBoss::SetupWeakPoints() {}

void GuardianBoss::FaceTarget(float deltaTime)
{
	if (!targetState_.hasTarget) { return; }
	Vector3 to{ targetState_.position.x - GetPosition().x, 0.0f, targetState_.position.z - GetPosition().z };
	const float lenSq = to.x * to.x + to.z * to.z;
	if (lenSq <= 0.0001f) { return; }
	const float desiredYaw = std::atan2(-to.x, to.z);
	float currentYaw = GetYaw();
	float diff = WrapAngle(desiredYaw - currentYaw);
	const float maxStep = rotateSpeed_ * deltaTime;
	diff = std::clamp(diff, -maxStep, maxStep);
	SetYaw(currentYaw + diff);
}

void GuardianBoss::UpdateTargetState(float)
{
	// 追加: DebugSceneでもボス挙動を確認できるよう、DummyTargetを攻撃対象として扱う。
	BossTargetType selected = BossTargetType::None;
	Vector3 selectedPosition{};

	const bool hasPlayerTarget = LengthXZ(GetTargetPosition()) > 0.0001f;
	if (preferredTargetType_ == BossTargetType::DummyTarget && hasDebugDummyTarget_)
	{
		selected = BossTargetType::DummyTarget;
		selectedPosition = debugDummyTargetPosition_;
	}
	else if (preferredTargetType_ == BossTargetType::Player && hasPlayerTarget)
	{
		selected = BossTargetType::Player;
		selectedPosition = GetTargetPosition();
	}
	else if (hasPlayerTarget)
	{
		selected = BossTargetType::Player;
		selectedPosition = GetTargetPosition();
	}
	else if (hasDebugDummyTarget_)
	{
		selected = BossTargetType::DummyTarget;
		selectedPosition = debugDummyTargetPosition_;
	}

	targetState_.targetType = selected;
	targetState_.hasTarget = selected != BossTargetType::None;
	targetState_.targetName = ToTargetTypeName(selected);
	if (!targetState_.hasTarget)
	{
		targetState_.position = {};
		targetState_.direction = {};
		targetState_.directDistance = 0.0f;
		targetState_.pathDistance = 0.0f;
		targetState_.isTargetVisible = false;
		targetState_.isInMeleeRange = false;
		targetState_.isInMidRange = false;
		targetState_.isInFarRange = false;
		return;
	}

	SetTargetPosition(selectedPosition);
	targetState_.position = selectedPosition;
	Vector3 toTarget = selectedPosition - GetPosition();
	toTarget.y = 0.0f;
	targetState_.directDistance = LengthXZ(toTarget);
	targetState_.direction = NormalizeXZSafe(toTarget, runtime_.lastMoveDirection);
	targetState_.pathDistance = CalculateCurrentPathDistance();
	navigator_.SetWorldAABBs(&pathBlockingObstacleAABBs_);

	int blockedIdx = -1;
	targetState_.isTargetVisible = !navigator_.IsSegmentBlockedByObstacle(GetPosition(), selectedPosition, GetPosition().y, &blockedIdx);
	const float melee = attackSettings_.meleeRange * GetRangeMultiplier();
	const float mid = attackSettings_.midRange * GetRangeMultiplier();
	const float far = attackSettings_.farRange * GetRangeMultiplier();
	targetState_.isInMeleeRange = targetState_.directDistance <= melee;
	targetState_.isInMidRange = targetState_.directDistance > melee && targetState_.directDistance <= mid;
	targetState_.isInFarRange = targetState_.directDistance > mid && targetState_.directDistance <= far;
}

float GuardianBoss::CalculateCurrentPathDistance() const
{
	const auto& path = navigator_.GetCurrentPath();
	if (path.size() < 2)
	{
		return targetState_.directDistance;
	}

	float total = 0.0f;
	for (size_t i = 1; i < path.size(); ++i)
	{
		total += LengthXZ(path[i] - path[i - 1]);
	}
	return total;
}

void GuardianBoss::UpdateCooldownTimers(float deltaTime)
{
	runtime_.attackCooldownTimer = std::max(0.0f, runtime_.attackCooldownTimer - deltaTime);
	runtime_.stepAttackCooldownTimer = std::max(0.0f, runtime_.stepAttackCooldownTimer - deltaTime);
	runtime_.chargeCooldownTimer = std::max(0.0f, runtime_.chargeCooldownTimer - deltaTime);
	runtime_.shockwaveCooldownTimer = std::max(0.0f, runtime_.shockwaveCooldownTimer - deltaTime);
	runtime_.farShotCooldownTimer = std::max(0.0f, runtime_.farShotCooldownTimer - deltaTime);
	chargeTimer_ = std::max(0.0f, chargeTimer_ - deltaTime);
}

float GuardianBoss::GetCooldownMultiplier() const
{
	return runtime_.isPhase2 ? phaseSettings_.phase2CooldownMultiplier : 1.0f;
}

float GuardianBoss::GetRangeMultiplier() const
{
	return runtime_.isPhase2 ? phaseSettings_.phase2AttackRangeMultiplier : 1.0f;
}

void GuardianBoss::ChangeBossState(BossState newState)
{
	if (GetStateMachine()) { GetStateMachine()->ChangeState(*this, newState); }
	else { SetState(newState); }
}

void GuardianBoss::EnterIdle() { ChangeBossState(BossState::Idle); runtime_.actionTimer = 0.0f; runtime_.isMoving = false; runtime_.currentActionName = "Idle"; }
void GuardianBoss::EnterMove() { ChangeBossState(BossState::Move); runtime_.actionTimer = 0.0f; runtime_.currentActionName = "Move"; }
void GuardianBoss::EnterAttack(const char* actionName) { ChangeBossState(BossState::Attack); runtime_.actionTimer = 0.0f; runtime_.isMoving = false; runtime_.currentActionName = actionName; }

BTNodeResult GuardianBoss::TickBehaviorTree(float deltaTime)
{
	if (!behaviorRoot_) { return BTNodeResult::Failure; }
	return behaviorRoot_->Tick(deltaTime);
}

void GuardianBoss::BuildBehaviorTree()
{
	auto root = std::make_unique<BTSelectorNode>();
	root->AddChild(std::make_unique<BTActionNode>([this](float) { return GetState() == BossState::Dead ? BTNodeResult::Success : BTNodeResult::Failure; }));
	root->AddChild(std::make_unique<BTActionNode>([this](float dt) { return TickPhase2Transition(dt); }));
	root->AddChild(std::make_unique<BTActionNode>([this](float dt) { return TickContinueCurrentAction(dt); }));
	root->AddChild(std::make_unique<BTActionNode>([this](float dt) { return TickMeleeAttack(dt); }));
	root->AddChild(std::make_unique<BTActionNode>([this](float dt) { return TickStepAttack(dt); }));
	root->AddChild(std::make_unique<BTActionNode>([this](float dt) { return TickShockwaveAttack(dt); }));
	root->AddChild(std::make_unique<BTActionNode>([this](float dt) { return TickFarShotAttack(dt); }));
	root->AddChild(std::make_unique<BTActionNode>([this](float dt) { return TickChaseTarget(dt); }));
	root->AddChild(std::make_unique<BTActionNode>([this](float) { EnterIdle(); return BTNodeResult::Success; }));
	behaviorRoot_ = std::move(root);
}

BTNodeResult GuardianBoss::TickContinueCurrentAction(float deltaTime)
{
	if (runtime_.currentActionName == "StepAttack")
	{
		UpdateStepAttack(deltaTime);
		return BTNodeResult::Running;
	}
	if (runtime_.currentActionName == "Shockwave")
	{
		UpdateShockwaveAttack(deltaTime);
		return BTNodeResult::Running;
	}
	if (runtime_.currentActionName == "FarShot")
	{
		UpdateFarShotAttack(deltaTime);
		return BTNodeResult::Running;
	}
	if (GetState() == BossState::Attack)
	{
		if (GetAttackComponent() && !GetAttackComponent()->IsAttacking())
		{
			runtime_.attackCooldownTimer = attackSettings_.attackCooldown * GetCooldownMultiplier();
			EnterIdle();
			return BTNodeResult::Success;
		}
		return BTNodeResult::Running;
	}
	return BTNodeResult::Failure;
}

BTNodeResult GuardianBoss::TickMeleeAttack(float)
{
	if (!attackSettings_.enableMelee || !targetState_.hasTarget || !targetState_.isInMeleeRange || !targetState_.isTargetVisible) { return BTNodeResult::Failure; }
	if (runtime_.attackCooldownTimer > 0.0f || !GetAttackComponent() || GetAttackComponent()->IsAttacking()) { return BTNodeResult::Failure; }
	if (GetAttackComponent()->StartAttackByName("HeavyPunch") || GetAttackComponent()->StartAttackByName("Punch"))
	{
		EnterAttack("MeleeAttack");
		return BTNodeResult::Running;
	}
	return BTNodeResult::Failure;
}

BTNodeResult GuardianBoss::TickStepAttack(float)
{
	if (!CanUseStepAttack()) { return BTNodeResult::Failure; }
	StartStepAttack();
	return BTNodeResult::Running;
}

void GuardianBoss::StartStepAttack()
{
	EnterAttack("StepAttack");
	stepAttackDirection_ = targetState_.direction;
	stepAttackStartPosition_ = GetPosition();
	runtime_.stepAttackCooldownTimer = attackSettings_.stepAttackCooldown * GetCooldownMultiplier();
}

void GuardianBoss::UpdateStepAttack(float deltaTime)
{
	const float moveDuration = std::max(0.05f, attackSettings_.stepAttackDuration);
	if (runtime_.actionTimer <= moveDuration)
	{
		const float maxStep = attackSettings_.stepAttackDistance / moveDuration;
		const float moveSpeed = std::max(attackSettings_.stepAttackSpeed, maxStep);
		Vector3 nextPos = GetPosition();
		nextPos.x += stepAttackDirection_.x * moveSpeed * deltaTime;
		nextPos.z += stepAttackDirection_.z * moveSpeed * deltaTime;
		SetPosition(nextPos);
		ResolveObstaclePenetrationXZ(deltaTime);
		runtime_.lastMoveDirection = stepAttackDirection_;
		movementVelocity_ = { stepAttackDirection_.x * moveSpeed, 0.0f, stepAttackDirection_.z * moveSpeed };
		return;
	}

	if (runtime_.actionTimer >= moveDuration + attackSettings_.stepAttackRecovery)
	{
		movementVelocity_ = {};
		EnterIdle();
	}
}

bool GuardianBoss::CanUseStepAttack() const
{
	return attackSettings_.enableStepAttack && targetState_.hasTarget && targetState_.isTargetVisible &&
		targetState_.isInMidRange && runtime_.stepAttackCooldownTimer <= 0.0f && GetState() != BossState::Attack;
}

BTNodeResult GuardianBoss::TickShockwaveAttack(float)
{
	if (!CanUseShockwave()) { return BTNodeResult::Failure; }
	StartShockwaveAttack();
	return BTNodeResult::Running;
}

void GuardianBoss::StartShockwaveAttack()
{
	EnterAttack("Shockwave");
	shockwaveOrigin_ = GetPosition();
	shockwaveDirection_ = targetState_.direction;
	runtime_.shockwaveCooldownTimer = attackSettings_.shockwaveCooldown * GetCooldownMultiplier();
}

void GuardianBoss::UpdateShockwaveAttack(float)
{
	if (runtime_.actionTimer >= attackSettings_.shockwaveDuration)
	{
		EnterIdle();
	}
}

bool GuardianBoss::CanUseShockwave() const
{
	if (!attackSettings_.enableShockwave || !targetState_.hasTarget || runtime_.shockwaveCooldownTimer > 0.0f || GetState() == BossState::Attack)
	{
		return false;
	}
	const float far = attackSettings_.farRange * GetRangeMultiplier();
	const bool usefulByDistance = targetState_.directDistance > attackSettings_.meleeRange && targetState_.directDistance <= far;
	return usefulByDistance && (!targetState_.isTargetVisible || targetState_.isInFarRange || runtime_.isPhase2);
}

BTNodeResult GuardianBoss::TickFarShotAttack(float)
{
	if (!CanUseFarShot()) { return BTNodeResult::Failure; }
	StartFarShotAttack();
	return BTNodeResult::Running;
}

void GuardianBoss::StartFarShotAttack()
{
	EnterAttack("FarShot");
	farShotOrigin_ = GetCenterPosition();
	farShotDirection_ = targetState_.direction;
	runtime_.farShotCooldownTimer = attackSettings_.farShotCooldown * GetCooldownMultiplier();
}

void GuardianBoss::UpdateFarShotAttack(float)
{
	if (runtime_.actionTimer >= attackSettings_.farShotLifetime)
	{
		EnterIdle();
	}
}

bool GuardianBoss::CanUseFarShot() const
{
	return attackSettings_.enableFarShot && targetState_.hasTarget && targetState_.directDistance > attackSettings_.farRange * 0.85f &&
		runtime_.farShotCooldownTimer <= 0.0f && GetState() != BossState::Attack;
}

BTNodeResult GuardianBoss::TickPhase2Transition(float deltaTime)
{
	if (runtime_.isChangingPhase)
	{
		UpdatePhase2Transition(deltaTime);
		return BTNodeResult::Running;
	}
	if (!ShouldEnterPhase2()) { return BTNodeResult::Failure; }
	StartPhase2();
	return BTNodeResult::Running;
}

bool GuardianBoss::ShouldEnterPhase2() const
{
	return !runtime_.isPhase2 && !runtime_.isChangingPhase && GetHPRate() <= phaseSettings_.phase2HpRate;
}

void GuardianBoss::StartPhase2()
{
	runtime_.isChangingPhase = true;
	phaseTransitionTimer_ = 0.0f;
	StopMove();
	if (GetAttackComponent()) { GetAttackComponent()->ForceEndCurrentAttack(); }
	ChangeBossState(BossState::PhaseTransition);
	runtime_.actionTimer = 0.0f;
	runtime_.currentActionName = "Phase2Transition";
}

void GuardianBoss::UpdatePhase2Transition(float deltaTime)
{
	phaseTransitionTimer_ += deltaTime;
	StopMove();
	if (phaseTransitionTimer_ >= phaseSettings_.phaseChangeDuration)
	{
		runtime_.isChangingPhase = false;
		runtime_.isPhase2 = true;
		SetPhase(BossPhase::Phase2);
		EnterIdle();
	}
}

BTNodeResult GuardianBoss::TickChaseTarget(float deltaTime)
{
	(void)deltaTime;
	if (!targetState_.hasTarget) { EnterIdle(); return BTNodeResult::Failure; }
	const float stopDistance = std::max(attackSettings_.meleeRange * 0.75f, 1.0f);
	if (targetState_.pathDistance <= stopDistance && targetState_.isTargetVisible)
	{
		EnterIdle();
		return BTNodeResult::Success;
	}
	EnterMove();
	return BTNodeResult::Success;
}

float GuardianBoss::GetCurrentMoveSpeed() const
{
	const float baseSpeed = 3.2f;
	return runtime_.isPhase2
		? baseSpeed * phaseSettings_.phase2MoveSpeedMultiplier
		: baseSpeed;
}

void GuardianBoss::DrawImGui()
{
#ifdef USE_IMGUI
	UpdateNavigationObstacleList();

	if (ImGui::Begin("PlainsGuardianBoss"))
	{
		BossBase::DrawImGui();
		if (ImGui::CollapsingHeader("ターゲット情報", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const char* targetItems[] = { "None", "Player", "DummyTarget" };
			int preferred = static_cast<int>(preferredTargetType_);
			if (ImGui::Combo("Preferred Target", &preferred, targetItems, 3))
			{
				preferredTargetType_ = static_cast<BossTargetType>(preferred);
			}
			ImGui::Text("Target: %s", targetState_.targetName.c_str());
			ImGui::Text("TargetType: %s", ToTargetTypeName(targetState_.targetType));
			ImGui::Text("HasTarget: %s", targetState_.hasTarget ? "true" : "false");
			ImGui::Text("DirectDistance: %.2f", targetState_.directDistance);
			ImGui::Text("PathDistance: %.2f", targetState_.pathDistance);
			ImGui::Text("Visible: %s", targetState_.isTargetVisible ? "true" : "false");
			ImGui::Text("Melee/Mid/Far: %s / %s / %s", targetState_.isInMeleeRange ? "true" : "false", targetState_.isInMidRange ? "true" : "false", targetState_.isInFarRange ? "true" : "false");
		}
		if (ImGui::CollapsingHeader("基本情報", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("State: %d", static_cast<int>(GetState()));
			ImGui::Text("CurrentAction: %s", runtime_.currentActionName.c_str());
			ImGui::Text("HP: %.1f/%.1f", GetHP(), GetMaxHP());
			ImGui::Text("isMoving: %s", runtime_.isMoving ? "true" : "false");
		}
		if (ImGui::CollapsingHeader("攻撃設定", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("Enable Melee", &attackSettings_.enableMelee);
			ImGui::Checkbox("Enable StepAttack", &attackSettings_.enableStepAttack);
			ImGui::Checkbox("Enable Charge", &attackSettings_.enableCharge);
			ImGui::Checkbox("Enable Shockwave", &attackSettings_.enableShockwave);
			ImGui::Checkbox("Enable FarShot", &attackSettings_.enableFarShot);
			ImGui::SliderFloat("Melee Range", &attackSettings_.meleeRange, 0.5f, 20.0f);
			ImGui::SliderFloat("Mid Range", &attackSettings_.midRange, 0.5f, 30.0f);
			ImGui::SliderFloat("Far Range", &attackSettings_.farRange, 0.5f, 60.0f);
			ImGui::SliderFloat("Melee Cooldown", &attackSettings_.attackCooldown, 0.0f, 8.0f);
			ImGui::SliderFloat("StepAttack Cooldown", &attackSettings_.stepAttackCooldown, 0.0f, 8.0f);
			ImGui::SliderFloat("Charge Cooldown", &attackSettings_.chargeCooldown, 0.0f, 8.0f);
			ImGui::SliderFloat("Shockwave Cooldown", &attackSettings_.shockwaveCooldown, 0.0f, 8.0f);
			ImGui::SliderFloat("FarShot Cooldown", &attackSettings_.farShotCooldown, 0.0f, 8.0f);
			ImGui::SliderFloat("StepAttack Distance", &attackSettings_.stepAttackDistance, 0.5f, 12.0f);
			ImGui::SliderFloat("StepAttack Speed", &attackSettings_.stepAttackSpeed, 1.0f, 30.0f);
			ImGui::SliderFloat("StepAttack Radius", &attackSettings_.stepAttackRadius, 0.5f, 12.0f);
			ImGui::SliderFloat("StepAttack Duration", &attackSettings_.stepAttackDuration, 0.05f, 2.0f);
			ImGui::SliderFloat("StepAttack Recovery", &attackSettings_.stepAttackRecovery, 0.0f, 2.0f);
			ImGui::SliderFloat("Shockwave Windup", &attackSettings_.shockwaveWindup, 0.0f, 2.0f);
			ImGui::SliderFloat("Shockwave Radius", &attackSettings_.shockwaveRadius, 0.5f, 20.0f);
			ImGui::SliderFloat("Shockwave Duration", &attackSettings_.shockwaveDuration, 0.05f, 3.0f);
			ImGui::SliderFloat("FarShot Speed", &attackSettings_.farShotSpeed, 1.0f, 40.0f);
			ImGui::SliderFloat("FarShot Lifetime", &attackSettings_.farShotLifetime, 0.05f, 5.0f);
		}
		if (ImGui::CollapsingHeader("第二フェーズ", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::SliderFloat("Phase2 HP Rate", &phaseSettings_.phase2HpRate, 0.1f, 0.9f);
			ImGui::SliderFloat("Phase Change Duration", &phaseSettings_.phaseChangeDuration, 0.1f, 6.0f);
			ImGui::SliderFloat("Phase2 Move Speed Mult", &phaseSettings_.phase2MoveSpeedMultiplier, 0.5f, 2.5f);
			ImGui::SliderFloat("Phase2 Cooldown Mult", &phaseSettings_.phase2CooldownMultiplier, 0.1f, 1.0f);
			ImGui::SliderFloat("Phase2 Range Mult", &phaseSettings_.phase2AttackRangeMultiplier, 0.5f, 2.0f);
			ImGui::Text("Phase2: %s", runtime_.isPhase2 ? "true" : "false");
			ImGui::Text("ChangingPhase: %s", runtime_.isChangingPhase ? "true" : "false");
			ImGui::Text("PhaseTimer: %.2f", phaseTransitionTimer_);
		}
		if (ImGui::CollapsingHeader("現在行動", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("currentActionName: %s", runtime_.currentActionName.c_str());
			ImGui::Text("Melee CT: %.2f", runtime_.attackCooldownTimer);
			ImGui::Text("Step CT: %.2f", runtime_.stepAttackCooldownTimer);
			ImGui::Text("Charge CT: %.2f", runtime_.chargeCooldownTimer);
			ImGui::Text("Shockwave CT: %.2f", runtime_.shockwaveCooldownTimer);
			ImGui::Text("FarShot CT: %.2f", runtime_.farShotCooldownTimer);
			ImGui::Text("isMoving: %s", runtime_.isMoving ? "true" : "false");
			ImGui::Text("lineBlocked: %s", pathState_.lineBlocked ? "true" : "false");
			ImGui::Text("Obstacle AABB Count: %d", pathState_.blockingObstacleAABBCount);
			ImGui::Text("Waypoint: (%.2f, %.2f, %.2f)", pathState_.currentWaypoint.x, pathState_.currentWaypoint.y, pathState_.currentWaypoint.z);
			ImGui::Text("PathDistance: %.2f", targetState_.pathDistance);
		}
		if (ImGui::CollapsingHeader("経路探索", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("経路探索を使う", &pathSettings_.enabled);
			ImGui::SliderFloat("再探索間隔", &pathSettings_.repathInterval, 0.05f, 2.0f);
			ImGui::SliderFloat("到達判定距離", &pathSettings_.waypointReachDistance, 0.5f, 2.5f);
			ImGui::SliderFloat("グリッドサイズ", &pathSettings_.gridSize, 0.5f, 2.5f);
			ImGui::SliderFloat("探索半径", &pathSettings_.searchRadius, 6.0f, 120.0f);
			ImGui::SliderFloat("障害物拡張半径", &pathSettings_.obstacleExpandRadius, 0.1f, 3.0f);
			ImGui::SliderFloat("一時ブロック時間", &pathSettings_.temporaryBlockDuration, 0.2f, 6.0f);
			ImGui::SliderFloat("一時ブロック半径", &pathSettings_.temporaryBlockRadius, 0.2f, 3.0f);
			ImGui::Checkbox("角抜け無効", &pathSettings_.cornerCuttingDisabled);
			ImGui::SliderFloat("再探索ターゲット閾値", &pathSettings_.targetRepathThreshold, 0.1f, 10.0f);
			ImGui::SliderFloat("スタック再探索拡張", &pathSettings_.stuckRepathExpandBonus, 0.0f, 4.0f);
			ImGui::SliderFloat("スタック再探索拡張最大", &pathSettings_.maxStuckRepathExpandBonus, 0.0f, 8.0f);
			ImGui::Checkbox("経路デバッグ描画", &pathDebugDrawEnabled_);
			ImGui::Checkbox("障害物デバッグ描画", &obstacleDebugDrawEnabled_);
			ImGui::Text("元の障害物AABB数: %d", pathState_.sourceObstacleAABBCount);
			ImGui::Text("A*へ渡す障害物AABB数: %d", pathState_.blockingObstacleAABBCount);
			ImGui::Text("経路あり: %s", pathState_.found ? "はい" : "いいえ");
			ImGui::Text("失敗理由: %s", pathState_.failureReason.c_str());
			ImGui::Text("最後のリパス理由: %s", pathState_.lastRepathReason.c_str());
			ImGui::Text("LineBlocked: %s", pathState_.lineBlocked ? "はい" : "いいえ");
			ImGui::Text("BlockedObstacle: %s", pathState_.blockedObstacleName.c_str());
		}
	}
	ImGui::End();
#endif
}

void GuardianBoss::UpdateNavigationObstacleList()
{
	pathBlockingObstacleAABBs_.clear();

	const auto* source = wallObstacleAABBs_;
	pathState_.sourceObstacleAABBCount = source ? static_cast<int>(source->size()) : 0;

	if (!source)
	{
		pathState_.blockingObstacleAABBCount = 0;
		return;
	}

	const Vector3 bossPos = GetPosition();

	for (const auto& aabb : *source)
	{
		const float sizeX = aabb.max.x - aabb.min.x;
		const float sizeY = aabb.max.y - aabb.min.y;
		const float sizeZ = aabb.max.z - aabb.min.z;

		// 床のように薄いAABBは経路障害物から除外し、壁や柱だけをA*へ渡す。
		if (sizeY <= 0.05f)
		{
			continue;
		}

		// ボスの足元より大きく下にあるAABBは、経路探索の障害物から除外する。
		if (aabb.max.y < bossPos.y - 1.5f)
		{
			continue;
		}

		// 極端に小さいAABBはノイズとして除外し、経路探索の誤反応を減らす。
		if (sizeX <= 0.05f || sizeZ <= 0.05f)
		{
			continue;
		}

		pathBlockingObstacleAABBs_.push_back(aabb);
	}

	pathState_.blockingObstacleAABBCount = static_cast<int>(pathBlockingObstacleAABBs_.size());
}

void GuardianBoss::StopMove()
{
	// BossBaseには速度APIがないため、停止状態は移動フラグだけで管理する。
	movementVelocity_ = Vector3{};
	runtime_.isMoving = false;
	pathState_.currentWaypoint = GetPosition();
}

bool GuardianBoss::MoveAlongPath(float deltaTime)
{
	if (!pathSettings_.enabled || !targetState_.hasTarget)
	{
		StopMove();
		return false;
	}

	const Vector3 currentPos = GetPosition();
	const Vector3 targetPos = targetState_.position;

	UpdateNavigationObstacleList();

	EnemyAStarNavigator::Settings settings = navigator_.GetSettings();
	settings.cellSize = pathSettings_.gridSize;
	settings.agentRadius = pathSettings_.obstacleExpandRadius + pathSettings_.stuckRepathExpandBonus;
	settings.searchRangeCells = static_cast<int>(pathSettings_.searchRadius);
	settings.repathIntervalSec = pathSettings_.repathInterval;
	settings.waypointReachDistance = pathSettings_.waypointReachDistance;
	settings.disableCornerCutting = pathSettings_.cornerCuttingDisabled;
	navigator_.SetSettings(settings);

	// MeleeEnemyと同じく、分類済みの回避対象AABBだけをnavigatorへ渡す。
	navigator_.SetWorldAABBs(&pathBlockingObstacleAABBs_);
	navigator_.TickTemporaryBlocks(deltaTime);
	pathState_.lastRepathTimer += deltaTime;

	pathState_.targetMovedDistanceForRepath = LengthXZ(targetPos - pathState_.lastPathTargetPos);
	if (pathState_.targetMovedDistanceForRepath >= pathSettings_.targetRepathThreshold || isStuck_)
	{
		navigator_.Reset();
		pathState_.lastRepathReason = isStuck_ ? "StuckForceRepath" : "TargetMoved";
	}

	Vector3 waypoint = targetPos;
	if (!navigator_.GetNextWaypoint(currentPos, targetPos, currentPos.y, deltaTime, waypoint))
	{
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

	pathState_.found = true;
	pathState_.failureReason = "None";
	pathState_.currentWaypoint = waypoint;
	pathState_.lastPathTargetPos = targetPos;
	pathState_.retryTimer = 0.0f;
	pathState_.lineBlocked = false;
	pathState_.blockedWaypointIndex = -1;
	pathState_.blockedObstacleName = "None";

	int blockedIdx = -1;
	if (navigator_.IsSegmentBlockedByObstacle(currentPos, waypoint, currentPos.y, &blockedIdx))
	{
		pathState_.lineBlocked = true;
		pathState_.blockedWaypointIndex = navigator_.GetCurrentPathIndex();
		pathState_.blockedObstacleName = (blockedIdx >= 0) ? ("Obstacle[" + std::to_string(blockedIdx) + "]") : "Unknown";
		pathState_.blockedSegmentFrom = currentPos;
		pathState_.blockedSegmentTo = waypoint;

		navigator_.Reset();
		navigator_.AddTemporaryBlockedArea(
			waypoint,
			pathSettings_.temporaryBlockRadius,
			pathSettings_.temporaryBlockDuration,
			"BossWaypointSegmentBlocked");

		pathState_.lastRepathReason = "WaypointSegmentBlocked";
		StopMove();
		return false;
	}

	Vector3 dir = waypoint - currentPos;
	dir.y = 0.0f;
	const float dirLen = LengthXZ(dir);
	if (dirLen <= 0.0001f)
	{
		StopMove();
		return false;
	}
	dir.x /= dirLen;
	dir.z /= dirLen;

	const float moveSpeed = GetCurrentMoveSpeed();
	Vector3 nextPos = currentPos;
	nextPos.x += dir.x * moveSpeed * deltaTime;
	nextPos.z += dir.z * moveSpeed * deltaTime;
	SetPosition(nextPos);

	if (!TryLandOnObstacleTop(deltaTime))
	{
		ResolveObstaclePenetrationXZ(deltaTime);
	}

	runtime_.isMoving = true;
	runtime_.lastMoveDirection = dir;
	movementVelocity_ = Vector3{ dir.x * moveSpeed, 0.0f, dir.z * moveSpeed };
	runtime_.currentActionName = "Chase";

	return true;
}

void GuardianBoss::UpdateStuckState(float deltaTime)
{
	stuckTimer_ += deltaTime;
	if (stuckTimer_ < 0.8f) { return; }
	const float moved = LengthXZ(GetPosition() - pathState_.lastStuckCheckPosition);
	isStuck_ = (GetState() == BossState::Move) && moved <= 0.2f && targetState_.pathDistance > attackSettings_.meleeRange;
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

bool GuardianBoss::ResolveObstaclePenetrationXZ(float deltaTime)
{
	(void)deltaTime;

	if (pathBlockingObstacleAABBs_.empty())
	{
		return false;
	}

	Vector3 pos = GetPosition();
	const Vector3 half{ 1.8f, 3.0f, 1.8f };

	bool resolved = false;

	for (const auto& o : pathBlockingObstacleAABBs_)
	{
		if (pos.y + half.y < o.min.y || pos.y - half.y > o.max.y)
		{
			continue;
		}

		const float overlapX = std::min(pos.x + half.x, o.max.x) - std::max(pos.x - half.x, o.min.x);
		const float overlapZ = std::min(pos.z + half.z, o.max.z) - std::max(pos.z - half.z, o.min.z);

		if (overlapX <= 0.0f || overlapZ <= 0.0f)
		{
			continue;
		}

		float pushX = 0.0f;
		float pushZ = 0.0f;

		if (overlapX < overlapZ)
		{
			pushX = pos.x < (o.min.x + o.max.x) * 0.5f ? -overlapX : overlapX;
		}
		else
		{
			pushZ = pos.z < (o.min.z + o.max.z) * 0.5f ? -overlapZ : overlapZ;
		}

		const float maxPush = 0.6f;
		pushX = std::clamp(pushX, -maxPush, maxPush);
		pushZ = std::clamp(pushZ, -maxPush, maxPush);

		pos.x += pushX;
		pos.z += pushZ;

		resolved = true;
	}

	if (resolved)
	{
		// 直接移動で障害物へめり込んだ場合に押し戻し、次フレームで再経路探索させる。
		SetPosition(pos);
		navigator_.Reset();
		pathState_.lastRepathReason = "BossObstaclePushResolve";
	}

	return resolved;
}
