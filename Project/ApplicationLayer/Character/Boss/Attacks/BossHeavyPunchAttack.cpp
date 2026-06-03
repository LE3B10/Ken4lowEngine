#define NOMINMAX
#include "BossHeavyPunchAttack.h"
#include "BossBase.h"
#include "Player.h"

#ifdef _DEBUG
#include <Wireframe.h>
#endif

#include <LogString.h>
#include <cmath>

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
	attackHitApplied_ = false;
	damageDebugState_ = {};
	attackSettings_.attackRadius = 3.25f;
	attackSettings_.attackDistance = 3.35f;
	attackSettings_.attackHeight = 1.10f;
	attackSettings_.attackForwardOffset = 0.0f;
	attackSettings_.attackActiveStartTime = 0.67f;
	attackSettings_.attackActiveEndTime = 0.90f;
	attackSettings_.attackDamage = 40;

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
	attackHitApplied_ = false;

	// フェーズリセット
	phase_ = Phase::None;
	phaseTimer_ = 0.0f;
	totalTimer_ = 0.0f;

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
	damageDebugState_.attackCenter = CalculateAttackCenter();
	damageDebugState_.isAttackActive = IsAttackWindowActive();
	if (owner_)
	{
		const K4E::Vector3 targetCenter = owner_->GetTargetPosition();
		const float dx = targetCenter.x - damageDebugState_.attackCenter.x;
		const float dy = targetCenter.y - damageDebugState_.attackCenter.y;
		const float dz = targetCenter.z - damageDebugState_.attackCenter.z;
		damageDebugState_.distanceToPlayer = std::sqrt(dx * dx + dy * dy + dz * dz);
		if (auto* player = owner_->GetTargetPlayer())
		{
			damageDebugState_.playerHp = player->GetHP();
		}
	}

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

	// 攻撃判定時間内だけヒット判定を試み、命中後はattackHitApplied_で多段ヒットを防ぐ。
	if (!attackHitApplied_ && IsAttackWindowActive())
	{
		TryHitPlayer();
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
	DrawAttackRangeDebug();
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
	ImGui::Text("attackHitApplied  : %s", attackHitApplied_ ? "true" : "false");
	ImGui::Text("Phase             : %s", GetPhaseName());
	ImGui::Text("PhaseTimer        : %.2f", phaseTimer_);
	ImGui::Text("TotalTimer        : %.2f", totalTimer_);
	ImGui::Text("CooldownRemaining : %.2f", cooldownRemaining_);
	ImGui::Text("Range             : %.2f - %.2f", minRange_, maxRange_);
	ImGui::Text("攻撃判定中か: %s", damageDebugState_.isAttackActive ? "はい" : "いいえ");
	ImGui::Text("ボス攻撃ヒット回数: %d", damageDebugState_.bossAttackHitCount);
	ImGui::Text("最後にプレイヤーへ与えたボスダメージ: %.1f", damageDebugState_.lastPlayerDamage);
	ImGui::Text("Player HP: %.1f", damageDebugState_.playerHp);
	ImGui::Text("攻撃判定中心座標: %.2f, %.2f, %.2f", damageDebugState_.attackCenter.x, damageDebugState_.attackCenter.y, damageDebugState_.attackCenter.z);
	ImGui::Text("プレイヤーとの距離: %.2f", damageDebugState_.distanceToPlayer);
	ImGui::DragFloat("ボス攻撃半径", &attackSettings_.attackRadius, 0.05f, 0.1f, 20.0f);
	ImGui::DragFloat("ボス攻撃距離", &attackSettings_.attackDistance, 0.05f, 0.0f, 30.0f);
	ImGui::DragFloat("ボス攻撃高さ", &attackSettings_.attackHeight, 0.05f, -5.0f, 10.0f);
	ImGui::DragFloat("ボス攻撃前方オフセット", &attackSettings_.attackForwardOffset, 0.05f, -10.0f, 10.0f);
	ImGui::DragInt("ボス攻撃ダメージ", &attackSettings_.attackDamage, 1, 0, 999);
	ImGui::DragFloat("攻撃判定開始時間", &attackSettings_.attackActiveStartTime, 0.01f, 0.0f, 10.0f);
	ImGui::DragFloat("攻撃判定終了時間", &attackSettings_.attackActiveEndTime, 0.01f, 0.0f, 10.0f);
	ImGui::Checkbox("ボス攻撃範囲表示", &debugSettings_.showAttackRange);
#endif
}

/// ---------------------------------------------------------------
///							ヒット判定
/// ---------------------------------------------------------------
void BossHeavyPunchAttack::TryHitPlayer()
{
	if (owner_ == nullptr || attackHitApplied_ || !IsAttackWindowActive()) return;

	// ボス攻撃範囲を調整しやすくするため、前方オフセット付きの球判定としてImGuiから編集できるようにする。
	const K4E::Vector3 attackCenter = CalculateAttackCenter();
	const K4E::Vector3 targetCenter = owner_->GetTargetPosition();

	const float dx = targetCenter.x - attackCenter.x;
	const float dy = targetCenter.y - attackCenter.y;
	const float dz = targetCenter.z - attackCenter.z;
	const float distanceSq = dx * dx + dy * dy + dz * dz;
	const float distance = std::sqrt(distanceSq);
	const float sumRadius = attackSettings_.attackRadius + targetRadius_;

	damageDebugState_.attackCenter = attackCenter;
	damageDebugState_.distanceToPlayer = distance;
	damageDebugState_.isAttackActive = true;

	if (distanceSq <= (sumRadius * sumRadius))
	{
#ifdef _DEBUG
		Log("[BossHeavyPunchAttack] Heavy hit success.\n");
#endif

		if (owner_->ApplyDamageToTargetPlayer(static_cast<float>(attackSettings_.attackDamage), &attackCenter))
		{
			attackHitApplied_ = true;
			hasHit_ = true;
			++damageDebugState_.bossAttackHitCount;
			damageDebugState_.lastPlayerDamage = static_cast<float>(attackSettings_.attackDamage);
			if (auto* player = owner_->GetTargetPlayer())
			{
				damageDebugState_.playerHp = player->GetHP();
			}
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

K4E::Vector3 BossHeavyPunchAttack::CalculateAttackCenter() const
{
	if (!owner_) return {};

	const float yaw = owner_->GetYaw();
	const K4E::Vector3 forward{ std::sin(yaw), 0.0f, std::cos(yaw) };
	const float forwardDistance = attackSettings_.attackDistance + attackSettings_.attackForwardOffset;
	K4E::Vector3 attackCenter = owner_->GetPosition();
	attackCenter.x += forward.x * forwardDistance;
	attackCenter.y += attackSettings_.attackHeight;
	attackCenter.z += forward.z * forwardDistance;
	return attackCenter;
}

bool BossHeavyPunchAttack::IsAttackWindowActive() const
{
	return isActive_
		&& totalTimer_ >= attackSettings_.attackActiveStartTime
		&& totalTimer_ <= attackSettings_.attackActiveEndTime;
}

void BossHeavyPunchAttack::DrawAttackRangeDebug()
{
#ifdef _DEBUG
	if (!debugSettings_.showAttackRange || !owner_) return;

	auto* wireframe = K4E::Wireframe::GetInstance();
	if (!wireframe || !wireframe->IsDebugDrawEnabled()) return;

	const K4E::Vector3 center = CalculateAttackCenter();
	const K4E::Vector4 color = IsAttackWindowActive() ? K4E::Vector4{ 1.0f, 0.05f, 0.05f, 1.0f } : K4E::Vector4{ 1.0f, 0.55f, 0.25f, 0.35f };
	wireframe->DrawSphere(center, attackSettings_.attackRadius, color);
	wireframe->DrawLine(owner_->GetPosition(), center, color);
#endif
}

/// ---------------------------------------------------------------
///							射程内判定
/// ---------------------------------------------------------------
bool BossHeavyPunchAttack::IsTargetInValidRange() const
{
	// 所有者がいないときは射程外とみなす
	if (owner_ == nullptr) return false;

	// ターゲットとの水平距離を計算して、射程内かどうかを判断する
	const float distance = owner_->GetDistanceToTargetXZ();

	// 射程内かどうかを判断
	return (distance >= minRange_ && distance <= attackSettings_.attackDistance + attackSettings_.attackRadius + targetRadius_);
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