#define NOMINMAX
#include "GuardianBoss.h"
#include "Attacks/BossPunchAttack.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace Ken4lowEngine;

namespace
{
	float Clamp(float v, float minValue, float maxValue)
	{
		return (v < minValue) ? minValue : (v > maxValue ? maxValue : v);
	}

	float WrapAngle(float angle)
	{
		while (angle > 3.14159265f) { angle -= 6.28318530f; }
		while (angle < -3.14159265f) { angle += 6.28318530f; }
		return angle;
	}
}

/// -------------------------------------------------------------
/// Guardian 固有初期化
/// -------------------------------------------------------------
void GuardianBoss::SetupBoss()
{
	// 人型ボス共通初期化
	HumanoidBossBase::SetupBoss();

	// フェーズ初期化
	SetPhase(BossPhase::Phase1);

	stateTimer_ = 0.0f;
	attackCooldownTimer_ = 0.0f;
	hasAppliedAttackHit_ = false;

	// ---------------------------------------------------------
	// 初期状態は Idle
	// ここも StateMachine 経由で合わせる
	// ---------------------------------------------------------
	ChangeBossState(BossState::Idle);

	// ---------------------------------------------------------
	// アニメーションコンポーネントへ Guardian 用パラメータを渡す
	// ---------------------------------------------------------
	if (GetAnimationComponent())
	{
		GetAnimationComponent()->SetWalkSpeed(6.0f);
		GetAnimationComponent()->SetWalkAmplitude(0.55f);
		GetAnimationComponent()->SetAttackDuration(attackDuration_);

		GetAnimationComponent()->ResetWalkTimer();
		GetAnimationComponent()->ResetAttackTimer();
		GetAnimationComponent()->ResetAllPose(1.0f);
	}
}

/// -------------------------------------------------------------
/// ダメージ
/// 軽いひるみへ移行
/// -------------------------------------------------------------
void GuardianBoss::OnDamaged(float damage)
{
	if (GetState() == BossState::Dead)
	{
		return;
	}

	// HP減算は基底側に任せる
	BossBase::OnDamaged(damage);

	// 生きていたらひるみへ
	if (!IsDead())
	{
		BeginStaggerState();
	}
}

/// -------------------------------------------------------------
/// 死亡
/// -------------------------------------------------------------
void GuardianBoss::OnDead()
{
	ChangeBossState(BossState::Dead);
	stateTimer_ = 0.0f;

	if (GetAnimationComponent())
	{
		GetAnimationComponent()->ResetAllPose(1.0f);
	}
}

/// -------------------------------------------------------------
/// 衝突
/// 今は空実装
/// -------------------------------------------------------------
void GuardianBoss::OnCollision(Collider* other)
{
	(void)other;
}

/// -------------------------------------------------------------
/// 状態更新
/// Guardian の思考をここで決める
/// -------------------------------------------------------------
void GuardianBoss::UpdateState(float deltaTime)
{
	stateTimer_ += deltaTime;
	attackCooldownTimer_ = std::max(0.0f, attackCooldownTimer_ - deltaTime);

	CheckDeath();
	if (GetState() == BossState::Dead)
	{
		return;
	}

	switch (GetState())
	{
	case BossState::Intro:
	{
		BeginIdleState();
		break;
	}

	case BossState::Idle:
	{
		FaceTarget(deltaTime);

		const float distance = GetDistanceToTargetXZ();

		// 攻撃条件成立
		if (distance <= attackRange_ && attackCooldownTimer_ <= 0.0f)
		{
			BeginAttackState();
		}
		// 遠ければ移動へ
		else if (distance > moveStartDistance_)
		{
			BeginMoveState();
		}
		break;
	}

	case BossState::Move:
	{
		FaceTarget(deltaTime);

		const float distance = GetDistanceToTargetXZ();

		// 攻撃条件成立
		if (distance <= attackRange_ && attackCooldownTimer_ <= 0.0f)
		{
			BeginAttackState();
		}
		// 十分近づいたら待機
		else if (distance <= moveStartDistance_)
		{
			BeginIdleState();
		}
		break;
	}

	case BossState::Attack:
	{
		FaceTarget(deltaTime);

		// 少し待ってから攻撃終了判定
		if (stateTimer_ >= 0.05f)
		{
			if (GetAttackComponent() && !GetAttackComponent()->IsAttacking())
			{
				attackCooldownTimer_ = attackCooldown_;
				hasAppliedAttackHit_ = false;

				if (GetDistanceToTargetXZ() <= moveStartDistance_)
				{
					BeginIdleState();
				}
				else
				{
					BeginMoveState();
				}
			}
		}
		break;
	}

	case BossState::Stagger:
	{
		if (stateTimer_ >= staggerDuration_)
		{
			BeginIdleState();
		}
		break;
	}

	case BossState::Down:
	case BossState::PhaseTransition:
	{
		break;
	}

	case BossState::Dead:
	default:
	{
		break;
	}
	}
}

/// -------------------------------------------------------------
/// 移動更新
/// Move状態のときだけ前進する
/// -------------------------------------------------------------
void GuardianBoss::UpdateMovement(float deltaTime)
{
	if (GetState() != BossState::Move)
	{
		return;
	}

	Vector3 toTarget
	{
		GetTargetPosition().x - GetPosition().x,
		0.0f,
		GetTargetPosition().z - GetPosition().z
	};

	const float lenSq = toTarget.x * toTarget.x + toTarget.z * toTarget.z;
	if (lenSq <= 0.0001f)
	{
		return;
	}

	const float len = std::sqrt(lenSq);
	toTarget.x /= len;
	toTarget.z /= len;

	Vector3 newPos = GetPosition();
	newPos.x += toTarget.x * moveSpeed_ * deltaTime;
	newPos.z += toTarget.z * moveSpeed_ * deltaTime;
	SetPosition(newPos);
}

/// -------------------------------------------------------------
/// 攻撃更新
/// 実際の攻撃処理は基底側 + AttackComponent に任せる
/// 見た目アニメは AnimationComponent 側で更新される
/// -------------------------------------------------------------
void GuardianBoss::UpdateAttack(float deltaTime)
{
	BossBase::UpdateAttack(deltaTime);
	(void)deltaTime;
}

/// -------------------------------------------------------------
/// 死亡チェック
/// -------------------------------------------------------------
void GuardianBoss::CheckDeath()
{
	if (IsDead() && GetState() != BossState::Dead)
	{
		OnDead();
	}
}

/// -------------------------------------------------------------
/// 攻撃登録
/// -------------------------------------------------------------
void GuardianBoss::SetupAttacks()
{
	RegisterAttack(std::make_unique<BossPunchAttack>());
}

/// -------------------------------------------------------------
/// フェーズ設定
/// 今は空でOK
/// -------------------------------------------------------------
void GuardianBoss::SetupPhaseData()
{}

/// -------------------------------------------------------------
/// 弱点設定
/// 今は空でOK
/// -------------------------------------------------------------
void GuardianBoss::SetupWeakPoints()
{}

/// -------------------------------------------------------------
/// ターゲット方向へ向く
/// -------------------------------------------------------------
void GuardianBoss::FaceTarget(float deltaTime)
{
	Vector3 toTarget
	{
		GetTargetPosition().x - GetPosition().x,
		0.0f,
		GetTargetPosition().z - GetPosition().z
	};

	const float lenSq = toTarget.x * toTarget.x + toTarget.z * toTarget.z;
	if (lenSq <= 0.0001f)
	{
		return;
	}

	const float desiredYaw = std::atan2(-toTarget.x, toTarget.z);
	float currentYaw = GetYaw();

	float diff = WrapAngle(desiredYaw - currentYaw);
	const float maxStep = rotateSpeed_ * deltaTime;
	diff = Clamp(diff, -maxStep, maxStep);

	currentYaw += diff;
	SetYaw(currentYaw);
}

/// -------------------------------------------------------------
/// ターゲットまでのXZ距離
/// -------------------------------------------------------------
float GuardianBoss::GetDistanceToTargetXZ() const
{
	const Vector3 pos = GetPosition();
	const Vector3 target = GetTargetPosition();

	const float dx = target.x - pos.x;
	const float dz = target.z - pos.z;
	return std::sqrt(dx * dx + dz * dz);
}

/// -------------------------------------------------------------
/// 状態変更ヘルパー
/// StateMachine と BossBase::state_ のズレを防ぐため、
/// 状態変更は必ずここを通す
/// -------------------------------------------------------------
void GuardianBoss::ChangeBossState(BossState newState)
{
	if (GetStateMachine())
	{
		GetStateMachine()->ChangeState(*this, newState);
	}
	else
	{
		// 念のためフォールバック
		SetState(newState);
	}
}

/// -------------------------------------------------------------
/// Attack 開始時共通処理
/// -------------------------------------------------------------
void GuardianBoss::BeginAttackState()
{
	ChangeBossState(BossState::Attack);
	stateTimer_ = 0.0f;
	hasAppliedAttackHit_ = false;

	if (GetAnimationComponent())
	{
		GetAnimationComponent()->ResetAttackTimer();
	}
}

/// -------------------------------------------------------------
/// Move 開始時共通処理
/// -------------------------------------------------------------
void GuardianBoss::BeginMoveState()
{
	ChangeBossState(BossState::Move);
	stateTimer_ = 0.0f;

	if (GetAnimationComponent())
	{
		GetAnimationComponent()->ResetWalkTimer();
	}
}

/// -------------------------------------------------------------
/// Idle 開始時共通処理
/// -------------------------------------------------------------
void GuardianBoss::BeginIdleState()
{
	ChangeBossState(BossState::Idle);
	stateTimer_ = 0.0f;
}

/// -------------------------------------------------------------
/// Stagger 開始時共通処理
/// -------------------------------------------------------------
void GuardianBoss::BeginStaggerState()
{
	ChangeBossState(BossState::Stagger);
	stateTimer_ = 0.0f;
}

/// -------------------------------------------------------------
/// 攻撃ヒットタイミング
/// 今は将来拡張用に残す
/// 現段階では BossPunchAttack 側に判定を寄せる方針
/// -------------------------------------------------------------
void GuardianBoss::TryAttackHit()
{
	// 今は未使用
}

/// -------------------------------------------------------------
/// ImGui
/// -------------------------------------------------------------
void GuardianBoss::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Begin("GuardianBoss");

	ImGui::Text("State: %d", static_cast<int>(GetState()));
	ImGui::Text("HP: %.1f / %.1f", GetHP(), GetMaxHP());
	ImGui::Text("DistanceToTargetXZ: %.2f", GetDistanceToTargetXZ());
	ImGui::Text("AttackCooldown: %.2f", attackCooldownTimer_);

	if (GetAnimationComponent())
	{
		ImGui::Separator();
		ImGui::Text("WalkAnimTime: %.2f", GetAnimationComponent()->GetWalkTime());
		ImGui::Text("AttackAnimTime: %.2f", GetAnimationComponent()->GetAttackTime());
	}

	ImGui::End();
#endif
}