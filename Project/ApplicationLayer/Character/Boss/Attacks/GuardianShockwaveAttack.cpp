#define NOMINMAX
#include "GuardianShockwaveAttack.h"
#include "BossBase.h"
#include "BossAttackEffects.h"
#include "Wireframe.h"
#include "GpuParticleManager.h"
#include "GpuParticleEmitter.h"
#include <LogString.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace Ken4lowEngine;

namespace
{
	/// <summary>
	/// 度数法をラジアンへ変換する
	/// </summary>
	float DegToRad(float degree)
	{
		constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
		return degree * kDegToRad;
	}

	float Clamp01(float value)
	{
		return std::clamp(value, 0.0f, 1.0f);
	}

	K4E::Vector3 MakeShockwavePoint(const K4E::Vector3& origin, float yaw, float localAngle, float distance)
	{
		const float worldAngle = yaw + localAngle;
		K4E::Vector3 point = origin;
		point.x += std::sin(worldAngle) * distance;
		point.z += std::cos(worldAngle) * distance;
		return point;
	}

	void EmitShockwaveRangeTelegraph(
		const char* emitterPrefix,
		const K4E::Vector3& origin,
		const K4E::Vector3& forward,
		float range,
		float angleDeg,
		float progress,
		uint32_t emitCount,
		float radius,
		float lifeScale,
		float speedScale)
	{
		if (emitterPrefix == nullptr || emitCount == 0) return;

		const float clampedRange = std::max(0.0f, range);
		if (clampedRange <= 0.01f) return;

		const float clampedAngleDeg = std::clamp(angleDeg, 0.0f, 360.0f);
		const float halfAngle = DegToRad(clampedAngleDeg * 0.5f);
		const float yaw = std::atan2(forward.x, forward.z);
		const float visibleRange = std::max(1.0f, clampedRange * Clamp01(progress));

		K4E::Vector3 base = origin;
		base.y += 0.12f;

		// 扇形の外周を複数点で光らせ、ショックウェーブの攻撃範囲をプレイヤーに見せる。
		constexpr int kArcSegments = 8;
		for (int i = 0; i <= kArcSegments; ++i)
		{
			const float t = static_cast<float>(i) / static_cast<float>(kArcSegments);
			const float localAngle = -halfAngle + (halfAngle * 2.0f * t);
			const K4E::Vector3 pos = MakeShockwavePoint(base, yaw, localAngle, visibleRange);

			char emitterName[96]{};
			std::snprintf(emitterName, sizeof(emitterName), "%sArc%02d", emitterPrefix, i);
			BossAttackEffects::EmitGuardianAttackPresenceEffect(emitterName, K4E::GpuParticleType::Shockwave, pos, emitCount, radius, lifeScale, speedScale);
		}

		// 中央ラインにも粒子を置き、範囲が徐々に前へ伸びてくることを分かりやすくする。
		constexpr int kForwardSegments = 4;
		for (int i = 1; i <= kForwardSegments; ++i)
		{
			const float t = static_cast<float>(i) / static_cast<float>(kForwardSegments);
			K4E::Vector3 pos = base;
			pos.x += forward.x * visibleRange * t;
			pos.z += forward.z * visibleRange * t;

			char emitterName[96]{};
			std::snprintf(emitterName, sizeof(emitterName), "%sLine%02d", emitterPrefix, i);
			BossAttackEffects::EmitGuardianAttackPresenceEffect(emitterName, K4E::GpuParticleType::Trail, pos, emitCount, radius * 0.75f, lifeScale, speedScale);
		}
	}
}

/// ---------------------------------------------------------------
///                         初期化処理
/// ---------------------------------------------------------------
void GuardianShockwaveAttack::Initialize(BossBase* owner)
{
	// 攻撃所有者を受け取って初期化する。
	owner_ = owner;

	isActive_ = false;
	isFinished_ = false;
	hasHit_ = false;
	hasTelegraphEffect_ = false;

	phase_ = Phase::None;
	phaseTimer_ = 0.0f;
	totalTimer_ = 0.0f;

	cooldownRemaining_ = 0.0f;
	hasLockedDirection_ = false;
}

/// ---------------------------------------------------------------
///                         攻撃開始
/// ---------------------------------------------------------------
void GuardianShockwaveAttack::Start()
{
	// 条件を満たさないときは開始しない。
	if (!CanStart()) return;

	isActive_ = true;
	isFinished_ = false;
	hasHit_ = false;
	hasTelegraphEffect_ = false;

	phase_ = Phase::None;
	phaseTimer_ = 0.0f;
	totalTimer_ = 0.0f;

	LockShockwaveDirection(); // 開始時点のプレイヤー方向をワールド座標で固定し、攻撃中の反転を防ぐ。

	// 衝撃波は叩きつけ前の予備動作から開始する。
	ChangePhase(Phase::Windup);

	Log("[GuardianShockwaveAttack] Start\n");
}

/// ---------------------------------------------------------------
///                         更新処理
/// ---------------------------------------------------------------
void GuardianShockwaveAttack::Update(float deltaTime)
{
	// 無効なときは何もしない。
	if (!isActive_) return;

	totalTimer_ += deltaTime;
	phaseTimer_ += deltaTime;

	switch (phase_)
	{
	case Phase::Windup:
		UpdateWindup(deltaTime);
		break;
	case Phase::Charge:
		UpdateCharge(deltaTime);
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
///                         攻撃終了
/// ---------------------------------------------------------------
void GuardianShockwaveAttack::End()
{
	// すでに無効なときは何もしない。
	if (!isActive_) return;

	isActive_ = false;
	isFinished_ = true;
	cooldownRemaining_ = cooldownSec_;

	phase_ = Phase::None;
	phaseTimer_ = 0.0f;

	Log("[GuardianShockwaveAttack] End\n");
}

/// ---------------------------------------------------------------
///                 今この攻撃を開始できるか
/// ---------------------------------------------------------------
bool GuardianShockwaveAttack::CanStart() const
{
	// 所有者がいないときは不可。
	if (owner_ == nullptr) return false;

	// 実行中やクールダウン中は不可。
	if (isActive_) return false;
	if (cooldownRemaining_ > 0.0f) return false;

	// 死亡中は不可。
	if (owner_->IsDead()) return false;

	// 攻撃開始距離は実ヒットリーチとは別の開始条件として判定する。
	if (!IsTargetInValidRange()) return false;

	return true;
}

/// ---------------------------------------------------------------
///                     クールダウン更新
/// ---------------------------------------------------------------
void GuardianShockwaveAttack::TickCooldown(float deltaTime)
{
	// クールダウンがないときは何もしない。
	if (cooldownRemaining_ <= 0.0f) return;

	cooldownRemaining_ -= deltaTime;
	if (cooldownRemaining_ < 0.0f) cooldownRemaining_ = 0.0f;
}

/// ---------------------------------------------------------------
///                 予備動作更新
/// ---------------------------------------------------------------
void GuardianShockwaveAttack::UpdateWindup(float deltaTime)
{
	(void)deltaTime;

	const float progress = startupTime_ <= 0.0f ? 1.0f : Clamp01(phaseTimer_ / startupTime_);
	// Windup中は攻撃範囲を小さく出し始め、これからショックウェーブが来ることを予告する。
	EmitShockwaveRangeTelegraph(
		"GuardianShockwaveWarn",
		lockedOrigin_,
		lockedForward_,
		shockwaveRange_,
		shockwaveAngleDeg_,
		0.25f + progress * 0.35f,
		1,
		0.28f + progress * 0.18f,
		0.22f,
		0.25f + progress * 0.35f);

	if (!hasTelegraphEffect_ && owner_ != nullptr)
	{
		hasTelegraphEffect_ = true;
		K4E::Vector3 telegraphPos = lockedOrigin_;
		telegraphPos.y += 0.10f;
		// 発生前の溜め位置に小さなリングを出して、回避タイミングを読ませる。
		BossAttackEffects::EmitGuardianAttackPresenceEffect("GuardianShockwaveTelegraph", K4E::GpuParticleType::Shockwave, telegraphPos, std::max<uint32_t>(8, particleSpawnCount_ / 3), std::max(0.3f, particleSpawnRadius_ * 0.75f), std::max(0.2f, startupTime_), 0.25f);
	}

	if (phaseTimer_ >= startupTime_) ChangePhase(Phase::Charge);
}

/// ---------------------------------------------------------------
///                 溜め更新
/// ---------------------------------------------------------------
void GuardianShockwaveAttack::UpdateCharge(float deltaTime)
{
	(void)deltaTime;

	const float progress = chargeTime_ <= 0.0f ? 1.0f : Clamp01(phaseTimer_ / chargeTime_);
	// Charge中は範囲を最大まで広げ、発生直前に粒子密度を上げて危険度を強調する。
	EmitShockwaveRangeTelegraph(
		"GuardianShockwaveCharge",
		lockedOrigin_,
		lockedForward_,
		shockwaveRange_,
		shockwaveAngleDeg_,
		0.60f + progress * 0.40f,
		2,
		0.42f + progress * 0.28f,
		0.18f,
		0.65f + progress * 0.55f);

	// 叩きつけ直前の溜めで攻撃発生タイミングを読みやすくする。
	if (phaseTimer_ >= chargeTime_) ChangePhase(Phase::Active);
}

/// ---------------------------------------------------------------
///                 判定発生更新
/// ---------------------------------------------------------------
void GuardianShockwaveAttack::UpdateActive(float deltaTime)
{
	// 判定時間中に一度だけダメージ判定を出す。
	(void)deltaTime;

	if (!hasHit_)
	{
		// Active開始時は最大範囲へ強い粒子を出し、ここが実際の攻撃判定であることを明確にする。
		EmitShockwaveRangeTelegraph(
			"GuardianShockwaveActive",
			lockedOrigin_,
			lockedForward_,
			shockwaveRange_,
			shockwaveAngleDeg_,
			1.0f,
			4,
			0.75f,
			0.35f,
			1.35f);

		TryHitPlayer();
		hasHit_ = true;
	}

	if (phaseTimer_ >= activeTime_) ChangePhase(Phase::Recovery);
}

/// ---------------------------------------------------------------
///                     後隙更新
/// ---------------------------------------------------------------
void GuardianShockwaveAttack::UpdateRecovery(float deltaTime)
{
	// 後隙が終わるまで次の攻撃へ移れないようにする。
	(void)deltaTime;

	if (phaseTimer_ >= recoveryTime_) End();
}

/// ---------------------------------------------------------------
///                     フェーズ切り替え
/// ---------------------------------------------------------------
void GuardianShockwaveAttack::ChangePhase(Phase newPhase)
{
	// フェーズ切り替え時にフェーズ内時間をリセットする。
	phase_ = newPhase;
	phaseTimer_ = 0.0f;

#ifdef _DEBUG
	switch (phase_)
	{
	case Phase::Windup:
		OutputDebugStringA("[GuardianShockwaveAttack] Phase -> Windup\n");
		break;
	case Phase::Charge:
		OutputDebugStringA("[GuardianShockwaveAttack] Phase -> Charge\n");
		break;
	case Phase::Active:
		OutputDebugStringA("[GuardianShockwaveAttack] Phase -> Active\n");
		break;
	case Phase::Recovery:
		OutputDebugStringA("[GuardianShockwaveAttack] Phase -> Recovery\n");
		break;
	case Phase::None:
	default:
		OutputDebugStringA("[GuardianShockwaveAttack] Phase -> None\n");
		break;
	}
#endif
}

/// ---------------------------------------------------------------
///                         描画処理
/// ---------------------------------------------------------------
void GuardianShockwaveAttack::Draw()
{
#ifdef _DEBUG
	if (owner_ == nullptr || !isActive_) return;

	// Debugビルドでは衝撃波の前方扇形をワイヤーで可視化する。
	const K4E::Vector3 bossCenter = hasLockedDirection_ ? lockedOrigin_ : owner_->GetCenterPosition();
	const K4E::Vector3 forward = hasLockedDirection_ ? lockedForward_ : K4E::Vector3{ std::sin(owner_->GetYaw()), 0.0f, std::cos(owner_->GetYaw()) };
	const float yaw = std::atan2(forward.x, forward.z);
	const float clampedRange = std::max(0.0f, shockwaveRange_);
	const float clampedAngle = std::clamp(shockwaveAngleDeg_, 0.0f, 360.0f);
	const float halfAngleRad = DegToRad(clampedAngle * 0.5f);

	K4E::Vector3 origin = bossCenter;
	origin.y += 0.15f;

	const Ken4lowEngine::Vector4 color = (phase_ == Phase::Active)
		? Ken4lowEngine::Vector4{ 1.0f, 0.25f, 0.05f, 1.0f }
		: Ken4lowEngine::Vector4{ 1.0f, 0.9f, 0.05f, 1.0f };

	Ken4lowEngine::Wireframe* wireframe = Ken4lowEngine::Wireframe::GetInstance();
	if (!wireframe) return;

	const int segmentCount = 12;
	K4E::Vector3 previousPoint = origin;
	for (int i = 0; i <= segmentCount; ++i)
	{
		const float t = static_cast<float>(i) / static_cast<float>(segmentCount);
		const float localAngle = -halfAngleRad + (halfAngleRad * 2.0f * t);
		const float worldAngle = yaw + localAngle;

		K4E::Vector3 edgePoint = origin;
		edgePoint.x += std::sin(worldAngle) * clampedRange;
		edgePoint.z += std::cos(worldAngle) * clampedRange;

		wireframe->DrawLine(origin, edgePoint, color);
		if (i > 0)
		{
			wireframe->DrawLine(previousPoint, edgePoint, color);
		}
		previousPoint = edgePoint;
	}
#endif
}

/// ---------------------------------------------------------------
///                         ImGui描画処理
/// ---------------------------------------------------------------
void GuardianShockwaveAttack::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Separator();
	ImGui::Text("[GuardianShockwaveAttack]");
	ImGui::Text("Active            : %s", isActive_ ? "true" : "false");
	ImGui::Text("Finished          : %s", isFinished_ ? "true" : "false");
	ImGui::Text("HasHit            : %s", hasHit_ ? "true" : "false");
	ImGui::Text("Phase             : %s", GetPhaseName());
	ImGui::Text("PhaseTimer        : %.2f", phaseTimer_);
	ImGui::Text("TotalTimer        : %.2f", totalTimer_);
	ImGui::Text("CooldownRemaining : %.2f", cooldownRemaining_);
	ImGui::Text("StartRange        : %.2f - %.2f", minRange_, maxRange_);
	ImGui::Text("ShockwaveRange    : %.2f", shockwaveRange_);
	ImGui::Text("ShockwaveAngleDeg : %.2f", shockwaveAngleDeg_);
	ImGui::Text("Damage            : %.2f", damage_);
	ImGui::Text("Startup/Charge/Active/Recovery : %.2f / %.2f / %.2f / %.2f", startupTime_, chargeTime_, activeTime_, recoveryTime_);
#endif
}

/// ---------------------------------------------------------------
///                         ヒット判定
/// ---------------------------------------------------------------
void GuardianShockwaveAttack::TryHitPlayer()
{
	if (owner_ == nullptr) return;

	// 衝撃波判定はボス正面を中心とした前方扇形で、攻撃開始距離とは別に計算する。
	const K4E::Vector3 bossCenter = hasLockedDirection_ ? lockedOrigin_ : owner_->GetCenterPosition();
	const K4E::Vector3 targetCenter = owner_->GetTargetPosition();
	const K4E::Vector3 forward = hasLockedDirection_ ? lockedForward_ : K4E::Vector3{ std::sin(owner_->GetYaw()), 0.0f, std::cos(owner_->GetYaw()) };

	const float toTargetX = targetCenter.x - bossCenter.x;
	const float toTargetZ = targetCenter.z - bossCenter.z;
	const float horizontalDistanceSq = toTargetX * toTargetX + toTargetZ * toTargetZ;
	const float horizontalDistance = std::sqrt(horizontalDistanceSq);
	const float forwardDistance = toTargetX * forward.x + toTargetZ * forward.z;

	const float clampedRange = std::max(0.0f, shockwaveRange_);
	const float clampedAngleDeg = std::clamp(shockwaveAngleDeg_, 0.0f, 360.0f);
	const float directionDot = (horizontalDistance > 0.0001f) ? (forwardDistance / horizontalDistance) : 1.0f;
	const float angleCos = std::cos(DegToRad(clampedAngleDeg * 0.5f));

	const bool isInsideForward = forwardDistance >= -targetRadius_;
	const bool isInsideRange = horizontalDistance <= (clampedRange + targetRadius_);
	const bool isInsideAngle = (clampedAngleDeg >= 360.0f) || (directionDot >= angleCos);

	if (isInsideForward && isInsideRange && isInsideAngle)
	{
		K4E::Vector3 hitPosition = bossCenter;
		hitPosition.x += forward.x * std::clamp(forwardDistance, 0.0f, clampedRange);
		hitPosition.y += 0.15f;
		hitPosition.z += forward.z * std::clamp(forwardDistance, 0.0f, clampedRange);

#ifdef _DEBUG
		Log("[GuardianShockwaveAttack] Shockwave hit success.\n");
#endif

		// 衝撃波の発生フレームで1回だけPlayerへダメージを流す。
		if (owner_->ApplyDamageToTargetPlayer(damage_, &hitPosition))
		{
			BossAttackEffects::EmitGuardianHitEffect("GuardianShockwaveImpact", K4E::GpuParticleType::Shockwave, hitPosition, particleSpawnCount_, particleSpawnRadius_, particleLifetimeScale_, particleInitialSpeedScale_);
			Log("[GuardianShockwaveAttack] Player damage applied.\n");
		}
	}
	else
	{
#ifdef _DEBUG
		Log("[GuardianShockwaveAttack] Shockwave hit miss.\n");
#endif
	}
}

void GuardianShockwaveAttack::LockShockwaveDirection()
{
	if (owner_ == nullptr)
	{
		hasLockedDirection_ = false;
		return;
	}

	lockedOrigin_ = owner_->GetCenterPosition();
	lockedForward_ = owner_->GetDirectionToTargetXZOrForward(lockedOrigin_);
	owner_->FaceDirectionXZImmediate(lockedForward_); // 衝撃波は開始時に固定した前方だけで判定・エフェクトを出す。
	hasLockedDirection_ = true;
}

/// ---------------------------------------------------------------
///                         有効距離内か
/// ---------------------------------------------------------------
bool GuardianShockwaveAttack::IsTargetInValidRange() const
{
	// オーナーがいないときは距離判定できないので無効。
	if (owner_ == nullptr) return false;

	const float distance = owner_->GetDistanceToTargetXZ();
	return (distance >= minRange_ && distance <= maxRange_);
}

/// ---------------------------------------------------------------
///                     デバッグ用フェーズ名
/// ---------------------------------------------------------------
const char* GuardianShockwaveAttack::GetPhaseName() const
{
	switch (phase_)
	{
	case Phase::Windup:   return "Windup";
	case Phase::Charge:   return "Charge";
	case Phase::Active:   return "Active";
	case Phase::Recovery: return "Recovery";
	case Phase::None:
	default:              return "None";
	}
}

/// ---------------------------------------------------------------
///                     攻撃開始距離を設定
/// ---------------------------------------------------------------
void GuardianShockwaveAttack::SetValidRange(float minRange, float maxRange)
{
	// 攻撃開始距離は実際の衝撃波リーチとは別管理にしてAI選択条件だけを更新する。
	minRange_ = std::max(0.0f, minRange);
	maxRange_ = std::max(minRange_, maxRange);
}

/// ---------------------------------------------------------------
///                 衝撃波判定パラメータを設定
/// ---------------------------------------------------------------
void GuardianShockwaveAttack::SetShockwaveParameters(float range, float angleDeg, float damage)
{
	// ParameterManagerの反映ボタンから、衝撃波の届く距離・扇形角度・ダメージを実行中攻撃へ差し替える。
	shockwaveRange_ = std::max(0.0f, range);
	shockwaveAngleDeg_ = std::clamp(angleDeg, 0.0f, 360.0f);
	damage_ = std::max(0.0f, damage);
}

/// ---------------------------------------------------------------
///                 衝撃波の時間パラメータを設定
/// ---------------------------------------------------------------
void GuardianShockwaveAttack::SetTimingParameters(float startupSec, float activeSec, float recoverySec, float cooldownSec)
{
	// 予備動作・判定時間・後隙・クールタイムをParameterManagerから調整可能にする。
	const float clampedStartup = std::max(0.0f, startupSec);
	startupTime_ = clampedStartup * 0.7f;
	chargeTime_ = clampedStartup * 0.3f;
	activeTime_ = std::max(0.0f, activeSec);
	recoveryTime_ = std::max(0.0f, recoverySec);
	cooldownSec_ = std::max(0.0f, cooldownSec);
}

void GuardianShockwaveAttack::SetImpactParticleParameters(uint32_t spawnCount, float spawnRadius, float lifetimeScale, float initialSpeedScale)
{
	// ParameterManagerのヒット演出値を、次回Shockwave命中時のGPUパーティクルへ反映する。
	particleSpawnCount_ = spawnCount;
	particleSpawnRadius_ = std::max(0.0f, spawnRadius);
	particleLifetimeScale_ = std::max(0.01f, lifetimeScale);
	particleInitialSpeedScale_ = std::max(0.0f, initialSpeedScale);
}
