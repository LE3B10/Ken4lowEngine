#pragma once
#include "Attacks/IBossAttack.h"
#include <Vector3.h>

namespace K4E = Ken4lowEngine;

class BossBase;

/// ---------------------------------------------------------------
/// 近接パンチ攻撃
///
/// 役割:
/// - 単発の近接攻撃を行う
/// - 予兆 → ヒット発生 → 終了 の流れを持つ
/// - 最初の基本攻撃として使う
///
/// 方針:
/// - まずはシンプルな時間管理ベースで作る
/// - 当たり判定は「前方距離 + 半径」で簡易実装
/// - 後で腕ボーン位置や部位座標に差し替えられるようにする
/// ---------------------------------------------------------------
class BossPunchAttack : public IBossAttack
{
public: /// ---------- 基本構造 ---------- ///

	~BossPunchAttack() override = default;

public: /// ---------- 初期化 ---------- ///

	/// <summary>
	/// 攻撃所有者を受け取って初期化
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
	/// 攻撃が終了したか
	/// </summary>
	bool IsFinished() const override { return isFinished_; }

	/// <summary>
	/// 現在実行中か
	/// </summary>
	bool IsActive() const override { return isActive_; }

public: /// ---------- クールダウン系 ---------- ///

	/// <summary>
	/// クールダウン更新
	/// </summary>
	void TickCooldown(float deltaTime) override;

	/// <summary>
	/// 残りクールダウン時間
	/// </summary>
	float GetCooldownRemaining() const override { return cooldownRemaining_; }

public: /// ---------- 参照用情報 ---------- ///

	/// <summary>
	/// 攻撃名
	/// </summary>
	const char* GetName() const override { return "Punch"; }

	/// <summary>
	/// 攻撃優先度
	/// </summary>
	int GetPriority() const override { return 10; }

	/// <summary>
	/// 有効距離の最小
	/// </summary>
	float GetMinRange() const override { return minRange_; }

	/// <summary>
	/// 有効距離の最大
	/// </summary>
	float GetMaxRange() const override { return maxRange_; }

public: /// ---------- 描画 ---------- ///

	void Draw() override;
	void DrawShadow() override {}
	void DrawImGui() override;

private: /// ---------- 内部処理 ---------- ///

	/// <summary>
	/// ヒット判定を一度だけ発生させる
	/// </summary>
	void TryHitPlayer();

	/// <summary>
	/// 攻撃開始条件に必要な距離内か
	/// </summary>
	bool IsTargetInValidRange() const;

private: /// ---------- 参照 ---------- ///

	BossBase* owner_ = nullptr;

private: /// ---------- 攻撃状態 ---------- ///

	bool isActive_ = false;
	bool isFinished_ = false;
	bool hasHit_ = false;

	float timer_ = 0.0f;

private: /// ---------- 時間設定 ---------- ///

	// 攻撃全体時間
	float totalDuration_ = 0.9f;

	// 予兆時間
	float windupTime_ = 0.35f;

	// ヒット発生タイミング
	float hitTime_ = 0.40f;

	// 攻撃終了タイミング
	float recoveryEndTime_ = 0.90f;

private: /// ---------- クールダウン ---------- ///

	float cooldownSec_ = 1.2f;
	float cooldownRemaining_ = 0.0f;

private: /// ---------- 攻撃性能 ---------- ///

	float damage_ = 20.0f;

	// 攻撃有効距離
	float minRange_ = 0.0f;
	float maxRange_ = 3.0f;

	// 前方ヒット半径
	float hitRadius_ = 1.4f;

	// ボス前方のどれくらい先で判定するか
	float hitForwardOffset_ = 1.8f;
};