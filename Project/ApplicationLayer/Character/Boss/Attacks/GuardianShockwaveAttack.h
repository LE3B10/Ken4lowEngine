#pragma once
#include "IBossAttack.h"

#include <cstdint>

/// ---------- 前方宣言 ---------- ///
class BossBase;

/// ---------------------------------------------------------------
///                 Guardian専用の前方衝撃波攻撃
/// ---------------------------------------------------------------
class GuardianShockwaveAttack : public IBossAttack
{
public: /// ---------- 列挙型 ---------- ///

	/// ---------- 攻撃内部フェーズ ---------- ///
	enum class Phase
	{
		None,       // 無効
		Windup,     // 予備動作
		Active,     // 判定発生
		Recovery    // 後隙
	};

public: /// ---------- 基本構造 ---------- ///

	~GuardianShockwaveAttack() override = default;

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
	const char* GetName() const override { return "GuardianShockwave"; }

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
	/// フェーズ内時間
	/// </summary>
	float GetPhaseTimer() const { return phaseTimer_; }

	/// <summary>
	/// すでにヒット済みか
	/// </summary>
	bool HasHit() const { return hasHit_; }

	/// <summary>
	/// 衝撃波の実リーチを取得
	/// </summary>
	float GetShockwaveRange() const { return shockwaveRange_; }

	/// <summary>
	/// 衝撃波の全角度を取得
	/// </summary>
	float GetShockwaveAngleDeg() const { return shockwaveAngleDeg_; }

	/// <summary>
	/// ダメージを取得
	/// </summary>
	float GetDamage() const { return damage_; }

public: /// ---------- パラメータ反映 ---------- ///

	/// <summary>
	/// 攻撃開始距離を設定
	/// </summary>
	void SetValidRange(float minRange, float maxRange);

	/// <summary>
	/// 衝撃波判定パラメータを設定
	/// </summary>
	void SetShockwaveParameters(float range, float angleDeg, float damage);

	/// <summary>
	/// 衝撃波の時間パラメータを設定
	/// </summary>
	void SetTimingParameters(float startupSec, float activeSec, float recoverySec, float cooldownSec);

	/// <summary>
	/// ヒット時GPUパーティクル調整値を設定
	/// </summary>
	void SetImpactParticleParameters(uint32_t spawnCount, float spawnRadius, float lifetimeScale, float initialSpeedScale);

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
	/// 攻撃開始時点のプレイヤー方向をワールド前方として固定する
	/// </summary>
	void LockShockwaveDirection();

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
	bool hasTelegraphEffect_ = false; // 予備動作の予兆エフェクトを一度だけ出したか

	Phase phase_ = Phase::None; // 現在フェーズ
	float phaseTimer_ = 0.0f;   // フェーズ内経過時間
	float totalTimer_ = 0.0f;   // 攻撃開始からの合計時間

	K4E::Vector3 lockedOrigin_{};                 // 攻撃開始時のワールド原点
	K4E::Vector3 lockedForward_{ 0.0f, 0.0f, 1.0f }; // 攻撃中に反転させない固定前方
	bool hasLockedDirection_ = false;             // 固定方向を取得済みか

private: /// ---------- 距離条件 ---------- ///

	float minRange_ = 3.0f;     // 近すぎる場合はパンチ系を優先する
	float maxRange_ = 10.0f;    // 衝撃波を開始できる中距離上限

private: /// ---------- フェーズ時間 ---------- ///

	float startupTime_ = 0.80f;  // 予備動作
	float activeTime_ = 0.25f;   // 判定時間
	float recoveryTime_ = 1.00f; // 後隙

private: /// ---------- ヒット判定 ---------- ///

	float damage_ = 15.0f;             // 衝撃波ダメージ
	float shockwaveRange_ = 10.0f;     // ボス正面方向へ届く衝撃波リーチ
	float shockwaveAngleDeg_ = 70.0f;  // 正面から左右へ広がる衝撃波の全角度
	uint32_t particleSpawnCount_ = 56;   // ヒット時GPUパーティクル数
	float particleSpawnRadius_ = 0.8f;  // ヒット時GPUパーティクル発生半径
	float particleLifetimeScale_ = 1.0f; // ヒット時GPUパーティクル寿命倍率
	float particleInitialSpeedScale_ = 1.0f; // ヒット時GPUパーティクル初速倍率

	float targetRadius_ = 0.65f;       // 仮のプレイヤー半径

private: /// ---------- クールダウン ---------- ///

	float cooldownSec_ = 6.0f;
	float cooldownRemaining_ = 0.0f;

private: /// ---------- 優先度 ---------- ///

	int priority_ = 70;
};
