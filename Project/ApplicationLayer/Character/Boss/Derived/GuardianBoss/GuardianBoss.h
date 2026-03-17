#pragma once
#include "BaseTypes/HumanoidBossBase.h"

/// -------------------------------------------------------------
/// GuardianBoss
///
/// 最初の人型近接ボス
///
/// 役割:
/// - HumanoidBossBase の見た目土台を利用
/// - 個別の数値設定
/// - 状態遷移
/// - 接近
/// - 被弾時のひるみ
///
/// ポイント:
/// - 歩行 / 攻撃の見た目アニメは BossAnimationComponent に任せる
/// - このクラスでは「いつ動くか」「いつ殴るか」を決める
/// - 状態変更は SetState 直書きではなく StateMachine 経由に統一する
/// -------------------------------------------------------------
class GuardianBoss : public HumanoidBossBase
{
public:
	virtual ~GuardianBoss() = default;

public: /// ---------- Boss固有初期化 ---------- ///

	void SetupBoss() override;
	void OnDamaged(float damage) override;
	void OnDead() override;
	void OnCollision(K4E::Collider* other) override;
	void DrawImGui() override;

protected: /// ---------- BossBase override ---------- ///

	void UpdateState(float deltaTime) override;
	void UpdateMovement(float deltaTime) override;
	void UpdateAttack(float deltaTime) override;
	void CheckDeath() override;

	void SetupAttacks() override;
	void SetupPhaseData() override;
	void SetupWeakPoints() override;

protected: /// ---------- Guardian専用補助 ---------- ///

	/// <summary>
	/// ターゲットの方向へ回転する
	/// </summary>
	void FaceTarget(float deltaTime);

	/// <summary>
	/// ターゲットまでのXZ距離
	/// </summary>
	float GetDistanceToTargetXZ() const;

	/// <summary>
	/// 状態変更ヘルパー
	/// 必ず StateMachine 経由で状態を切り替える
	/// </summary>
	void ChangeBossState(BossState newState);

	/// <summary>
	/// 攻撃開始時の共通処理
	/// </summary>
	void BeginAttackState();

	/// <summary>
	/// Move 開始時の共通処理
	/// </summary>
	void BeginMoveState();

	/// <summary>
	/// Idle 開始時の共通処理
	/// </summary>
	void BeginIdleState();

	/// <summary>
	/// Stagger 開始時の共通処理
	/// </summary>
	void BeginStaggerState();

	/// <summary>
	/// 攻撃ヒットタイミング
	/// 後で本物の近接判定に差し替える
	/// </summary>
	void TryAttackHit();

protected: /// ---------- Guardian固有パラメータ ---------- ///

	float moveSpeed_ = 2.0f;          // 接近速度
	float rotateSpeed_ = 4.0f;        // 旋回速度

	float attackRange_ = 5.75f;       // この距離以下で攻撃開始
	float moveStartDistance_ = 4.5f;  // この距離より離れたら移動開始

	float attackDuration_ = 0.85f;    // 攻撃アニメ全体時間
	float attackCooldown_ = 1.20f;    // 攻撃後クールダウン
	float staggerDuration_ = 0.30f;   // ひるみ時間

	float stateTimer_ = 0.0f;         // 状態滞在時間
	float attackCooldownTimer_ = 0.0f;// クールダウン残り

	bool hasAppliedAttackHit_ = false;

protected: /// ---------- モデルや体格差分 ---------- ///

	K4E::Vector3 GetInitialBodyScale() const override { return { 1.0f, 1.0f, 1.0f }; }
	K4E::Vector3 GetHeadScale() const override { return { 1.0f, 1.0f, 1.0f }; }
	K4E::Vector3 GetArmScale() const override { return { 1.0f, 1.0f, 1.0f }; }
	K4E::Vector3 GetLegScale() const override { return { 1.0f, 1.0f, 1.0f }; }
};