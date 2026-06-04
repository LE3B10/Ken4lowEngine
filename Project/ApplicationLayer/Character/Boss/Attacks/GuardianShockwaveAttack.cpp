#define NOMINMAX
#include "GuardianShockwaveAttack.h"
#include "BossBase.h"
#include "Wireframe.h"
#include <LogString.h>

#include <algorithm>
#include <cmath>
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

	phase_ = Phase::None;
	phaseTimer_ = 0.0f;
	totalTimer_ = 0.0f;

	cooldownRemaining_ = 0.0f;
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

	phase_ = Phase::None;
	phaseTimer_ = 0.0f;
	totalTimer_ = 0.0f;

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
	// 予備動作中はタイマーだけ進め、叩きつけタイミングで判定発生へ移る。
	(void)deltaTime;

	if (phaseTimer_ >= startupTime_) ChangePhase(Phase::Active);
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
	const K4E::Vector3 bossCenter = owner_->GetCenterPosition();
	const float yaw = owner_->GetYaw();
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
	ImGui::Text("Startup/Active/Recovery : %.2f / %.2f / %.2f", startupTime_, activeTime_, recoveryTime_);
#endif
}

/// ---------------------------------------------------------------
///                         ヒット判定
/// ---------------------------------------------------------------
void GuardianShockwaveAttack::TryHitPlayer()
{
	if (owner_ == nullptr) return;

	// 衝撃波判定はボス正面を中心とした前方扇形で、攻撃開始距離とは別に計算する。
	const K4E::Vector3 bossCenter = owner_->GetCenterPosition();
	const float yaw = owner_->GetYaw();
	const K4E::Vector3 targetCenter = owner_->GetTargetPosition();

	K4E::Vector3 forward
	{
		std::sin(yaw),
		0.0f,
		std::cos(yaw)
	};

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
	startupTime_ = std::max(0.0f, startupSec);
	activeTime_ = std::max(0.0f, activeSec);
	recoveryTime_ = std::max(0.0f, recoverySec);
	cooldownSec_ = std::max(0.0f, cooldownSec);
}
