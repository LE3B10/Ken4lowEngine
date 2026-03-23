#pragma once
#include "Attacks/IBossAttack.h"
#include <Vector3.h>

namespace K4E = Ken4lowEngine;

class BossBase;

/// ---------------------------------------------------------------
/// 重攻撃パンチ
///
/// 役割:
/// - 通常 Punch より重い近接攻撃
/// - 発生は遅いが高威力
/// - 攻撃後の硬直も大きい
/// - Guardian の「避けるべき一撃」として使う
///
/// 方針:
/// - 基本構造は BossPunchAttack と揃える
/// - 後でノックバックやエフェクトを足しやすいようにする
/// ---------------------------------------------------------------
class BossHeavyPunchAttack : public IBossAttack
{
public: /// ---------- 列挙型 ---------- ///

	/// <summary>
	/// 攻撃の内部フェーズ
	/// </summary>
	enum class Phase
	{
		None,       // 未使用
		Windup,     // 溜め
		Hold,		// 溜め切って一瞬止める
		Active,     // 発生
		Recovery    // 攻撃後の硬直
	};

public: /// ---------- 基本構造 ---------- ///

	~BossHeavyPunchAttack() override = default;

public: /// ---------- 初期化 ---------- ///

	/// <summary>
	/// 所有者を受け取って初期化
	/// </summary>
	void Initialize(BossBase* owner) override;

public: /// ---------- 攻撃開始 / 更新 / 終了 ---------- ///

	/// <summary>
	/// 攻撃開始
	/// </summary>
	void Start() override;

	/// <summary>
	/// 攻撃更新
	/// </summary>
	void Update(float deltaTime) override;

	/// <summary>
	/// 攻撃終了
	/// </summary>
	void End() override;

public: /// ---------- 判定系 ---------- ///

	/// <summary>
	/// 今この攻撃を開始できるか
	/// </summary>
	bool CanStart() const override;

	/// <summary>
	/// 攻撃終了済みか
	/// </summary>
	bool IsFinished() const override { return isFinished_; }

	/// <summary>
	/// 実行中か
	/// </summary>
	bool IsActive() const override { return isActive_; }

public: /// ---------- クールダウン系 ---------- ///

	/// <summary>
	/// クールダウン更新
	/// </summary>
	void TickCooldown(float deltaTime) override;

	/// <summary>
	/// 残りクールダウン
	/// </summary>
	float GetCooldownRemaining() const override { return cooldownRemaining_; }

public: /// ---------- 参照用情報 ---------- ///

	/// <summary>
	/// 攻撃名
	/// </summary>
	const char* GetName() const override { return "HeavyPunch"; }

	/// <summary>
	/// 攻撃優先度
	/// Punch より高めにしておく
	/// </summary>
	int GetPriority() const override { return priority_; }

	/// <summary>
	/// 最小距離
	/// </summary>
	float GetMinRange() const override { return minRange_; }

	/// <summary>
	/// 最大距離
	/// </summary>
	float GetMaxRange() const override { return maxRange_; }

public: /// ---------- デバッグ参照 ---------- ///

	Phase GetPhase() const { return phase_; }
	float GetPhaseTimer() const { return phaseTimer_; }
	bool HasHit() const { return hasHit_; }

public: /// ---------- 描画 ---------- ///

	void Draw() override;
	void DrawShadow() override {}
	void DrawImGui() override;

private: /// ---------- 内部処理 ---------- ///

	/// <summary>
	/// 溜め更新
	/// </summary>
	void UpdateWindup(float deltaTime);

	/// <summary>
	/// 溜め切り保持更新
	/// 一瞬だけ予兆ポーズを見せる
	/// </summary>
	void UpdateHold(float deltaTime);

	/// <summary>
	/// 発生更新
	/// </summary>
	void UpdateActive(float deltaTime);

	/// <summary>
	/// 硬直更新
	/// </summary>
	void UpdateRecovery(float deltaTime);

	/// <summary>
	/// フェーズ切り替え
	/// </summary>
	void ChangePhase(Phase newPhase);

	/// <summary>
	/// プレイヤーにヒットするか試す
	/// 発生中に1回だけ呼ぶ
	/// </summary>
	void TryHitPlayer();

	/// <summary>
	/// 攻撃開始可能な距離か
	/// </summary>
	bool IsTargetInValidRange() const;

	/// <summary>
	/// デバッグ表示用フェーズ名
	/// </summary>
	const char* GetPhaseName() const;

private: /// ---------- 参照 ---------- ///

	BossBase* owner_ = nullptr;

private: /// ---------- 実行状態 ---------- ///

	bool isActive_ = false;
	bool isFinished_ = false;
	bool hasHit_ = false;

	Phase phase_ = Phase::None;
	float phaseTimer_ = 0.0f;
	float totalTimer_ = 0.0f;

private: /// ---------- 距離条件 ---------- ///

	// HeavyPunch は少しだけ踏み込みがある想定
	float minRange_ = 0.0f;
	float maxRange_ = 6.50f;

private: /// ---------- フェーズ時間 ---------- ///

	// 通常パンチより重く見せたいので溜めを長くする
	float windupTime_ = 0.55f;

	float holdTime_ = 0.12f;

	// 発生は短め
	float activeTime_ = 0.12f;

	// 硬直は長くして隙を作る
	float recoveryTime_ = 0.80f;

private: /// ---------- ヒット判定 ---------- ///

	float damage_ = 40.0f;              // 通常より高威力
	float hitRadius_ = 1.45f;           // 少し大きめ
	float hitForwardOffset_ = 1.70f;    // より前に届く
	float targetRadius_ = 0.65f;        // 仮プレイヤー半径

private: /// ---------- クールダウン ---------- ///

	float cooldownSec_ = 2.20f;
	float cooldownRemaining_ = 0.0f;

private: /// ---------- 優先度 ---------- ///

	int priority_ = 80;
};