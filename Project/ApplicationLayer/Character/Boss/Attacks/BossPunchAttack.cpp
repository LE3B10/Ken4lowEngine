#define NOMINMAX
#include "BossPunchAttack.h"
#include "BossBase.h"

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

/// ---------------------------------------------------------------
///							　描画処理
/// ---------------------------------------------------------------
void BossPunchAttack::Draw()
{
	// 例:
	// - Windup 中に拳の前に予兆球
	// - Active 中に当たり判定球を赤で表示
	// 今は未実装
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
	ImGui::Text("Damage            : %.2f", damage_);
#endif
}

/// ---------------------------------------------------------------
///					ヒット判定を一度だけ発生させる
/// ---------------------------------------------------------------
void BossPunchAttack::TryHitPlayer()
{
	if (owner_ == nullptr) return;

	// 攻撃の中心座標を計算する
	const K4E::Vector3 armRoot = owner_->GetRightArmRootWorldPosition();
	const float yaw = owner_->GetYaw();

	// Yaw から前方ベクトルを計算
	K4E::Vector3 forward
	{
		std::sin(yaw),
		0.0f,
		std::cos(yaw)
	};

	// 攻撃中心は腕の根元から前方に少しオフセットした位置
	K4E::Vector3 attackCenter = armRoot;
	attackCenter.x += forward.x * hitForwardOffset_;
	attackCenter.y += 0.25f;
	attackCenter.z += forward.z * hitForwardOffset_;

	// ターゲットの中心座標を取得
	const K4E::Vector3 targetCenter = owner_->GetTargetPosition();

	const float dx = targetCenter.x - attackCenter.x; // XZ 平面での距離を計算
	const float dy = targetCenter.y - attackCenter.y; // Y 軸の距離も考慮する場合はこれも使う
	const float dz = targetCenter.z - attackCenter.z; // XZ 平面での距離を計算

	// 攻撃中心とターゲット中心の距離の二乗を計算
	const float distanceSq = dx * dx + dy * dy + dz * dz;

	// 攻撃の当たり判定半径とターゲットの半径を足した値の二乗と比較してヒット判定
	const float sumRadius = hitRadius_ + targetRadius_;

	// 距離の二乗が半径の二乗以下ならヒットと判定
	if (distanceSq <= (sumRadius * sumRadius))
	{
#ifdef _DEBUG
		// ヒットしたときのデバッグログ
		OutputDebugStringA("[BossPunchAttack] Melee hit success.\n");
#endif

		// -------------------------------------------------------
		// TODO:
		// 将来的にはここでプレイヤーへ本当にダメージを与える
		//
		// 例:
		// if (Player* player = owner_->GetTargetPlayer())
		// {
		//     player->OnDamaged(damage_);
		// }
		// -------------------------------------------------------
	}
	else
	{
#ifdef _DEBUG
		// ヒットしなかったときのデバッグログ
		OutputDebugStringA("[BossPunchAttack] Melee hit miss.\n");
#endif
	}
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
	return (distance >= minRange_ && distance <= maxRange_);
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