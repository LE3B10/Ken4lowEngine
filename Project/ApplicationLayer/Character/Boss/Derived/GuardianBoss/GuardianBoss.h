#pragma once
#include "BaseTypes/HumanoidBossBase.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <AABB.h>

/// ----------------------------------------------------------------
///						ガーディアンボス
///
/// GamePlayWorldのクリスタル破壊後に出現する人型ボス。
/// HumanoidBossBaseの部位構成を利用し、Guardian専用の攻撃選択、近接判定、
/// 衝撃波/突進/ひるみ/デバッグ統計を管理する。
/// ----------------------------------------------------------------
class GuardianBoss : public HumanoidBossBase
{
public:

	GuardianBoss() = default;
	~GuardianBoss() override;

public: /// ---------- Boss固有初期化 ---------- ///

	// Guardian専用の初期パラメータ、モデル、攻撃、Collider設定を構築する。
	void SetupBoss() override;
	// 攻撃やエフェクトなどGuardianが保持する派生リソースを解放する。
	void Finalize() override;
	// ParameterManager上のGuardian調整値を実行中インスタンスへ反映する。
	void ApplyParameters() override;
	// 近接/ギミックなど銃弾以外の被ダメージ統計を更新し、ボス共通処理へ渡す。
	void OnDamaged(float damage) override;
	// プレイヤー銃弾ヒット数と最後の被ダメージを記録してからHPへ反映する。
	void OnBulletDamaged(float damage) override;
	// 死亡状態へ遷移し、World側のクリア進行が検知できる状態にする。
	void OnDead() override;
	// プレイヤー弾・近接などCollider種別に応じてGuardianへのダメージ処理へ振り分ける。
	void OnCollision(K4E::Collider* other) override;
	void DrawImGui() override;
	int GetMeleeHitCount() const;
	int GetBulletHitCount() const { return runtimeState_.bulletHitCount; }
	float GetLastReceivedDamage() const { return runtimeState_.lastReceivedDamage; }
	int GetBossAttackHitCount() const { return runtimeState_.bossAttackHitCount; }
	float GetLastPlayerDamage() const { return runtimeState_.lastPlayerDamage; }
	bool ConsumePhaseTransitionPresentation(BossPhase& outPhase);

protected: /// ---------- BossBase override ---------- ///

	void UpdateState(float deltaTime) override;
	void UpdatePhase(float deltaTime) override;
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
	/// HP割合に応じたフェーズ移行を開始し、強化を伝える短い停止を入れる
	/// </summary>
	void BeginPhaseTransition(BossPhase newPhase);

	float GetCurrentAttackCooldownScale() const;
	float GetCurrentChargeSpeedScale() const;
	float GetCurrentChargeCooldownScale() const;
	float GetCurrentWaveCooldownScale() const;
	float GetCurrentRecoveryScale() const;
	float GetCurrentChargeStartupScale() const;
	float GetCurrentWaveStartupScale() const;
	void ApplyPhaseVisual(float transitionRate);
	void UpdatePhaseAuraEffect(float deltaTime);
	void StartBossPhaseAura(BossPhase phase);
	void StopBossPhaseAura();
	void EmitBossPhaseAura(BossPhase phase);

	/// <summary>
	/// Guardian専用の見た目パラメータをParameterManagerから取得する
	/// </summary>
	void ApplyVisualParameters();

	/// <summary>
	/// プレイヤー方向を元に、障害物回避を含めた移動方向を返す
	/// </summary>
	K4E::Vector3 BuildNavigationMoveDirection(float deltaTime);

	/// <summary>
	/// 現在位置からターゲット方向へのXZ正規化ベクトルを返す
	/// </summary>
	K4E::Vector3 GetDirectDirectionToTargetXZ() const;

	/// <summary>
	/// 指定方向の先に障害物があるか調べる
	/// </summary>
	bool IsMoveDirectionBlocked(const K4E::Vector3& direction, float probeDistance) const;

	/// <summary>
	/// 候補方向の中から、障害物を避けつつターゲットへ一番近づく方向を選ぶ
	/// </summary>
	K4E::Vector3 SelectBestNavigationDirection(const K4E::Vector3& directDirection);

	/// <summary>
	/// 指定方向へ進んだ場合にターゲットとの距離がどれだけ縮むかを評価する
	/// </summary>
	float EvaluateMoveDirectionScore(const K4E::Vector3& direction) const;

	/// <summary>
	/// 現在の進行方向上で最初に邪魔になる障害物を探す
	/// </summary>
	bool FindBlockingObstacle(const K4E::Vector3& direction, float probeDistance, K4E::AABB& outObstacle) const;

	/// <summary>
	/// 障害物の左右端から、プレイヤーへ近づきやすいバイパス地点を作る
	/// </summary>
	bool TryBuildBypassTarget(const K4E::AABB& obstacle, const K4E::Vector3& directDirection, K4E::Vector3& outTarget);

	/// <summary>
	/// バイパス地点へ向かう方向を返す
	/// </summary>
	K4E::Vector3 GetDirectionToBypassTarget() const;

	/// <summary>
	/// バイパス状態を解除する
	/// </summary>
	void ClearBypassNavigation();

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
		float range = 18.0f;             // 衝撃波の実ヒット判定リーチ
		float angleDeg = 70.0f;          // 衝撃波の前方扇形全角度
		float damage = 15.0f;            // 衝撃波ダメージ
		float cooldown = 6.0f;           // 衝撃波専用クールタイム
		float startupSec = 1.2f;         // 衝撃波予備動作
		float activeSec = 0.25f;         // 衝撃波判定時間
		float recoverySec = 1.0f;        // 衝撃波後隙
		float startRange = 10.0f;        // 衝撃波の攻撃開始上限（実リーチとは別にAI開始条件へ使う）
	};

	struct PhaseTuning
	{
		float phase2HpRate = 0.70f;      // Phase2へ入るHP割合
		float phase3HpRate = 0.35f;      // Phase3へ入るHP割合
		float transitionSec = 0.85f;     // フェーズ移行時に短く止める時間
		float phase2AttackCooldownScale = 0.82f;
		float phase3AttackCooldownScale = 0.68f;
		float phase2ChargeSpeedScale = 1.10f;
		float phase3ChargeSpeedScale = 1.22f;
		float phase2ChargeCooldownScale = 0.82f;
		float phase3ChargeCooldownScale = 0.68f;
		float phase2WaveCooldownScale = 0.82f;
		float phase3WaveCooldownScale = 0.66f;
		float phase2RecoveryScale = 0.88f;
		float phase3RecoveryScale = 0.78f;
		float phase2ChargeStartupScale = 0.85f;
		float phase3ChargeStartupScale = 0.70f;
		float phase2WaveStartupScale = 0.83f;
		float phase3WaveStartupScale = 0.71f;
		float bossBulletDamageMultiplier = 0.60f; // 通常敵を変えず、ボスだけ耐久を調整するための倍率
	};

	struct ChargeTuning
	{
		float speed = 18.0f;             // 突進速度
		float distance = 12.0f;          // 突進距離
		float damage = 20.0f;            // 突進ダメージ
		float startupSec = 1.0f;         // 突進予備動作
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
		float phaseTransitionTimer = 0.0f;
		float phaseAuraTimer = 0.0f;
		bool phaseAuraActive = false;
		BossPhase currentAuraPhase = BossPhase::Phase1;
		K4E::Vector3 lastPhaseAuraPosition{};
		uint32_t lastPhaseAuraCount = 0;
		float lastPhaseAuraRadius = 0.0f;
		bool phasePresentationPending = false;
		BossPhase presentationPhase = BossPhase::Phase1;
	};

	struct AttackSelectState
	{
		std::string lastSelectedAttack = "None"; // HeavyPunch 連打抑制にも使う
		float heavyPunchReuseDelay = 1.0f;       // HeavyPunch を再使用できるまでの待ち時間
		float heavyPunchReuseTimer = 0.0f;       // HeavyPunch 連打防止タイマー
		bool useManualAttackDebug = false;       // true の間は自動攻撃選択を止める
		int manualAttackIndex = 0;               // 0: Punch / 1: HeavyPunch / 2: GuardianShockwave
	};

	struct NavigationTuning
	{
		bool enabled = true;                 // 簡易Navigationを使うか
		float probeDistance = 4.5f;          // 前方の障害物確認距離
		float sideProbeDistance = 3.5f;      // 左右回避方向の確認距離
		float candidateBlend = 0.80f;        // プレイヤー方向と横方向を混ぜる比率
		float bypassMargin = 2.2f;           // 障害物の角からどれだけ外側を通るか
		float bypassReachDistance = 1.25f;   // バイパス地点へ到達した扱いにする距離
		float bypassMaxDuration = 3.0f;      // バイパス移動を続けられる最大時間
	};

	struct NavigationRuntime
	{
		bool isBypassing = false;            // 障害物の端へ回り込み中か
		float bypassTimer = 0.0f;            // バイパス継続時間
		K4E::Vector3 bypassTarget{};         // 回り込み用の中継地点
		K4E::Vector3 selectedDirection{};    // 最終的に選ばれた移動方向
		int selectedCandidateIndex = -1;     // デバッグ用の候補番号
		int blockedCount = 0;                // 障害物に詰まった回数
	};

	// 移動関連
	MovementTuning movementTuning_;

	// 攻撃ヒット関連
	AttackHitTuning attackHitTuning_;

	// 攻撃調整パラメータ
	ShockwaveTuning shockwaveTuning_;

	// 突進攻撃調整パラメータ
	ChargeTuning chargeTuning_;

	// HP割合フェーズとボス専用ダメージ倍率
	PhaseTuning phaseTuning_;

	// パーティクル調整パラメータ
	ParticleTuning particleTuning_;

	// アニメーション調整パラメータ
	AnimationTuning animationTuning_;

	// 見た目調整パラメータ
	VisualTuning visualTuning_;

	// 実行時状態
	RuntimeState runtimeState_;

	// 攻撃選択状態
	AttackSelectState attackSelectState_;

	// 簡易Navigation関連
	NavigationTuning navigationTuning_;

	// 簡易Navigation実行時状態
	NavigationRuntime navigationRuntime_;

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
