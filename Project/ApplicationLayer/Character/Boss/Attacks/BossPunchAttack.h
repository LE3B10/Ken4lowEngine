#pragma once
#include "IBossAttack.h"

/// ---------- 前方宣言 ---------- ///
class BossBase;

/// ---------------------------------------------------------------
///						ボスの近接パンチ攻撃
/// ---------------------------------------------------------------
class BossPunchAttack : public IBossAttack
{
public: /// ---------- 列挙型 ---------- ///

	/// ---------- 攻撃内部フェーズ ---------- ///
	enum class Phase
	{
		None,       // 無効
		Windup,     // 予兆
		Active,     // 発生
		Recovery    // 回復 / 終了待ち
	};

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
	int GetPriority() const override { return priority_; }

	/// <summary>
	/// 有効距離の最小
	/// </summary>
	float GetMinRange() const override { return minRange_; }

	/// <summary>
	/// 有効距離の最大
	/// </summary>
	float GetMaxRange() const override { return maxRange_; }

public: /// ---------- デバッグ参照 ---------- ///

	/// <summary>
	/// 現在フェーズを取得
	/// </summary>
	Phase GetPhase() const { return phase_; }

	/// <summary>
	/// フェーズない時間
	/// </summary>
	float GetPhaseTimer() const { return phaseTimer_; }

	/// <summary>
	/// すでにヒット済みか
	/// </summary>
	bool HasHit() const { return hasHit_; }

public: /// ---------- 描画 ---------- ///

	void Draw() override;
	void DrawShadow() override {}
	void DrawImGui() override;

private: /// ---------- 内部処理 ---------- ///

	/// <summary>
	/// 3段階フェーズ更新
	/// </summary>
	void UpdateWindup(float deltaTime);
	void UpdateActive(float deltaTime);
	void UpdateRecovery(float deltaTime);

	/// <summary>
	/// フェーズ切り替え
	/// </summary>
	void ChangePhase(Phase newPhase);

	/// <summary>
	/// ヒット判定を一度だけ発生させる
	/// </summary>
	void TryHitPlayer();

	/// <summary>
	/// 攻撃開始条件に必要な距離内か
	/// </summary>
	bool IsTargetInValidRange() const;

	/// <summary>
	/// デバッグ用フェーズ名
	/// </summary>
	const char* GetPhaseName() const;

private: /// ---------- 参照 ---------- ///

	BossBase* owner_ = nullptr;

private: /// ---------- 実行状態 ---------- ///

	bool isActive_ = false;     // 実行中か
	bool isFinished_ = false;   // 今回の実行が終わったか
	bool hasHit_ = false;       // 今回すでにヒットを出したか

	Phase phase_ = Phase::None; // 現在フェーズ
	float phaseTimer_ = 0.0f;   // フェーズ内経過時間
	float totalTimer_ = 0.0f;   // 攻撃開始からの合計時間

private: /// ---------- 距離条件 ---------- ///

	float minRange_ = 0.0f;     // 最小有効距離
	float maxRange_ = 5.75f;    // 最大有効距離

private: /// ---------- フェーズ時間 ---------- ///

	// 溜め
	float windupTime_ = 0.30f;

	// 発生
	float activeTime_ = 0.12f;

	// 硬直
	float recoveryTime_ = 0.40f;

private: /// ---------- ヒット判定 ---------- ///

	float damage_ = 20.0f;              // 将来プレイヤーに与えるダメージ
	float hitRadius_ = 1.15f;           // パンチ球の半径
	float hitForwardOffset_ = 1.35f;    // 右腕根本から前方へずらす距離
	float targetRadius_ = 0.65f;        // 仮のプレイヤー半径

private: /// ---------- クールダウン ---------- ///

	float cooldownSec_ = 1.10f;
	float cooldownRemaining_ = 0.0f;

private: /// ---------- 優先度 ---------- ///

	int priority_ = 50;
};