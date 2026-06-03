#define NOMINMAX
#include "BossPunchAttack.h"
#include "BossBase.h"
#include "Player.h"

#ifdef _DEBUG
#include <Wireframe.h>
#endif

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
	attackHitApplied_ = false;
	damageDebugState_ = {};

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
	attackHitApplied_ = false;

	// フェーズリセット
	phase_ = Phase::None;
	phaseTimer_ = 0.0f;
	totalTimer_ = 0.0f;

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

	// 攻撃判定時間内だけヒット判定を試み、命中後はattackHitApplied_で多段ヒットを防ぐ。
	if (!attackHitApplied_ && IsAttackWindowActive())
	{
		TryHitPlayer();
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

/// ---------------------------------------------------------------
///							　描画処理
/// ---------------------------------------------------------------
void BossPunchAttack::Draw()
{
	DrawAttackRangeDebug();
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
///					ヒット判定を一度だけ発生させる
/// ---------------------------------------------------------------
void BossPunchAttack::TryHitPlayer()
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
		OutputDebugStringA("[BossPunchAttack] Melee hit success.\n");
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

K4E::Vector3 BossPunchAttack::CalculateAttackCenter() const
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

bool BossPunchAttack::IsAttackWindowActive() const
{
	return isActive_
		&& totalTimer_ >= attackSettings_.attackActiveStartTime
		&& totalTimer_ <= attackSettings_.attackActiveEndTime;
}

void BossPunchAttack::DrawAttackRangeDebug()
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
///							有効距離内か
/// ---------------------------------------------------------------
bool BossPunchAttack::IsTargetInValidRange() const
{
	// オーナーがいないときは距離判定できないので無効
	if (owner_ == nullptr) return false;

	// ターゲットまでの距離を取得して、有効距離内か判定
	const float distance = owner_->GetDistanceToTargetXZ();
	return (distance >= minRange_ && distance <= attackSettings_.attackDistance + attackSettings_.attackRadius + targetRadius_);
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