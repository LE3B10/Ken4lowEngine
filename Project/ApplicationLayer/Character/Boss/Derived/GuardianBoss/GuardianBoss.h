#pragma once
#include "BaseTypes/HumanoidBossBase.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

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
	int GetBulletHitCount() const { return runtimeState_.bulletHitCount; }
	float GetLastReceivedDamage() const { return runtimeState_.lastReceivedDamage; }
	int GetBossAttackHitCount() const { return runtimeState_.bossAttackHitCount; }
	float GetLastPlayerDamage() const { return runtimeState_.lastPlayerDamage; }

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

	/// <summary>
	/// Guardian専用の見た目パラメータをParameterManagerから取得する
	/// </summary>
	void ApplyVisualParameters();

protected: /// ---------- Guardian固有パラメータ ---------- ///

	struct MovementTuning
	{
		float moveSpeed = 2.0f;          // 接近速度
		float rotateSpeed = 4.0f;        // 旋回速度
		float moveStartDistance = 4.8f;  // この距離より離れたら移動開始
		float moveStopDistance = 4.8f;   // この距離より近づいたら移動停止
	};

	struct AttackHitTuning
	{
		float attackRange = 5.75f;       // この距離以下で攻撃開始
		float hitRange = 6.0f;           // 実際の攻撃判定がボス正面へ届く距離
		float hitRadius = 2.0f;          // 実際の攻撃判定の太さ
		float forwardOffset = 3.0f;      // 実際の攻撃判定をボス正面へずらす距離
		float hitAngleDeg = 90.0f;       // 実際の攻撃判定を正面から左右へ広げる全角度
		float closeRange = 4.0f;         // 近距離攻撃帯
		float middleRange = 10.0f;       // 中距離攻撃帯
		float farRange = 20.0f;          // 遠距離検知帯
	};

	struct ShockwaveTuning
	{
		float range = 10.0f;             // 衝撃波の実ヒット判定リーチ
		float angleDeg = 70.0f;          // 衝撃波の前方扇形全角度
		float damage = 15.0f;            // 衝撃波ダメージ
		float cooldown = 6.0f;           // 衝撃波専用クールタイム
		float startupSec = 0.8f;         // 衝撃波予備動作
		float activeSec = 0.25f;         // 衝撃波判定時間
		float recoverySec = 1.0f;        // 衝撃波後隙
		float startRange = 10.0f;        // 衝撃波の攻撃開始上限（実リーチとは別にAI開始条件へ使う）
	};

	struct ChargeTuning
	{
		float speed = 18.0f;             // 突進速度
		float distance = 12.0f;          // 突進距離
		float damage = 20.0f;            // 突進ダメージ
		float startupSec = 0.6f;         // 突進予備動作
		float recoverySec = 1.0f;        // 突進後隙
		float cooldown = 8.0f;           // 突進クールタイム
	};

	struct ParticleTuning
	{
		uint32_t spawnCount = 48;         // 攻撃ヒット時GPUパーティクル数
		float spawnRadius = 0.5f;         // 攻撃ヒット時GPUパーティクル発生半径
		float lifetime = 1.0f;            // 攻撃ヒット時GPUパーティクル寿命倍率
		float initialSpeed = 1.0f;        // 攻撃ヒット時GPUパーティクル初速倍率
	};

	struct AnimationTuning
	{
		float attackDuration = 0.85f;     // 攻撃アニメ全体時間
		float attackCooldown = 1.20f;     // 攻撃後クールダウン
		float staggerDuration = 0.30f;    // ひるみ時間
		float walkSpeed = 6.0f;           // 歩行アニメ速度
		float walkAmplitude = 0.55f;      // 歩行アニメ振幅
	};

	struct VisualTuning
	{
		std::string bodyModelPath = "Characters/body.gltf";
		std::string headModelPath = "Characters/head.gltf";
		std::string leftArmModelPath = "Characters/left_arm.gltf";
		std::string rightArmModelPath = "Characters/right_arm.gltf";
		std::string leftLegModelPath = "Characters/left_leg.gltf";
		std::string rightLegModelPath = "Characters/right_leg.gltf";
		std::string skinPath = "Characters/zombie.dds";
	};

	struct RuntimeState
	{
		float stateTimer = 0.0f;          // 状態滞在時間
		float attackCooldownTimer = 0.0f; // クールダウン残り
		bool hasAppliedAttackHit = false;

		int receivedHitCount = 0;
		int bulletHitCount = 0;
		int bossAttackHitCount = 0;
		float lastReceivedDamage = 0.0f;
		float lastPlayerDamage = 0.0f;
	};

	struct AttackSelectState
	{
		std::string lastSelectedAttack = "None"; // HeavyPunch 連打抑制にも使う
		float heavyPunchReuseDelay = 1.0f;       // HeavyPunch を再使用できるまでの待ち時間
		float heavyPunchReuseTimer = 0.0f;       // HeavyPunch 連打防止タイマー
		bool useManualAttackDebug = false;       // true の間は自動攻撃選択を止める
		int manualAttackIndex = 0;               // 0: Punch / 1: HeavyPunch / 2: GuardianShockwave
	};

	MovementTuning movementTuning_;
	AttackHitTuning attackHitTuning_;
	ShockwaveTuning shockwaveTuning_;
	ChargeTuning chargeTuning_;
	ParticleTuning particleTuning_;
	AnimationTuning animationTuning_;
	VisualTuning visualTuning_;
	RuntimeState runtimeState_;
	AttackSelectState attackSelectState_;

protected: /// ---------- モデルや体格差分 ---------- ///

	K4E::Vector3 GetInitialBodyScale() const override { return { 1.0f, 1.0f, 1.0f }; }
	K4E::Vector3 GetHeadScale() const override { return { 1.0f, 1.0f, 1.0f }; }
	K4E::Vector3 GetArmScale() const override { return { 1.0f, 1.0f, 1.0f }; }
	K4E::Vector3 GetLegScale() const override { return { 1.0f, 1.0f, 1.0f }; }

protected: /// ---------- Guardian 専用モデル ---------- ///

	std::string GetBodyModelPath() const override;
	std::string GetHeadModelPath() const override;
	std::string GetLeftArmModelPath() const override;
	std::string GetRightArmModelPath() const override;
	std::string GetLeftLegModelPath() const override;
	std::string GetRightLegModelPath() const override;

	/// <summary>
	/// Guardian 用スキン
	/// </summary>
	virtual std::string GetGuardianSkinPath() const;
};