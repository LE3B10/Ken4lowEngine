#define NOMINMAX
#include "BossHeavyPunchAttack.h"
#include "Core/BossBase.h"

#include <Windows.h>
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
/// 初期化
/// BossAttackComponent 側から owner を受け取る
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::Initialize(BossBase* owner)
{
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
/// 攻撃開始
/// 条件を満たす場合のみ開始
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::Start()
{
	if (!CanStart())
	{
		return;
	}

	isActive_ = true;
	isFinished_ = false;
	hasHit_ = false;

	phase_ = Phase::None;
	phaseTimer_ = 0.0f;
	totalTimer_ = 0.0f;

	// HeavyPunch は最初に Windup から始まる
	ChangePhase(Phase::Windup);

	DebugLog("[BossHeavyPunchAttack] Start\n");
}

/// ---------------------------------------------------------------
/// 攻撃更新
/// フェーズごとに分岐する
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::Update(float deltaTime)
{
	if (!isActive_)
	{
		return;
	}

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
/// 攻撃終了
/// クールダウン開始
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::End()
{
	if (!isActive_)
	{
		return;
	}

	isActive_ = false;
	isFinished_ = true;
	cooldownRemaining_ = cooldownSec_;

	phase_ = Phase::None;
	phaseTimer_ = 0.0f;

	DebugLog("[BossHeavyPunchAttack] End\n");
}

/// ---------------------------------------------------------------
/// 今この攻撃を開始できるか
/// ---------------------------------------------------------------
bool BossHeavyPunchAttack::CanStart() const
{
	if (owner_ == nullptr)
	{
		return false;
	}

	// 実行中は不可
	if (isActive_)
	{
		return false;
	}

	// クールダウン中は不可
	if (cooldownRemaining_ > 0.0f)
	{
		return false;
	}

	// 死亡中は不可
	if (owner_->IsDead())
	{
		return false;
	}

	// 射程内にターゲットがいないと不可
	if (!IsTargetInValidRange())
	{
		return false;
	}

	return true;
}

/// ---------------------------------------------------------------
/// クールダウン更新
/// 実行中ではないときに進める
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::TickCooldown(float deltaTime)
{
	if (cooldownRemaining_ <= 0.0f)
	{
		return;
	}

	cooldownRemaining_ -= deltaTime;
	if (cooldownRemaining_ < 0.0f)
	{
		cooldownRemaining_ = 0.0f;
	}
}

/// ---------------------------------------------------------------
/// 溜め更新
/// 両腕を上げる予兆フェーズ
/// 終了後は Hold に入る
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::UpdateWindup(float deltaTime)
{
	(void)deltaTime;

	// 将来的にはここで
	// - 予兆SE
	// - 溜めエフェクト
	// - 体の震え
	// なども入れられる

	if (phaseTimer_ >= windupTime_)
	{
		// すぐ殴らず、一瞬止めて予兆を見せる
		ChangePhase(Phase::Hold);
	}
}

/// ---------------------------------------------------------------
/// Hold 更新
/// 溜め切った姿勢を短時間だけ維持する
/// ここが「エヴォーカー風の腕上げ」を見せる時間
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::UpdateHold(float deltaTime)
{
	(void)deltaTime;

	if (phaseTimer_ >= holdTime_)
	{
		ChangePhase(Phase::Active);
	}
}

/// ---------------------------------------------------------------
/// 発生更新
/// 発生中に1回だけヒットを試す
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::UpdateActive(float deltaTime)
{
	(void)deltaTime;

	// 発生した瞬間に1回だけヒット判定
	if (!hasHit_)
	{
		TryHitPlayer();
		hasHit_ = true;
	}

	if (phaseTimer_ >= activeTime_)
	{
		ChangePhase(Phase::Recovery);
	}
}

/// ---------------------------------------------------------------
/// 硬直更新
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::UpdateRecovery(float deltaTime)
{
	(void)deltaTime;

	if (phaseTimer_ >= recoveryTime_)
	{
		End();
	}
}

/// ---------------------------------------------------------------
/// フェーズ切り替え
/// phaseTimer を毎回リセットする
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::ChangePhase(Phase newPhase)
{
	phase_ = newPhase;
	phaseTimer_ = 0.0f;

#ifdef _DEBUG
	switch (phase_)
	{
	case Phase::Windup:
		OutputDebugStringA("[BossHeavyPunchAttack] Phase -> Windup\n");
		break;

	case Phase::Hold:
		OutputDebugStringA("[BossHeavyPunchAttack] Phase -> Hold\n");
		break;

	case Phase::Active:
		OutputDebugStringA("[BossHeavyPunchAttack] Phase -> Active\n");
		break;

	case Phase::Recovery:
		OutputDebugStringA("[BossHeavyPunchAttack] Phase -> Recovery\n");
		break;

	case Phase::None:
	default:
		OutputDebugStringA("[BossHeavyPunchAttack] Phase -> None\n");
		break;
	}
#endif
}

/// ---------------------------------------------------------------
/// 描画
/// 今は未実装
/// 将来的に予兆表示や当たり判定デバッグ描画を置く
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::Draw()
{
	// 例:
	// - Windup/Hold 中は予兆色を出す
	// - Active 中は攻撃球を描く
}

/// ---------------------------------------------------------------
/// ImGui
/// デバッグ確認用
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
	ImGui::Text("Damage            : %.2f", damage_);
#endif
}

/// ---------------------------------------------------------------
/// ヒット判定
/// 発生中に一度だけ行う
///
/// 右腕根本座標 + 前方オフセット位置に攻撃球を置き、
/// プレイヤー仮想球との重なりで簡易判定する
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::TryHitPlayer()
{
	if (owner_ == nullptr)
	{
		return;
	}

	// -----------------------------------------------------------
	// 攻撃中心
	// HeavyPunch は Punch より前に届くよう少し伸ばす
	// -----------------------------------------------------------
	const K4E::Vector3 armRoot = owner_->GetRightArmRootWorldPosition();
	const float yaw = owner_->GetYaw();

	K4E::Vector3 forward
	{
		std::sin(yaw),
		0.0f,
		std::cos(yaw)
	};

	K4E::Vector3 attackCenter = armRoot;
	attackCenter.x += forward.x * hitForwardOffset_;
	attackCenter.y += 0.30f;
	attackCenter.z += forward.z * hitForwardOffset_;

	// -----------------------------------------------------------
	// 仮のプレイヤー中心
	// 今は targetPosition_ を使用
	// -----------------------------------------------------------
	const K4E::Vector3 targetCenter = owner_->GetTargetPosition();

	const float dx = targetCenter.x - attackCenter.x;
	const float dy = targetCenter.y - attackCenter.y;
	const float dz = targetCenter.z - attackCenter.z;

	const float distanceSq = dx * dx + dy * dy + dz * dz;
	const float sumRadius = hitRadius_ + targetRadius_;

	if (distanceSq <= (sumRadius * sumRadius))
	{
#ifdef _DEBUG
		OutputDebugStringA("[BossHeavyPunchAttack] Heavy hit success.\n");
#endif

		// -------------------------------------------------------
		// TODO:
		// 将来的にここで
		// - プレイヤーへダメージ
		// - ノックバック
		// - ヒットストップ
		// - 画面揺れ
		// を追加する
		// -------------------------------------------------------
	}
	else
	{
#ifdef _DEBUG
		OutputDebugStringA("[BossHeavyPunchAttack] Heavy hit miss.\n");
#endif
	}
}

/// ---------------------------------------------------------------
/// 射程内判定
/// HeavyPunch は近～中近距離向け
/// ---------------------------------------------------------------
bool BossHeavyPunchAttack::IsTargetInValidRange() const
{
	if (owner_ == nullptr)
	{
		return false;
	}

	const float distance = owner_->GetDistanceToTargetXZ();
	return (distance >= minRange_ && distance <= maxRange_);
}

/// ---------------------------------------------------------------
/// デバッグ用フェーズ名
/// ---------------------------------------------------------------
const char* BossHeavyPunchAttack::GetPhaseName() const
{
	switch (phase_)
	{
	case Phase::Windup:   return "Windup";
	case Phase::Hold:     return "Hold";
	case Phase::Active:   return "Active";
	case Phase::Recovery: return "Recovery";
	case Phase::None:
	default:              return "None";
	}
}