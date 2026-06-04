#pragma once
#include "BaseTypes/HumanoidBossBase.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

/// ----------------------------------------------------------------
///						ガーディアンボス
/// ----------------------------------------------------------------
class GuardianBoss : public HumanoidBossBase
{
public:

	GuardianBoss() = default;
	~GuardianBoss() override;

public: /// ---------- Boss固有初期化 ---------- ///

	void SetupBoss() override;
	void Finalize() override;
	void ApplyParameters() override;
	void OnDamaged(float damage) override;
	void OnBulletDamaged(float damage) override;
	void OnDead() override;
	void OnCollision(K4E::Collider* other) override;
	void DrawImGui() override;
	int GetMeleeHitCount() const;
	int GetBulletHitCount() const { return bulletHitCount_; }
	float GetLastReceivedDamage() const { return lastReceivedDamage_; }
	int GetBossAttackHitCount() const { return bossAttackHitCount_; }
	float GetLastPlayerDamage() const { return lastPlayerDamage_; }

protected: /// ---------- BossBase override ---------- ///

	void UpdateState(float deltaTime) override;
	void UpdateMovement(float deltaTime) override;
	void UpdateAttack(float deltaTime) override;
	void CheckDeath() override;
	void OnTargetPlayerDamaged(float damage) override;

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

	/// <summary>
	/// 現在の状況で最適な攻撃を開始する
	/// 実際の攻撃候補選択は BossBrain に任せる
	/// </summary>
	bool TryStartBestAttack();

	/// <summary>
	/// 指定名の攻撃を安全に開始する
	/// </summary>
	bool StartAttackByNameSafe(const char* attackName);

	/// <summary>
	/// Guardian専用の攻撃判定パラメータを登録済み攻撃へ反映する
	/// </summary>
	void ApplyAttackHitParametersToAttacks();

protected: /// ---------- Guardian固有パラメータ ---------- ///

	float moveSpeed_ = 2.0f;          // 接近速度
	float rotateSpeed_ = 4.0f;        // 旋回速度

	float attackRange_ = 5.75f;       // この距離以下で攻撃開始
	float attackHitRange_ = 6.0f;     // 実際の攻撃判定がボス正面へ届く距離
	float attackHitRadius_ = 2.0f;    // 実際の攻撃判定の太さ
	float attackForwardOffset_ = 3.0f;// 実際の攻撃判定をボス正面へずらす距離
	float moveStartDistance_ = 4.8f;  // この距離より離れたら移動開始
	float moveStopDistance_ = 4.8f;   // この距離より近づいたら移動停止

	float attackDuration_ = 0.85f;    // 攻撃アニメ全体時間
	float attackCooldown_ = 1.20f;    // 攻撃後クールダウン
	float staggerDuration_ = 0.30f;   // ひるみ時間
	float animationWalkSpeed_ = 6.0f;   // 歩行アニメ速度
	float animationWalkAmplitude_ = 0.55f; // 歩行アニメ振幅

	float stateTimer_ = 0.0f;         // 状態滞在時間
	float attackCooldownTimer_ = 0.0f;// クールダウン残り

	bool hasAppliedAttackHit_ = false;

	int receivedHitCount_ = 0;
	int bulletHitCount_ = 0;
	int bossAttackHitCount_ = 0;
	float lastReceivedDamage_ = 0.0f;
	float lastPlayerDamage_ = 0.0f;

	/// <summary>
	/// 最後に選ばれた攻撃名
	/// HeavyPunch 連打抑制にも使う
	/// </summary>
	std::string lastSelectedAttack_ = "None";

	/// <summary>
	/// HeavyPunch を再使用できるまでの待ち時間
	/// </summary>
	float heavyPunchReuseDelay_ = 1.0f;

	/// <summary>
	/// HeavyPunch 連打防止タイマー
	/// </summary>
	float heavyPunchReuseTimer_ = 0.0f;

	bool useManualAttackDebug_ = false;   // true の間は自動攻撃選択を止める
	int manualAttackIndex_ = 0;           // 0: Punch / 1: HeavyPunch

protected: /// ---------- モデルや体格差分 ---------- ///

	K4E::Vector3 GetInitialBodyScale() const override { return { 1.0f, 1.0f, 1.0f }; }
	K4E::Vector3 GetHeadScale() const override { return { 1.0f, 1.0f, 1.0f }; }
	K4E::Vector3 GetArmScale() const override { return { 1.0f, 1.0f, 1.0f }; }
	K4E::Vector3 GetLegScale() const override { return { 1.0f, 1.0f, 1.0f }; }

protected: /// ---------- Guardian 専用モデル ---------- ///

	/*std::string GetBodyModelPath() const override { return "Boss/Guardian/body.gltf"; }
	std::string GetHeadModelPath() const override { return "Boss/Guardian/head.gltf"; }
	std::string GetLeftArmModelPath() const override { return "Boss/Guardian/left_arm.gltf"; }
	std::string GetRightArmModelPath() const override { return "Boss/Guardian/right_arm.gltf"; }
	std::string GetLeftLegModelPath() const override { return "Boss/Guardian/left_leg.gltf"; }
	std::string GetRightLegModelPath() const override { return "Boss/Guardian/right_leg.gltf"; }*/

	/// <summary>
	/// Guardian 用スキン
	/// </summary>
	virtual std::string GetGuardianSkinPath() const;
};