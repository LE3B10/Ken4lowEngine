#define NOMINMAX
#include "BossAnimationComponent.h"
#include "Core/BossBase.h"

#include <algorithm>
#include <cmath>

using namespace Ken4lowEngine;

namespace
{
	/// <summary>
	/// 値を min ～ max に収める
	/// </summary>
	float Clamp(float v, float minValue, float maxValue)
	{
		return (v < minValue) ? minValue : (v > maxValue ? maxValue : v);
	}

	/// <summary>
	/// 線形補間
	/// </summary>
	float Lerp(float a, float b, float t)
	{
		return a + (b - a) * t;
	}
}

/// -------------------------------------------------------------
/// 初期化
/// -------------------------------------------------------------
void BossAnimationComponent::Initialize(BossBase* owner)
{
	owner_ = owner;

	walkAnimTime_ = 0.0f;
	attackAnimTime_ = 0.0f;
}

/// -------------------------------------------------------------
/// 終了処理
/// -------------------------------------------------------------
void BossAnimationComponent::Finalize()
{
	owner_ = nullptr;
}

/// -------------------------------------------------------------
/// 毎フレーム更新
/// 現在の BossState を見て見た目アニメを切り替える
/// -------------------------------------------------------------
void BossAnimationComponent::Update(BossBase& boss, float deltaTime)
{
	switch (boss.GetState())
	{
	case BossState::Idle:
		UpdateIdle(boss, deltaTime);
		break;

	case BossState::Move:
		UpdateMove(boss, deltaTime);
		break;

	case BossState::Attack:
		UpdateAttack(boss, deltaTime);
		break;

	case BossState::Stagger:
		UpdateStagger(boss, deltaTime);
		break;

	case BossState::Dead:
		UpdateDead(boss, deltaTime);
		break;

	case BossState::Intro:
	case BossState::Down:
	case BossState::PhaseTransition:
	default:
		// 特殊状態はとりあえず自然姿勢へ戻す
		ResetAllPose(Clamp(deltaTime * 6.0f, 0.0f, 1.0f));
		break;
	}
}

/// -------------------------------------------------------------
/// Idle
/// 待機中はニュートラル姿勢へ戻す
/// -------------------------------------------------------------
void BossAnimationComponent::UpdateIdle(BossBase& boss, float deltaTime)
{
	(void)boss;
	ResetAllPose(Clamp(deltaTime * 8.0f, 0.0f, 1.0f));
}

/// -------------------------------------------------------------
/// Move
/// 歩行アニメを進める
/// -------------------------------------------------------------
void BossAnimationComponent::UpdateMove(BossBase& boss, float deltaTime)
{
	UpdateWalkAnimation(boss, deltaTime);
}

/// -------------------------------------------------------------
/// Attack
/// 攻撃モーションを進める
/// -------------------------------------------------------------
void BossAnimationComponent::UpdateAttack(BossBase& boss, float deltaTime)
{
	attackAnimTime_ += deltaTime;
	UpdateAttackAnimation(boss, deltaTime);
}

/// -------------------------------------------------------------
/// Stagger
/// ひるみ中は一旦ポーズを戻す
/// -------------------------------------------------------------
void BossAnimationComponent::UpdateStagger(BossBase& boss, float deltaTime)
{
	(void)boss;
	ResetAllPose(Clamp(deltaTime * 12.0f, 0.0f, 1.0f));
}

/// -------------------------------------------------------------
/// Dead
/// 死亡後はゆっくり姿勢を固定方向へ戻す
/// 今は簡易的に全姿勢を戻すだけ
/// -------------------------------------------------------------
void BossAnimationComponent::UpdateDead(BossBase& boss, float deltaTime)
{
	(void)boss;
	ResetAllPose(Clamp(deltaTime * 3.0f, 0.0f, 1.0f));
}

/// -------------------------------------------------------------
/// 歩行アニメ
/// 腕と脚を逆位相で振る
/// -------------------------------------------------------------
void BossAnimationComponent::UpdateWalkAnimation(BossBase& boss, float deltaTime)
{
	auto& parts = boss.GetBodyParts();
	const auto& idx = boss.GetPartIndices();

	// 必要部位が揃っていなければ何もしない
	if (idx.leftArm >= parts.size() ||
		idx.rightArm >= parts.size() ||
		idx.leftLeg >= parts.size() ||
		idx.rightLeg >= parts.size())
	{
		return;
	}

	// アニメ時間を進める
	walkAnimTime_ += deltaTime * walkAnimSpeed_;

	// sin 波で前後に振る
	const float swing = std::sin(walkAnimTime_) * walkSwingAmplitude_;

	parts[idx.leftArm].transform.rotate_.x = swing;
	parts[idx.rightArm].transform.rotate_.x = -swing;
	parts[idx.leftLeg].transform.rotate_.x = -swing;
	parts[idx.rightLeg].transform.rotate_.x = swing;
}

/// -------------------------------------------------------------
/// 攻撃アニメ
/// 右腕を溜めてから振り下ろす
/// -------------------------------------------------------------
void BossAnimationComponent::UpdateAttackAnimation(BossBase& boss, float deltaTime)
{
	(void)deltaTime;

	auto& parts = boss.GetBodyParts();
	const auto& idx = boss.GetPartIndices();

	// 必要部位が揃っていなければ何もしない
	if (idx.leftArm >= parts.size() ||
		idx.rightArm >= parts.size() ||
		idx.leftLeg >= parts.size() ||
		idx.rightLeg >= parts.size())
	{
		return;
	}

	const float duration = std::max(0.01f, attackDuration_);
	const float t = Clamp(attackAnimTime_ / duration, 0.0f, 1.0f);

	float rightArmX = 0.0f;

	// 前半: 溜め
	if (t < 0.35f)
	{
		const float localT = t / 0.35f;
		rightArmX = Lerp(0.0f, -1.75f, localT);
	}
	// 後半: 振り下ろし
	else
	{
		const float localT = (t - 0.35f) / 0.65f;
		rightArmX = Lerp(-1.75f, 1.20f, localT);
	}

	parts[idx.rightArm].transform.rotate_.x = rightArmX;
	parts[idx.leftArm].transform.rotate_.x = -0.20f;
	parts[idx.leftLeg].transform.rotate_.x = 0.08f;
	parts[idx.rightLeg].transform.rotate_.x = -0.08f;
}

/// -------------------------------------------------------------
/// 全部位を自然姿勢へ戻す
/// blendRate が大きいほど早くゼロへ寄る
/// -------------------------------------------------------------
void BossAnimationComponent::ResetAllPose(float blendRate)
{
	if (!owner_)
	{
		return;
	}

	auto& parts = owner_->GetBodyParts();
	const auto& idx = owner_->GetPartIndices();

	if (idx.leftArm >= parts.size() ||
		idx.rightArm >= parts.size() ||
		idx.leftLeg >= parts.size() ||
		idx.rightLeg >= parts.size())
	{
		return;
	}

	blendRate = Clamp(blendRate, 0.0f, 1.0f);

	auto BlendToZero = [blendRate](float& v)
		{
			v = Lerp(v, 0.0f, blendRate);
		};

	BlendToZero(parts[idx.leftArm].transform.rotate_.x);
	BlendToZero(parts[idx.rightArm].transform.rotate_.x);
	BlendToZero(parts[idx.leftLeg].transform.rotate_.x);
	BlendToZero(parts[idx.rightLeg].transform.rotate_.x);

	BlendToZero(parts[idx.leftArm].transform.rotate_.y);
	BlendToZero(parts[idx.rightArm].transform.rotate_.y);
	BlendToZero(parts[idx.leftLeg].transform.rotate_.y);
	BlendToZero(parts[idx.rightLeg].transform.rotate_.y);

	BlendToZero(parts[idx.leftArm].transform.rotate_.z);
	BlendToZero(parts[idx.rightArm].transform.rotate_.z);
	BlendToZero(parts[idx.leftLeg].transform.rotate_.z);
	BlendToZero(parts[idx.rightLeg].transform.rotate_.z);
}

/// -------------------------------------------------------------
/// 攻撃アニメ時間リセット
/// -------------------------------------------------------------
void BossAnimationComponent::ResetAttackTimer()
{
	attackAnimTime_ = 0.0f;
}

/// -------------------------------------------------------------
/// 歩行アニメ時間リセット
/// -------------------------------------------------------------
void BossAnimationComponent::ResetWalkTimer()
{
	walkAnimTime_ = 0.0f;
}