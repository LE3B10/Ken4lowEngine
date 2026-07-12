#define NOMINMAX
#include "BossHeavyPunchAttack.h"
#include "BossBase.h"
#include "BossAttackEffects.h"
#include "BossMeleeAttackUtility.h"
#include "GpuParticleType.h"

#include <algorithm>
#include <cmath>

#ifdef _DEBUG
#include <LogString.h>
#endif // _DEBUG

#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace Ken4lowEngine;

/// ---------------------------------------------------------------
///							初期化処理
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::Initialize(BossBase* owner)
{
	// 攻撃所有者を受け取って初期化
	owner_ = owner;

	// 攻撃状態を初期化
	isActive_ = false;
	isFinished_ = false;
	hasHit_ = false;

	// フェーズを初期化
	phase_ = Phase::None;
	phaseTimer_ = 0.0f;
	totalTimer_ = 0.0f;

	// クールダウンを初期化
	cooldownRemaining_ = 0.0f;
}

/// ---------------------------------------------------------------
///							攻撃開始
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::Start()
{
	// 条件を満たさないときは開始しない
	if (!CanStart()) return;

	// 状態リセット
	isActive_ = true;
	isFinished_ = false;
	hasHit_ = false;

	// フェーズリセット
	phase_ = Phase::None;
	phaseTimer_ = 0.0f;
	totalTimer_ = 0.0f;

	LockAttackDirection(); // 攻撃中に毎フレーム向き直らないよう、開始時の前方を固定する。

	// HeavyPunch は最初に Windup から始まる
	ChangePhase(Phase::Windup);

	Log("[BossHeavyPunchAttack] Start\n");
}

/// ---------------------------------------------------------------
///						  攻撃更新
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::Update(float deltaTime)
{
	// 実行中でないときは更新しない
	if (!isActive_)	return;

	totalTimer_ += deltaTime;
	phaseTimer_ += deltaTime;

	switch (phase_)
	{
	case Phase::Windup:
		UpdateWindup(deltaTime);
		break;

	case Phase::Hold:
		UpdateHold(deltaTime);
		break;

	case Phase::Active:
		UpdateActive(deltaTime);
		break;

	case Phase::Recovery:
		UpdateRecovery(deltaTime);
		break;

	case Phase::None:
	default:
		break;
	}
}

/// ---------------------------------------------------------------
///							攻撃終了
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::End()
{
	// 実行中でないときは何もしない
	if (!isActive_)	return;

	isActive_ = false;
	isFinished_ = true;
	cooldownRemaining_ = cooldownSec_;

	phase_ = Phase::None;
	phaseTimer_ = 0.0f;

	Log("[BossHeavyPunchAttack] End\n");
}

/// ---------------------------------------------------------------
///					今この攻撃を開始できるか
/// ---------------------------------------------------------------
bool BossHeavyPunchAttack::CanStart() const
{
	// 所有者がいないときは不可
	if (owner_ == nullptr) return false;

	// 実行中は不可
	if (isActive_) return false;

	// クールダウン中は不可
	if (cooldownRemaining_ > 0.0f) return false;

	// 死亡中は不可
	if (owner_->IsDead()) return false;

	// 射程内にターゲットがいないと不可
	if (!IsTargetInValidRange()) return false;

	// それ以外は開始可能
	return true;
}

/// ---------------------------------------------------------------
///						クールダウン更新
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::TickCooldown(float deltaTime)
{
	// 実行中はクールダウンしない
	if (cooldownRemaining_ <= 0.0f) return;

	// クールダウンを進める
	cooldownRemaining_ -= deltaTime;

	// クールダウンが0未満にならないようにする
	if (cooldownRemaining_ < 0.0f) cooldownRemaining_ = 0.0f;
}

/// ---------------------------------------------------------------
///							溜め更新
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::UpdateWindup(float deltaTime)
{
	(void)deltaTime;

	// 将来的にはここで
	// - 予兆SE
	// - 溜めエフェクト
	// - 体の震え
	// なども入れられる

	// 溜め時間が終わったら発生に移る
	if (phaseTimer_ >= windupTime_)
	{
		// すぐ殴らず、一瞬止めて予兆を見せる
		ChangePhase(Phase::Hold);
	}
}

/// ---------------------------------------------------------------
///							Hold 更新
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::UpdateHold(float deltaTime)
{
	(void)deltaTime;

	if (phaseTimer_ >= holdTime_)
	{
		// 予兆を見せる時間が終わったら攻撃発生
		ChangePhase(Phase::Active);
	}
}

/// ---------------------------------------------------------------
///							発生更新
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::UpdateActive(float deltaTime)
{
	// 今は特に攻撃中の挙動はない想定
	(void)deltaTime;

	// 発生した瞬間に1回だけヒット判定
	if (!hasHit_)
	{
		TryHitPlayer();
		hasHit_ = true;
	}

	// 発生時間が終わったら硬直に移る
	if (phaseTimer_ >= activeTime_) ChangePhase(Phase::Recovery);
}

/// ---------------------------------------------------------------
///								硬直更新
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::UpdateRecovery(float deltaTime)
{
	// 今は特に硬直中の挙動はない想定
	(void)deltaTime;

	// 硬直時間が終わったら攻撃終了
	if (phaseTimer_ >= recoveryTime_) End();
}

/// ---------------------------------------------------------------
///							フェーズ切り替え
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::ChangePhase(Phase newPhase)
{
	// フェーズを切り替える
	phase_ = newPhase;
	phaseTimer_ = 0.0f;

#ifdef _DEBUG
	switch (phase_)
	{
	case Phase::Windup:
		Log("[BossHeavyPunchAttack] Phase -> Windup\n");
		break;

	case Phase::Hold:
		Log("[BossHeavyPunchAttack] Phase -> Hold\n");
		break;

	case Phase::Active:
		Log("[BossHeavyPunchAttack] Phase -> Active\n");
		break;

	case Phase::Recovery:
		Log("[BossHeavyPunchAttack] Phase -> Recovery\n");
		break;

	case Phase::None:
	default:
		Log("[BossHeavyPunchAttack] Phase -> None\n");
		break;
	}
#endif
}

/// ---------------------------------------------------------------
///							描画処理
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::Draw()
{
	// 例:
	// - Windup/Hold 中は予兆色を出す
	// - Active 中は攻撃球を描く
}

/// ---------------------------------------------------------------
///						ImGui描画処理
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Separator();
	ImGui::Text("[BossHeavyPunchAttack]");
	ImGui::Text("Active            : %s", isActive_ ? "true" : "false");
	ImGui::Text("Finished          : %s", isFinished_ ? "true" : "false");
	ImGui::Text("HasHit            : %s", hasHit_ ? "true" : "false");
	ImGui::Text("Phase             : %s", GetPhaseName());
	ImGui::Text("PhaseTimer        : %.2f", phaseTimer_);
	ImGui::Text("TotalTimer        : %.2f", totalTimer_);
	ImGui::Text("CooldownRemaining : %.2f", cooldownRemaining_);
	ImGui::Text("Range             : %.2f - %.2f", minRange_, maxRange_);
	ImGui::Text("HitRange          : %.2f", hitRange_);
	ImGui::Text("HitRadius         : %.2f", hitRadius_);
	ImGui::Text("HitForwardOffset  : %.2f", hitForwardOffset_);
	ImGui::Text("HitAngleDeg       : %.2f", hitAngleDeg_);
	ImGui::Text("Damage            : %.2f", damage_);
#endif
}

/// ---------------------------------------------------------------
///							ヒット判定
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::TryHitPlayer()
{
	if (owner_ == nullptr) return;

	const K4E::Vector3 forward = hasLockedDirection_
		? lockedForward_
		: K4E::Vector3{ std::sin(owner_->GetYaw()), 0.0f, std::cos(owner_->GetYaw()) };
	const BossMeleeHitSettings settings{ hitRange_, hitRadius_, hitForwardOffset_, hitAngleDeg_, targetRadius_, 0.30f };
	K4E::Vector3 attackCenter{};
	if (BossMeleeAttackUtility::TryCalculateHitCenter(*owner_, forward, settings, attackCenter))
	{
#ifdef _DEBUG
		Log("[BossHeavyPunchAttack] Heavy hit success.\n");
#endif

		// ボス近接攻撃の発生フレームで1回だけPlayerへダメージを流す。
		if (owner_->ApplyDamageToTargetPlayer(damage_, &attackCenter))
		{
			BossAttackEffects::EmitGuardianHitEffect("GuardianHeavyPunchImpact", K4E::GpuParticleType::Debris, attackCenter, particleSpawnCount_, particleSpawnRadius_, particleLifetimeScale_, particleInitialSpeedScale_);
			Log("[BossHeavyPunchAttack] Player damage applied.\n");
		}
	}
	else
	{
#ifdef _DEBUG
		Log("[BossHeavyPunchAttack] Heavy hit miss.\n");
#endif
	}
}


void BossHeavyPunchAttack::LockAttackDirection()
{
	hasLockedDirection_ = BossMeleeAttackUtility::LockDirection(owner_, lockedForward_);
}

/// ---------------------------------------------------------------
///							射程内判定
/// ---------------------------------------------------------------
bool BossHeavyPunchAttack::IsTargetInValidRange() const
{
	return BossMeleeAttackUtility::IsTargetInRange(owner_, minRange_, maxRange_);
}

/// ---------------------------------------------------------------
///					デバッグ用フェーズ名
/// ---------------------------------------------------------------
const char* BossHeavyPunchAttack::GetPhaseName() const
{
	switch (phase_)
	{
	case Phase::Windup:   return "Windup";	 // 溜め
	case Phase::Hold:     return "Hold";	 // 溜め切って一瞬止める
	case Phase::Active:   return "Active";	 // 発生
	case Phase::Recovery: return "Recovery"; // 攻撃後の硬直
	case Phase::None:
	default:              return "None";	 // 未使用
	}
}
/// ---------------------------------------------------------------
///						攻撃開始距離を設定
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::SetValidRange(float minRange, float maxRange)
{
	minRange_ = minRange;
	maxRange_ = maxRange;
	BossMeleeAttackUtility::NormalizeValidRange(minRange_, maxRange_);
}

/// ---------------------------------------------------------------
///					実際の攻撃判定パラメータを設定
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::SetHitParameters(float hitRange, float hitRadius, float hitForwardOffset, float hitAngleDeg)
{
	BossMeleeHitSettings settings{ hitRange, hitRadius, hitForwardOffset, hitAngleDeg };
	BossMeleeAttackUtility::NormalizeHitSettings(settings);
	hitRange_ = settings.range;
	hitRadius_ = settings.radius;
	hitForwardOffset_ = settings.forwardOffset;
	hitAngleDeg_ = settings.angleDeg;
}

void BossHeavyPunchAttack::SetImpactParticleParameters(uint32_t spawnCount, float spawnRadius, float lifetimeScale, float initialSpeedScale)
{
	BossImpactParticleSettings settings{ spawnCount, spawnRadius, lifetimeScale, initialSpeedScale };
	BossMeleeAttackUtility::NormalizeParticleSettings(settings);
	particleSpawnCount_ = settings.spawnCount;
	particleSpawnRadius_ = settings.spawnRadius;
	particleLifetimeScale_ = settings.lifetimeScale;
	particleInitialSpeedScale_ = settings.initialSpeedScale;
}
