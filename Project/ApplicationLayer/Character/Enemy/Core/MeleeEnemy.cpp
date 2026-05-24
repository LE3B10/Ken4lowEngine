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
}

void MeleeEnemy::Update(float deltaTime)
{
	attackController_.Update(*this, deltaTime);

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

		int attackSelect = static_cast<int>(selectedAttackType_);
		const char* attackItems[] = { "Scratch", "OneTwo" };
		if (ImGui::Combo("AttackPattern", &attackSelect, attackItems, IM_ARRAYSIZE(attackItems)))
		{
			selectedAttackType_ = static_cast<MeleeAttackType>(attackSelect);
		}
		ImGui::Text("Selected Attack: %s", attackItems[attackSelect]);
		ImGui::Text("Current BT: %s", currentBehaviorName_);
		ImGui::Text("Distance To Target: %.2f", GetDistanceToTarget());
		ImGui::Text("Is Attacking: %s", attackController_.IsAttacking() ? "true" : "false");
		ImGui::Text("Current Attack: %s", attackController_.GetCurrentAttackName());
		ImGui::Text("Attack Elapsed: %.2f", attackController_.GetAttackElapsed());
		ImGui::Text("Current Step: %d", attackController_.GetCurrentStepIndex());
		ImGui::Text("Cooldown Remaining: %.2f", attackController_.GetCooldownRemaining());
		ImGui::Text("Attack Active: %s", attackController_.IsCurrentStepActive() ? "true" : "false");
		ImGui::Text("Last Hit: %s", attackController_.WasLastHitSuccess() ? "Hit" : "Miss");

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
				ImGui::SliderFloat("OneTwo Recovery", &oneTwo->recoveryTime, 0.01f, 2.0f);
				ImGui::SliderFloat("OneTwo Cooldown", &oneTwo->cooldown, 0.01f, 3.0f);
				ImGui::TreePop();
			}
		}
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

Vector3 MeleeEnemy::GetTargetPositionForAttack() const
{
	return GetTargetPosition();
}

bool MeleeEnemy::IsTargetInDetectRange() const { return GetDistanceToTarget() <= detectRange_; }
bool MeleeEnemy::IsTargetInMeleeRange() const { return GetDistanceToTarget() <= meleeAttackRange_; }
bool MeleeEnemy::IsAttackCooldownReady() const { return attackController_.CanStartAttack(); }

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
	if (!attackController_.IsAttacking() && attackController_.CanStartAttack())
	{
		// 攻撃パターンをデータとして扱い、ScratchとOneTwoを同じ制御経路で実行する
		attackController_.StartAttack(selectedAttackType_);
		animState_ = AnimState::Attack;
	}
	currentBehaviorName_ = (selectedAttackType_ == MeleeAttackType::OneTwo) ? "OneTwoAttack" : "ScratchAttack";
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

void MeleeEnemy::ApplyAttackMove(const Vector3& horizontalVelocity)
{
	SetVelocity({ horizontalVelocity.x, GetVelocity().y, horizontalVelocity.z });
}

void MeleeEnemy::NotifyAttackHit(int, const Vector3&)
{
	// TODO: Playerへの実ダメージ処理の接続先が確定したらここで適用する
}

void MeleeEnemy::EvaluateBehavior(float deltaTime)
{
	if (IsDeadCondition()) { DeadAction(); return; }
	if (attackController_.IsAttacking()) { MeleeAttackAction(); return; }
	if (HasTarget() && IsTargetInMeleeRange())
	{
		if (IsAttackCooldownReady()) { MeleeAttackAction(); return; }
	}
	if (HasTarget() && IsTargetInDetectRange()) { ChaseTargetAction(); return; }
	WanderAction(deltaTime);
}
