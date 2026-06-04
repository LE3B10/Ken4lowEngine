#define NOMINMAX
#include "BossAnimationComponent.h"
#include "BossBase.h"
#include "BossAttackComponent.h"
#include "BossPunchAttackAnimation.h"
#include "BossHeavyPunchAttackAnimation.h"

#include "BossPunchAttack.h"
#include "BossHeavyPunchAttack.h"
#include "GuardianShockwaveAttack.h"
#include "BossChargeAttack.h"

#include <LinearInterpolation.h>

#include <algorithm>
#include <cmath>

using namespace Ken4lowEngine;

/// -------------------------------------------------------------
///						初期化処理
/// -------------------------------------------------------------
void BossAnimationComponent::Initialize(BossBase* owner)
{
	// アニメーション対象のボスを受け取って初期化
	owner_ = owner;

	// アニメーション状態を初期化
	walkAnimTime_ = 0.0f;
	attackAnimTime_ = 0.0f;
	breathTime_ = 0.0f;

	// 以前の登録を消しておく
	attackAnimations_.clear();

	// 攻撃アニメーションを登録
	RegisterAttackAnimation(std::make_unique<BossPunchAttackAnimation>());
	RegisterAttackAnimation(std::make_unique<BossHeavyPunchAttackAnimation>());
}

/// -------------------------------------------------------------
///							終了処理
/// -------------------------------------------------------------
void BossAnimationComponent::Finalize()
{
	owner_ = nullptr;
}

/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void BossAnimationComponent::Update(BossBase& boss, float deltaTime)
{
	if (!HasRequiredParts(boss))
	{
		return;
	}

	breathTime_ += deltaTime; // Idle/Walkの周期時間を毎フレーム進め、停止していた呼吸・歩行揺れを復旧する。

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
		ResetAllPose(std::clamp(deltaTime * 6.0f, 0.0f, 1.0f));
		break;
	}
}

/// -------------------------------------------------------------
///			　Idle 待機中はニュートラル姿勢へ戻す
/// -------------------------------------------------------------
void BossAnimationComponent::UpdateIdle(BossBase& boss, float deltaTime)
{
	UpdateIdleAnimation(boss, deltaTime); // Idle状態は専用関数へ集約し、重複実装で呼吸時間が止まる不具合を防ぐ。
}

/// -------------------------------------------------------------
///						Move 歩行アニメを進める
/// -------------------------------------------------------------
void BossAnimationComponent::UpdateMove(BossBase& boss, float deltaTime)
{
	UpdateWalkAnimation(boss, deltaTime);
}

/// -------------------------------------------------------------
///					Attack 攻撃モーションを進める
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
	ResetAllPose(std::clamp(deltaTime * 12.0f, 0.0f, 1.0f));
}

/// -------------------------------------------------------------
///			Dead 死亡後はゆっくり姿勢を固定方向へ戻す
/// -------------------------------------------------------------
void BossAnimationComponent::UpdateDead(BossBase& boss, float deltaTime)
{
	auto& body = boss.GetBody();
	auto& parts = boss.GetBodyParts();
	const auto& idx = boss.GetPartIndices();

	// 体幹は少し前傾させて、頭は少し上を向かせる
	Damp(body.transform.rotate_.z, 0.55f, bodyDampSpeed_ * 0.35f, deltaTime);
	Damp(body.transform.rotate_.x, 0.20f, bodyDampSpeed_ * 0.35f, deltaTime);
	Damp(parts[idx.leftArm].transform.rotate_.x, 0.75f, limbDampSpeed_ * 0.35f, deltaTime);
	Damp(parts[idx.rightArm].transform.rotate_.x, 0.85f, limbDampSpeed_ * 0.35f, deltaTime);
	Damp(parts[idx.leftLeg].transform.rotate_.x, -0.15f, limbDampSpeed_ * 0.35f, deltaTime);
	Damp(parts[idx.rightLeg].transform.rotate_.x, -0.05f, limbDampSpeed_ * 0.35f, deltaTime);
}

/// -------------------------------------------------------------
///						Idleアニメーション
/// -------------------------------------------------------------
void BossAnimationComponent::UpdateIdleAnimation(BossBase& boss, float deltaTime)
{
	auto& body = boss.GetBody();
	auto& parts = boss.GetBodyParts();
	const auto& idx = boss.GetPartIndices();

	const float breath = std::sin(breathTime_ * 2.2f);
	const float breath01 = (breath * 0.5f) + 0.5f;
	const float headSway = std::sin(breathTime_ * 1.4f + 0.8f);

	// 目標値
	const float targetBodyPitch = idleBreathPitch_ * breath;
	const float targetHeadPitch = -idleBreathPitch_ * 0.6f * breath01;
	const float targetHeadYaw = idleHeadSway_ * headSway;

	Damp(body.transform.rotate_.x, targetBodyPitch, bodyDampSpeed_, deltaTime); // Idle呼吸は回転だけに留め、ワールドY座標をアニメ側で潰さない。

	// ---------------------------------------------------------
	// Yaw は触らない
	// body.transform.rotate_.y は FaceTarget / 移動 / 攻撃前方計算で使う
	// ---------------------------------------------------------

	Damp(body.transform.rotate_.z, 0.0f, bodyDampSpeed_, deltaTime);

	Damp(parts[idx.head].transform.rotate_.x, targetHeadPitch, headDampSpeed_, deltaTime);
	Damp(parts[idx.head].transform.rotate_.y, targetHeadYaw, headDampSpeed_, deltaTime);
	Damp(parts[idx.head].transform.rotate_.z, 0.0f, headDampSpeed_, deltaTime);

	Damp(parts[idx.leftArm].transform.rotate_.x, -0.05f, limbDampSpeed_, deltaTime);
	Damp(parts[idx.rightArm].transform.rotate_.x, -0.05f, limbDampSpeed_, deltaTime);
	Damp(parts[idx.leftLeg].transform.rotate_.x, 0.0f, limbDampSpeed_, deltaTime);
	Damp(parts[idx.rightLeg].transform.rotate_.x, 0.0f, limbDampSpeed_, deltaTime);
}

/// -------------------------------------------------------------
///				歩行アニメ 腕と脚を逆位相で振る
/// -------------------------------------------------------------
void BossAnimationComponent::UpdateWalkAnimation(BossBase& boss, float deltaTime)
{
	auto& body = boss.GetBody();
	auto& parts = boss.GetBodyParts();
	const auto& idx = boss.GetPartIndices();

	walkAnimTime_ += deltaTime * walkAnimSpeed_;

	const float swing = std::sin(walkAnimTime_) * walkSwingAmplitude_;
	const float twist = std::sin(walkAnimTime_) * moveBodyTwistYaw_;
	const float shoulderLean = std::sin(walkAnimTime_) * moveShoulderLean_;

	const float targetLeftArmX = swing;
	const float targetRightArmX = -swing;
	const float targetLeftLegX = -swing;
	const float targetRightLegX = swing;

	const float targetBodyPitch = 0.03f + std::abs(std::sin(walkAnimTime_)) * 0.03f;
	const float targetBodyRoll = shoulderLean * 0.35f;

	const float targetHeadYaw = -twist * 0.35f;
	const float targetHeadPitch = 0.03f;

	// 実座標Yは触らない
	Damp(body.transform.rotate_.x, targetBodyPitch, bodyDampSpeed_, deltaTime);
	Damp(body.transform.rotate_.z, targetBodyRoll, bodyDampSpeed_, deltaTime);

	Damp(parts[idx.head].transform.rotate_.x, targetHeadPitch, headDampSpeed_, deltaTime);
	Damp(parts[idx.head].transform.rotate_.y, targetHeadYaw, headDampSpeed_, deltaTime);

	Damp(parts[idx.leftArm].transform.rotate_.x, targetLeftArmX, limbDampSpeed_, deltaTime);
	Damp(parts[idx.rightArm].transform.rotate_.x, targetRightArmX, limbDampSpeed_, deltaTime);
	Damp(parts[idx.leftLeg].transform.rotate_.x, targetLeftLegX, limbDampSpeed_, deltaTime);
	Damp(parts[idx.rightLeg].transform.rotate_.x, targetRightLegX, limbDampSpeed_, deltaTime);

	Damp(parts[idx.leftArm].transform.rotate_.z, -shoulderLean, limbDampSpeed_, deltaTime);
	Damp(parts[idx.rightArm].transform.rotate_.z, shoulderLean, limbDampSpeed_, deltaTime);
	Damp(parts[idx.leftLeg].transform.rotate_.z, 0.0f, limbDampSpeed_, deltaTime);
	Damp(parts[idx.rightLeg].transform.rotate_.z, 0.0f, limbDampSpeed_, deltaTime);
}

/// -------------------------------------------------------------
///			 攻撃アニメ 右腕を溜めてから振り下ろす
/// -------------------------------------------------------------
void BossAnimationComponent::UpdateAttackAnimation(BossBase& boss, float deltaTime)
{
	IBossAttack* currentAttack = GetCurrentAttack(boss);

	// 攻撃が無ければ基本姿勢へと戻る
	if (!currentAttack)
	{
		ApplyPose(boss, BuildDefaultAttackPose(), deltaTime);
		return;
	}

	// 登録済みアニメクラスの中から今の攻撃を扱えるものを探して更新する
	for (const auto& attackAnimation : attackAnimations_)
	{
		if (!attackAnimation)
		{
			continue;
		}

		if (attackAnimation->CanHandle(currentAttack))
		{
			attackAnimation->UpdatePose(*this, boss, currentAttack, deltaTime);
			return;
		}
	}

	// 専用クラス未登録のGuardian攻撃はここで既存ポーズを流用して分かりやすい予備動作を出す。
	if (dynamic_cast<GuardianShockwaveAttack*>(currentAttack))
	{
		ApplyPose(boss, BuildShockwavePose(), deltaTime);
		return;
	}
	if (dynamic_cast<BossChargeAttack*>(currentAttack))
	{
		ApplyPose(boss, BuildChargePose(), deltaTime);
		return;
	}

	// 該当アニメがない場合は最低限の姿勢だけを適用
	ApplyPose(boss, BuildDefaultAttackPose(), deltaTime);
}

IBossAttack* BossAnimationComponent::GetCurrentAttack(const BossBase& boss) const
{
	if (!boss.GetAttackComponent())
	{
		return nullptr;
	}

	return boss.GetAttackComponent()->GetCurrentAttack();
}

/// -------------------------------------------------------------
///					  攻撃アニメの基本姿勢
/// -------------------------------------------------------------
BossAnimationComponent::BossPose BossAnimationComponent::BuildDefaultAttackPose() const
{
	BossPose pose;

	pose.bodyPitch = 0.03f;
	pose.bodyRoll = 0.0f;

	pose.headYaw = 0.0f;
	pose.headPitch = 0.0f;

	pose.leftArmX = -0.05f;
	pose.rightArmX = -0.05f;
	pose.leftArmZ = 0.0f;
	pose.rightArmZ = 0.0f;

	pose.leftLegX = 0.0f;
	pose.rightLegX = 0.0f;

	return pose;
}

/// -------------------------------------------------------------
///					攻撃アニメの個別姿勢構築
/// -------------------------------------------------------------
BossAnimationComponent::BossPose BossAnimationComponent::BuildPunchPose() const
{
	BossPose pose = BuildDefaultAttackPose();

	BossPunchAttack::Phase punchPhase = BossPunchAttack::Phase::None;
	float phaseTime = 0.0f;
	bool hasPunchPhase = false;

	if (owner_)
	{
		if (IBossAttack* current = GetCurrentAttack(*owner_))
		{
			if (auto* punch = dynamic_cast<BossPunchAttack*>(current))
			{
				punchPhase = punch->GetPhase();
				phaseTime = punch->GetPhaseTimer();
				hasPunchPhase = true;
			}
		}
	}

	// PunchAttackでなければ基本姿勢のまま返す
	if (!hasPunchPhase) return pose;

	// フェーズごとの経過時間で補間し、前フェーズのattackAnimTime_残りでポーズが固まる問題を防ぐ。
	switch (punchPhase)
	{
	case BossPunchAttack::Phase::Windup:
		{
			// 両手を上に振りかざす
			const float t = Smoothstep01(phaseTime / 0.30f);

			pose.bodyPitch = Lerp(0.03f, -0.18f, t);
			pose.bodyRoll = Lerp(0.0f, 0.0f, t);

			pose.headYaw = Lerp(0.0f, 0.0f, t);
			pose.headPitch = Lerp(0.0f, -0.08f, t);

			pose.leftArmX = Lerp(-0.05f, -2.00f, t);
			pose.rightArmX = Lerp(-0.05f, -2.00f, t);

			pose.leftArmZ = Lerp(0.0f, 0.35f, t);
			pose.rightArmZ = Lerp(0.0f, -0.35f, t);

			pose.leftLegX = Lerp(0.0f, 0.08f, t);
			pose.rightLegX = Lerp(0.0f, 0.08f, t);
			break;
		}

	case BossPunchAttack::Phase::Active:
		{
			// 両腕を一気に振り下ろす
			const float t = Smoothstep01(phaseTime / 0.12f);

			pose.bodyPitch = Lerp(-0.18f, 0.28f, t);
			pose.bodyRoll = 0.0f;

			pose.headYaw = 0.0f;
			pose.headPitch = Lerp(-0.08f, 0.06f, t);

			pose.leftArmX = Lerp(-2.00f, 0.85f, t);
			pose.rightArmX = Lerp(-2.00f, 0.85f, t);

			pose.leftArmZ = Lerp(0.35f, 0.10f, t);
			pose.rightArmZ = Lerp(-0.35f, -0.10f, t);

			pose.leftLegX = Lerp(0.08f, -0.04f, t);
			pose.rightLegX = Lerp(0.08f, -0.04f, t);
			break;
		}

	case BossPunchAttack::Phase::Recovery:
	case BossPunchAttack::Phase::None:
	default:
		{
			const float t = Smoothstep01(phaseTime / 0.40f);

			pose.bodyPitch = Lerp(0.28f, 0.03f, t);
			pose.bodyRoll = 0.0f;

			pose.headYaw = 0.0f;
			pose.headPitch = Lerp(0.06f, 0.0f, t);

			pose.leftArmX = Lerp(0.85f, -0.05f, t);
			pose.rightArmX = Lerp(0.85f, -0.05f, t);

			pose.leftArmZ = Lerp(0.10f, 0.0f, t);
			pose.rightArmZ = Lerp(-0.10f, 0.0f, t);

			pose.leftLegX = Lerp(-0.04f, 0.0f, t);
			pose.rightLegX = Lerp(-0.04f, 0.0f, t);
			break;
		}
	}

	return pose;
}

/// -------------------------------------------------------------
///			重い攻撃は大きく振りかぶってから叩きつける
/// -------------------------------------------------------------
BossAnimationComponent::BossPose BossAnimationComponent::BuildHeavyPunchPose() const
{
	BossPose pose = BuildDefaultAttackPose();

	BossHeavyPunchAttack::Phase heavyPhase = BossHeavyPunchAttack::Phase::None;
	float phaseTime = 0.0f;
	bool hasHeavyPhase = false;

	if (owner_)
	{
		if (IBossAttack* current = GetCurrentAttack(*owner_))
		{
			if (auto* heavy = dynamic_cast<BossHeavyPunchAttack*>(current))
			{
				heavyPhase = heavy->GetPhase();
				phaseTime = heavy->GetPhaseTimer();
				hasHeavyPhase = true;
			}
		}
	}

	// HeavyPunchでなければ基本姿勢のまま返す
	if (!hasHeavyPhase)	return pose;

	switch (heavyPhase)
	{
	case BossHeavyPunchAttack::Phase::Windup:
		{
			// 大きく両腕をあげる溜め
			const float t = Smoothstep01(phaseTime / 0.55f);

			pose.bodyPitch = Lerp(0.03f, -0.28f, t);
			pose.bodyRoll = 0.0f;

			pose.headYaw = 0.0f;
			pose.headPitch = Lerp(0.0f, -0.10f, t);

			pose.leftArmX = Lerp(-0.05f, -2.35f, t);
			pose.rightArmX = Lerp(-0.05f, -2.35f, t);

			pose.leftArmZ = Lerp(0.0f, 0.55f, t);
			pose.rightArmZ = Lerp(0.0f, -0.55f, t);

			pose.leftLegX = Lerp(0.0f, 0.12f, t);
			pose.rightLegX = Lerp(0.0f, 0.12f, t);

			break;
		}
	case BossHeavyPunchAttack::Phase::Hold:
		{
			// 上げ切った姿勢を一瞬キープして「重さ」を出す
			pose.bodyPitch = -0.28f;
			pose.bodyRoll = 0.0f;

			pose.headYaw = 0.0f;
			pose.headPitch = -0.10f;

			pose.leftArmX = -2.35f;
			pose.rightArmX = -2.35f;

			pose.leftArmZ = 0.55f;
			pose.rightArmZ = -0.55f;

			pose.leftLegX = 0.12f;
			pose.rightLegX = 0.12f;
			break;
		}

	case BossHeavyPunchAttack::Phase::Active:
		{
			// 一気に叩き下ろす
			const float t = Smoothstep01(phaseTime / 0.12f);

			pose.bodyPitch = Lerp(-0.28f, 0.42f, t);
			pose.bodyRoll = 0.0f;

			pose.headYaw = 0.0f;
			pose.headPitch = Lerp(-0.10f, 0.12f, t);

			pose.leftArmX = Lerp(-2.35f, 1.05f, t);
			pose.rightArmX = Lerp(-2.35f, 1.05f, t);

			pose.leftArmZ = Lerp(0.55f, 0.08f, t);
			pose.rightArmZ = Lerp(-0.55f, -0.08f, t);

			pose.leftLegX = Lerp(0.12f, -0.08f, t);
			pose.rightLegX = Lerp(0.12f, -0.08f, t);
			break;
		}

	case BossHeavyPunchAttack::Phase::Recovery:
		{
			// 重攻撃なので戻りは遅め
			const float t = Smoothstep01(phaseTime / 0.80f);

			pose.bodyPitch = Lerp(0.42f, 0.03f, t);
			pose.bodyRoll = 0.0f;

			pose.headYaw = 0.0f;
			pose.headPitch = Lerp(0.12f, 0.0f, t);

			pose.leftArmX = Lerp(1.05f, -0.05f, t);
			pose.rightArmX = Lerp(1.05f, -0.05f, t);

			pose.leftArmZ = Lerp(0.08f, 0.0f, t);
			pose.rightArmZ = Lerp(-0.08f, 0.0f, t);

			pose.leftLegX = Lerp(-0.08f, 0.0f, t);
			pose.rightLegX = Lerp(-0.08f, 0.0f, t);
			break;
		}

	case BossHeavyPunchAttack::Phase::None:
	default:
		break;
	}

	return pose;
}


/// -------------------------------------------------------------
///			Shockwave は重攻撃の溜めを流用し、叩きつけを強調する
/// -------------------------------------------------------------
BossAnimationComponent::BossPose BossAnimationComponent::BuildShockwavePose() const
{
	BossPose pose = BuildDefaultAttackPose();
	GuardianShockwaveAttack::Phase phase = GuardianShockwaveAttack::Phase::None;
	float phaseTime = 0.0f;
	if (owner_)
	{
		if (IBossAttack* current = GetCurrentAttack(*owner_))
		{
			if (auto* shockwave = dynamic_cast<GuardianShockwaveAttack*>(current))
			{
				phase = shockwave->GetPhase();
				phaseTime = shockwave->GetPhaseTimer();
			}
		}
	}

	switch (phase)
	{
	case GuardianShockwaveAttack::Phase::Windup:
		{
			const float t = Smoothstep01(phaseTime / 0.80f);
			pose.bodyPitch = Lerp(0.03f, -0.24f, t);
			pose.headPitch = Lerp(0.0f, -0.12f, t);
			pose.leftArmX = Lerp(-0.05f, -2.25f, t);
			pose.rightArmX = Lerp(-0.05f, -2.25f, t);
			pose.leftArmZ = Lerp(0.0f, 0.55f, t);
			pose.rightArmZ = Lerp(0.0f, -0.55f, t);
			pose.leftLegX = Lerp(0.0f, 0.12f, t);
			pose.rightLegX = Lerp(0.0f, 0.12f, t);
			break;
		}
	case GuardianShockwaveAttack::Phase::Charge:
		{
			const float t = Smoothstep01(phaseTime / 0.25f);
			pose.bodyPitch = Lerp(-0.24f, -0.30f, t);
			pose.headPitch = -0.12f;
			pose.leftArmX = Lerp(-2.25f, -2.45f, t);
			pose.rightArmX = Lerp(-2.25f, -2.45f, t);
			pose.leftArmZ = 0.55f;
			pose.rightArmZ = -0.55f;
			pose.leftLegX = 0.14f;
			pose.rightLegX = 0.14f;
			break;
		}
	case GuardianShockwaveAttack::Phase::Active:
		{
			const float t = Smoothstep01(phaseTime / 0.25f);
			pose.bodyPitch = Lerp(-0.24f, 0.48f, t);
			pose.headPitch = Lerp(-0.12f, 0.14f, t);
			pose.leftArmX = Lerp(-2.25f, 1.15f, t);
			pose.rightArmX = Lerp(-2.25f, 1.15f, t);
			pose.leftArmZ = Lerp(0.55f, 0.05f, t);
			pose.rightArmZ = Lerp(-0.55f, -0.05f, t);
			pose.leftLegX = -0.10f;
			pose.rightLegX = -0.10f;
			break;
		}
	case GuardianShockwaveAttack::Phase::Recovery:
		{
			const float t = Smoothstep01(phaseTime / 1.00f);
			pose.bodyPitch = Lerp(0.48f, 0.03f, t);
			pose.headPitch = Lerp(0.14f, 0.0f, t);
			pose.leftArmX = Lerp(1.15f, -0.05f, t);
			pose.rightArmX = Lerp(1.15f, -0.05f, t);
			pose.leftLegX = Lerp(-0.10f, 0.0f, t);
			pose.rightLegX = Lerp(-0.10f, 0.0f, t);
			break;
		}
	case GuardianShockwaveAttack::Phase::None:
	default:
		break;
	}
	return pose;
}

/// -------------------------------------------------------------
///			ChargeAttack は低く構えてから突進する姿勢を作る
/// -------------------------------------------------------------
BossAnimationComponent::BossPose BossAnimationComponent::BuildChargePose() const
{
	BossPose pose = BuildDefaultAttackPose();
	BossChargeAttack::Phase phase = BossChargeAttack::Phase::None;
	float phaseTime = 0.0f;
	if (owner_)
	{
		if (IBossAttack* current = GetCurrentAttack(*owner_))
		{
			if (auto* charge = dynamic_cast<BossChargeAttack*>(current))
			{
				phase = charge->GetPhase();
				phaseTime = charge->GetPhaseTimer();
			}
		}
	}

	switch (phase)
	{
	case BossChargeAttack::Phase::Windup:
		{
			const float t = Smoothstep01(phaseTime / 0.60f);
			pose.bodyPitch = Lerp(0.03f, 0.32f, t);
			pose.headPitch = Lerp(0.0f, -0.10f, t);
			pose.leftArmX = Lerp(-0.05f, -0.90f, t);
			pose.rightArmX = Lerp(-0.05f, -0.90f, t);
			pose.leftArmZ = Lerp(0.0f, 0.35f, t);
			pose.rightArmZ = Lerp(0.0f, -0.35f, t);
			pose.leftLegX = Lerp(0.0f, 0.22f, t);
			pose.rightLegX = Lerp(0.0f, -0.18f, t);
			break;
		}
	case BossChargeAttack::Phase::Charging:
		pose.bodyPitch = 0.42f;
		pose.headPitch = -0.06f;
		pose.leftArmX = -0.65f;
		pose.rightArmX = -0.65f;
		pose.leftArmZ = 0.20f;
		pose.rightArmZ = -0.20f;
		pose.leftLegX = 0.30f;
		pose.rightLegX = -0.25f;
		break;
	case BossChargeAttack::Phase::Recovery:
		{
			const float t = Smoothstep01(phaseTime / 1.00f);
			pose.bodyPitch = Lerp(0.42f, 0.03f, t);
			pose.headPitch = Lerp(-0.06f, 0.0f, t);
			pose.leftArmX = Lerp(-0.65f, -0.05f, t);
			pose.rightArmX = Lerp(-0.65f, -0.05f, t);
			pose.leftLegX = Lerp(0.30f, 0.0f, t);
			pose.rightLegX = Lerp(-0.25f, 0.0f, t);
			break;
		}
	case BossChargeAttack::Phase::None:
	default:
		break;
	}
	return pose;
}

/// -------------------------------------------------------------
///				構築したポーズを実際のボス姿勢へ適用する
/// -------------------------------------------------------------
void BossAnimationComponent::ApplyPose(BossBase& boss, const BossPose& pose, float deltaTime)
{
	auto& body = boss.GetBody();
	auto& parts = boss.GetBodyParts();
	const auto& idx = boss.GetPartIndices();

	Damp(body.transform.rotate_.x, pose.bodyPitch, bodyDampSpeed_, deltaTime);
	Damp(body.transform.rotate_.z, pose.bodyRoll, bodyDampSpeed_, deltaTime);

	Damp(parts[idx.head].transform.rotate_.x, pose.headPitch, headDampSpeed_, deltaTime);
	Damp(parts[idx.head].transform.rotate_.y, pose.headYaw, headDampSpeed_, deltaTime);

	Damp(parts[idx.leftArm].transform.rotate_.x, pose.leftArmX, limbDampSpeed_, deltaTime);
	Damp(parts[idx.rightArm].transform.rotate_.x, pose.rightArmX, limbDampSpeed_, deltaTime);
	Damp(parts[idx.leftArm].transform.rotate_.z, pose.leftArmZ, limbDampSpeed_, deltaTime);
	Damp(parts[idx.rightArm].transform.rotate_.z, pose.rightArmZ, limbDampSpeed_, deltaTime);

	Damp(parts[idx.leftLeg].transform.rotate_.x, pose.leftLegX, limbDampSpeed_, deltaTime);
	Damp(parts[idx.rightLeg].transform.rotate_.x, pose.rightLegX, limbDampSpeed_, deltaTime);
}

/// -------------------------------------------------------------
///				攻撃アニメクラスを登録
/// -------------------------------------------------------------
void BossAnimationComponent::RegisterAttackAnimation(std::unique_ptr<IBossAttackAnimation> attackAnimation)
{
	// 攻撃アニメーションは複数登録できるようにする予定
	if (!attackAnimation) return;

	attackAnimations_.push_back(std::move(attackAnimation));
}

/// -------------------------------------------------------------
///					値を滑らかに目標へ近づける
/// -------------------------------------------------------------
void BossAnimationComponent::Damp(float& value, float target, float speed, float deltaTime)
{
	const float t = std::clamp(deltaTime * speed, 0.0f, 1.0f);
	value = Lerp(value, target, t);
}

/// -------------------------------------------------------------
///					角度を滑らかに目標へ近づける
/// -------------------------------------------------------------
void BossAnimationComponent::DampAngle(float& value, float target, float speed, float deltaTime)
{
	Damp(value, target, speed, deltaTime);
}

/// -------------------------------------------------------------
///					必要な部位が揃っているか
/// -------------------------------------------------------------
bool BossAnimationComponent::HasRequiredParts(const BossBase& boss) const
{
	const auto& parts = boss.GetBodyParts();
	const auto& idx = boss.GetPartIndices();

	return idx.head < parts.size() &&
		idx.leftArm < parts.size() &&
		idx.rightArm < parts.size() &&
		idx.leftLeg < parts.size() &&
		idx.rightLeg < parts.size();
}

/// -------------------------------------------------------------
/// 全部位を自然姿勢へ戻す
/// blendRate が大きいほど早くゼロへ寄る
/// -------------------------------------------------------------
void BossAnimationComponent::ResetAllPose(float blendRate)
{
	if (!owner_ || !HasRequiredParts(*owner_))
	{
		return;
	}

	auto& body = owner_->GetBody();
	auto& parts = owner_->GetBodyParts();
	const auto& idx = owner_->GetPartIndices();

	blendRate = std::clamp(blendRate, 0.0f, 1.0f);

	auto BlendTo = [blendRate](float& v, float target)
		{
			v = Lerp(v, target, blendRate);
		};

	// 実座標Yは触らない
	BlendTo(body.transform.rotate_.x, 0.0f);
	BlendTo(body.transform.rotate_.z, 0.0f);

	BlendTo(parts[idx.head].transform.rotate_.x, 0.0f);
	BlendTo(parts[idx.head].transform.rotate_.y, 0.0f);
	BlendTo(parts[idx.head].transform.rotate_.z, 0.0f);

	BlendTo(parts[idx.leftArm].transform.rotate_.x, 0.0f);
	BlendTo(parts[idx.leftArm].transform.rotate_.y, 0.0f);
	BlendTo(parts[idx.leftArm].transform.rotate_.z, 0.0f);

	BlendTo(parts[idx.rightArm].transform.rotate_.x, 0.0f);
	BlendTo(parts[idx.rightArm].transform.rotate_.y, 0.0f);
	BlendTo(parts[idx.rightArm].transform.rotate_.z, 0.0f);

	BlendTo(parts[idx.leftLeg].transform.rotate_.x, 0.0f);
	BlendTo(parts[idx.leftLeg].transform.rotate_.y, 0.0f);
	BlendTo(parts[idx.leftLeg].transform.rotate_.z, 0.0f);

	BlendTo(parts[idx.rightLeg].transform.rotate_.x, 0.0f);
	BlendTo(parts[idx.rightLeg].transform.rotate_.y, 0.0f);
	BlendTo(parts[idx.rightLeg].transform.rotate_.z, 0.0f);
}

/// -------------------------------------------------------------
///						攻撃アニメ時間リセット
/// -------------------------------------------------------------
void BossAnimationComponent::ResetAttackTimer()
{
	attackAnimTime_ = 0.0f;
}

/// -------------------------------------------------------------
///						歩行アニメ時間リセット
/// -------------------------------------------------------------
void BossAnimationComponent::ResetWalkTimer()
{
	walkAnimTime_ = 0.0f;
}