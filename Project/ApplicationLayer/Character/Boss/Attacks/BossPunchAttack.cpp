#define NOMINMAX
#include "BossPunchAttack.h"
#include "BossBase.h"
#include "BossAttackEffects.h"
#include "BossMeleeAttackUtility.h"
#include "GpuParticleType.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
	/// <summary>
	/// デバッグログ出力
	/// </summary>
	void DebugLog(const std::string& text)
	{
		OutputDebugStringA(text.c_str());
	}
}

/// ---------------------------------------------------------------
///							初期化処理
/// ---------------------------------------------------------------
void BossPunchAttack::Initialize(BossBase* owner)
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
void BossPunchAttack::Start()
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

	// 最初は溜めから入る
	ChangePhase(Phase::Windup);

	// デバッグログ
	DebugLog("[BossPunchAttack] Start\n");
}

/// ---------------------------------------------------------------
///							更新処理
/// ---------------------------------------------------------------
void BossPunchAttack::Update(float deltaTime)
{
	// 無効なときは何もしない
	if (!isActive_) return;

	totalTimer_ += deltaTime; // 攻撃開始からの総時間
	phaseTimer_ += deltaTime; // 現在フェーズに入ってからの時間

	// フェーズごとに更新
	switch (phase_)
	{
	case Phase::Windup: // 溜め
		UpdateWindup(deltaTime);
		break;

	case Phase::Active: // 発生
		UpdateActive(deltaTime);
		break;

	case Phase::Recovery: // 硬直
		UpdateRecovery(deltaTime);
		break;

	case Phase::None: // 無効
	default:
		break;
	}
}

/// ---------------------------------------------------------------
///							攻撃終了
/// ---------------------------------------------------------------
void BossPunchAttack::End()
{
	// すでに無効なときは何もしない
	if (!isActive_) return;

	// 状態リセット
	isActive_ = false;
	isFinished_ = true;
	cooldownRemaining_ = cooldownSec_;

	// フェーズリセット
	phase_ = Phase::None;
	phaseTimer_ = 0.0f;

	// デバッグログ
	DebugLog("[BossPunchAttack] End\n");
}

/// ---------------------------------------------------------------
///					今この攻撃を開始できるか
/// ---------------------------------------------------------------
bool BossPunchAttack::CanStart() const
{
	// オーナーがいないときは開始不可
	if (owner_ == nullptr) return false;

	// 実行中は開始不可
	if (isActive_) return false;

	// クールダウン中は開始不可
	if (cooldownRemaining_ > 0.0f) return false;

	// 死亡中は開始不可
	if (owner_->IsDead()) return false;

	// 攻撃距離内でなければ開始不可
	if (!IsTargetInValidRange()) return false;

	// それ以外は開始可能
	return true;
}

/// ---------------------------------------------------------------
///						クールダウン更新
/// ---------------------------------------------------------------
void BossPunchAttack::TickCooldown(float deltaTime)
{
	// クールダウンがないときは何もしない
	if (cooldownRemaining_ <= 0.0f) return;

	// クールダウンを減らす
	cooldownRemaining_ -= deltaTime;

	// クールダウンが0未満にならないように
	if (cooldownRemaining_ < 0.0f)	cooldownRemaining_ = 0.0f;
}

/// ---------------------------------------------------------------
///				溜め更新 少し構えてから発生へ移る
/// ---------------------------------------------------------------
void BossPunchAttack::UpdateWindup(float deltaTime)
{
	// 今は特に溜め中の挙動はないので、時間だけ進める
	(void)deltaTime;

	// 溜めが終わったら発生へ
	if (phaseTimer_ >= windupTime_)	ChangePhase(Phase::Active);
}

/// ---------------------------------------------------------------
///			発生更新 フェーズ中に1回だけヒット判定を出す
/// ---------------------------------------------------------------
void BossPunchAttack::UpdateActive(float deltaTime)
{
	// 今は特に発生中の挙動はないので、時間だけ進める
	(void)deltaTime;

	// 発生中に一度だけヒット判定
	if (!hasHit_)
	{
		TryHitPlayer(); // ヒット判定を試みる
		hasHit_ = true; // これ以降はヒット判定しない
	}

	// 発生時間を過ぎたら硬直へ
	if (phaseTimer_ >= activeTime_) ChangePhase(Phase::Recovery);
}

/// ---------------------------------------------------------------
///						硬直更新 攻撃後の隙
/// ---------------------------------------------------------------
void BossPunchAttack::UpdateRecovery(float deltaTime)
{
	// 今は特に硬直中の挙動はないので、時間だけ進める
	(void)deltaTime;

	// 硬直時間を過ぎたら攻撃終了
	if (phaseTimer_ >= recoveryTime_) End();
}

/// ---------------------------------------------------------------
///			フェーズ切り替え phaseTimer_ を毎回リセットする
/// ---------------------------------------------------------------
void BossPunchAttack::ChangePhase(Phase newPhase)
{
	// フェーズ切り替え
	phase_ = newPhase;
	phaseTimer_ = 0.0f;

#ifdef _DEBUG
	switch (phase_)
	{
	case Phase::Windup: // 溜め
		OutputDebugStringA("[BossPunchAttack] Phase -> Windup\n");
		break;

	case Phase::Active: // 発生
		OutputDebugStringA("[BossPunchAttack] Phase -> Active\n");
		break;

	case Phase::Recovery: // 硬直
		OutputDebugStringA("[BossPunchAttack] Phase -> Recovery\n");
		break;

	case Phase::None: // 無効
	default:
		OutputDebugStringA("[BossPunchAttack] Phase -> None\n");
		break;
	}
#endif
}

void BossPunchAttack::Draw()
{
}

/// ---------------------------------------------------------------
///							ImGui描画処理
/// ---------------------------------------------------------------
void BossPunchAttack::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Separator();
	ImGui::Text("[BossPunchAttack]");
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
///					ヒット判定を一度だけ発生させる
/// ---------------------------------------------------------------
void BossPunchAttack::TryHitPlayer()
{
	if (owner_ == nullptr) return;

	const K4E::Vector3 forward = hasLockedDirection_
		? lockedForward_
		: K4E::Vector3{ std::sin(owner_->GetYaw()), 0.0f, std::cos(owner_->GetYaw()) };
	const BossMeleeHitSettings settings{ hitRange_, hitRadius_, hitForwardOffset_, hitAngleDeg_, targetRadius_, 0.25f };
	K4E::Vector3 attackCenter{};
	if (BossMeleeAttackUtility::TryCalculateHitCenter(*owner_, forward, settings, attackCenter))
	{
#ifdef _DEBUG
		OutputDebugStringA("[BossPunchAttack] Melee hit success.\n");
#endif

		// ボス近接攻撃の発生フレームで1回だけPlayerへダメージを流す。
		if (owner_->ApplyDamageToTargetPlayer(damage_, &attackCenter))
		{
			BossAttackEffects::EmitGuardianHitEffect("GuardianPunchImpact", K4E::GpuParticleType::Spark, attackCenter, particleSpawnCount_, particleSpawnRadius_, particleLifetimeScale_, particleInitialSpeedScale_);
			DebugLog("[BossPunchAttack] Player damage applied.\n");
		}
	}
	else
	{
#ifdef _DEBUG
		OutputDebugStringA("[BossPunchAttack] Melee hit miss.\n");
#endif
	}
}


void BossPunchAttack::LockAttackDirection()
{
	hasLockedDirection_ = BossMeleeAttackUtility::LockDirection(owner_, lockedForward_);
}

/// ---------------------------------------------------------------
///							有効距離内か
/// ---------------------------------------------------------------
bool BossPunchAttack::IsTargetInValidRange() const
{
	return BossMeleeAttackUtility::IsTargetInRange(owner_, minRange_, maxRange_);
}

/// ---------------------------------------------------------------
///					デバッグ用フェーズ名
/// ---------------------------------------------------------------
const char* BossPunchAttack::GetPhaseName() const
{
	// フェーズ列挙型を文字列に変換して返す
	switch (phase_)
	{
	case Phase::Windup:   return "Windup";	 // 溜め
	case Phase::Active:   return "Active";	 // 発生
	case Phase::Recovery: return "Recovery"; // 硬直
	case Phase::None:
	default:              return "None";	 // 無効
	}
}
/// ---------------------------------------------------------------
///						攻撃開始距離を設定
/// ---------------------------------------------------------------
void BossPunchAttack::SetValidRange(float minRange, float maxRange)
{
	minRange_ = minRange;
	maxRange_ = maxRange;
	BossMeleeAttackUtility::NormalizeValidRange(minRange_, maxRange_);
}

/// ---------------------------------------------------------------
///					実際の攻撃判定パラメータを設定
/// ---------------------------------------------------------------
void BossPunchAttack::SetHitParameters(float hitRange, float hitRadius, float hitForwardOffset, float hitAngleDeg)
{
	BossMeleeHitSettings settings{ hitRange, hitRadius, hitForwardOffset, hitAngleDeg };
	BossMeleeAttackUtility::NormalizeHitSettings(settings);
	hitRange_ = settings.range;
	hitRadius_ = settings.radius;
	hitForwardOffset_ = settings.forwardOffset;
	hitAngleDeg_ = settings.angleDeg;
}

void BossPunchAttack::SetImpactParticleParameters(uint32_t spawnCount, float spawnRadius, float lifetimeScale, float initialSpeedScale)
{
	BossImpactParticleSettings settings{ spawnCount, spawnRadius, lifetimeScale, initialSpeedScale };
	BossMeleeAttackUtility::NormalizeParticleSettings(settings);
	particleSpawnCount_ = settings.spawnCount;
	particleSpawnRadius_ = settings.spawnRadius;
	particleLifetimeScale_ = settings.lifetimeScale;
	particleInitialSpeedScale_ = settings.initialSpeedScale;
}
