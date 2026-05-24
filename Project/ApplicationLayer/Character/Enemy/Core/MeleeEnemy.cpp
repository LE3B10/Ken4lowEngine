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
	attackCooldownTimer_ = 0.0f;
	attackLockTimer_ = 0.0f;
	wanderTimer_ = 0.0f;
	currentBehaviorName_ = "Wander";
	animState_ = AnimState::Idle;
}

void MeleeEnemy::Update(float deltaTime)
{
	if (attackCooldownTimer_ > 0.0f) { attackCooldownTimer_ = std::max(0.0f, attackCooldownTimer_ - deltaTime); }
	if (attackLockTimer_ > 0.0f) { attackLockTimer_ = std::max(0.0f, attackLockTimer_ - deltaTime); }

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
	EnemyBase::DrawImGui();
#ifdef USE_IMGUI
	if (ImGui::TreeNode("MeleeEnemy"))
	{
		ImGui::SliderFloat("detectRange", &detectRange_, 1.0f, 50.0f);
		ImGui::SliderFloat("meleeAttackRange", &meleeAttackRange_, 0.5f, 10.0f);
		ImGui::SliderFloat("moveSpeed", &moveSpeed_, 0.1f, 10.0f);
		ImGui::SliderFloat("attackCooldown", &attackCooldown_, 0.1f, 5.0f);
		ImGui::SliderFloat("attackLockTime", &attackLockTime_, 0.05f, 2.0f);
		ImGui::Text("Current BT: %s", currentBehaviorName_);
		ImGui::Text("Distance To Target: %.2f", GetDistanceToTarget());
		ImGui::Text("Attack Cooldown Timer: %.2f", attackCooldownTimer_);
		ImGui::Text("Attack Lock Timer: %.2f", attackLockTimer_);
		ImGui::TreePop();
	}
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

bool MeleeEnemy::IsTargetInDetectRange() const { return GetDistanceToTarget() <= detectRange_; }
bool MeleeEnemy::IsTargetInMeleeRange() const { return GetDistanceToTarget() <= meleeAttackRange_; }
bool MeleeEnemy::IsAttackCooldownReady() const { return attackCooldownTimer_ <= 0.0f; }

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
	SetVelocity({ 0.0f, GetVelocity().y, 0.0f });
	animState_ = AnimState::Dead;
	currentBehaviorName_ = "DeadAction";
}

void MeleeEnemy::MeleeAttackAction()
{
	FaceToTarget();
	SetVelocity({ 0.0f, GetVelocity().y, 0.0f });
	if (IsAttackCooldownReady())
	{
		attackCooldownTimer_ = attackCooldown_;
		attackLockTimer_ = attackLockTime_;
		// TODO: Playerへの実ダメージ処理に接続する
		animState_ = AnimState::Attack;
	}
	currentBehaviorName_ = "MeleeAttackAction";
}

void MeleeEnemy::ChaseTargetAction()
{
	const Vector3 toTarget = GetTargetPosition() - GetCenterPosition();
	const Vector3 moveDir = NormalizeXZ(toTarget);
	SetVelocity({ moveDir.x * moveSpeed_, GetVelocity().y, moveDir.z * moveSpeed_ });
	FaceToTarget();
	animState_ = AnimState::Move;
	currentBehaviorName_ = "ChaseTargetAction";
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

void MeleeEnemy::EvaluateBehavior(float deltaTime)
{
	if (IsDeadCondition()) { DeadAction(); return; }
	if (HasTarget() && IsTargetInMeleeRange())
	{
		if (attackLockTimer_ > 0.0f || IsAttackCooldownReady()) { MeleeAttackAction(); return; }
	}
	if (HasTarget() && IsTargetInDetectRange()) { ChaseTargetAction(); return; }
	WanderAction(deltaTime);
}
