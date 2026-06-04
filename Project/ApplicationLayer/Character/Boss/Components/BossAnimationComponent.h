#pragma once
#include "IBossAttackAnimation.h"

#include <memory>
#include <vector>

/// ---------- 前方宣言 ---------- ///
class BossBase;
class IBossAttack;

/// -------------------------------------------------------------
///				ボス用アニメーションコンポーネント
/// -------------------------------------------------------------
class BossAnimationComponent
{
private: /// ---------- 内部ポーズ構造 ---------- ///

	/// ---------- ポーズ構造 ---------- ///
	struct BossPose
	{
		float bodyPitch = 0.0f;	  // 前後の傾き
		float bodyRoll = 0.0f;	  // 左右の傾き

		float headYaw = 0.0f;	  // 左右の回転
		float headPitch = 0.0f;	  // 前後の傾き

		float leftArmX = -0.05f;  // 腕の前後の傾き。歩行アニメで少し前に出すために初期値は -0.05f
		float rightArmX = -0.05f; // 腕の前後の傾き。歩行アニメで少し前に出すために初期値は -0.05f
		float leftArmZ = 0.0f;	  // 腕の内外の回転
		float rightArmZ = 0.0f;   // 腕の内外の回転

		float leftLegX = 0.0f;	  // 脚の前後の傾き
		float rightLegX = 0.0f;   // 脚の前後の傾き
	};

public: /// ---------- ライフサイクル ---------- ///

	/// <summary>
	/// 初期化
	/// owner はアニメ対象のボス本体
	/// </summary>
	void Initialize(BossBase* owner);

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// 毎フレーム更新
	/// boss の状態を見てアニメを切り替える
	/// </summary>
	void Update(BossBase& boss, float deltaTime);

public: /// ---------- 外部から使う制御 ---------- ///

	/// <summary>
	/// 全部位をニュートラル姿勢へ戻す
	/// </summary>
	void ResetAllPose(float blendRate);

	/// <summary>
	/// 歩行アニメ速度
	/// </summary>
	void SetWalkSpeed(float speed) { walkAnimSpeed_ = speed; }

	/// <summary>
	/// 歩行アニメの振り幅
	/// </summary>
	void SetWalkAmplitude(float amplitude) { walkSwingAmplitude_ = amplitude; }

	/// <summary>
	/// 攻撃アニメ全体時間
	/// </summary>
	void SetAttackDuration(float duration) { attackDuration_ = duration; }

	/// <summary>
	/// 攻撃アニメ時間をリセット
	/// Attack突入時に呼ぶ
	/// </summary>
	void ResetAttackTimer();

	/// <summary>
	/// 歩行アニメ時間をリセット
	/// </summary>
	void ResetWalkTimer();

	/// <summary>
	/// 現在の歩行アニメ時間
	/// </summary>
	float GetWalkTime() const { return walkAnimTime_; }

	/// <summary>
	/// 現在の攻撃アニメ時間
	/// </summary>
	float GetAttackTime() const { return attackAnimTime_; }

	/// <summary>
	/// 呼吸時間を取得
	/// </summary>
	float GetBreathTime() const { return breathTime_; }

private: /// ---------- 状態別更新 ---------- ///

	// アイドル状態のアニメ更新
	void UpdateIdle(BossBase& boss, float deltaTime);

	// 移動状態のアニメ更新
	void UpdateMove(BossBase& boss, float deltaTime);

	// 攻撃状態のアニメ更新
	void UpdateAttack(BossBase& boss, float deltaTime);

	// ダメージを受けたときの硬直アニメ更新
	void UpdateStagger(BossBase& boss, float deltaTime);

	// 死亡状態のアニメ更新
	void UpdateDead(BossBase& boss, float deltaTime);

private: /// ---------- 実アニメ処理 ---------- ///

	/// <summary>
	/// ボスのアイドルアニメーションを更新
	/// </summary>
	void UpdateIdleAnimation(BossBase& boss, float deltaTime);

	/// <summary>
	/// 歩行モーション
	/// 腕脚を交互に振る
	/// </summary>
	void UpdateWalkAnimation(BossBase& boss, float deltaTime);

	/// <summary>
	/// 攻撃モーション
	/// 攻撃種別ごとのポーズ構築関数を呼び分ける
	/// </summary>
	void UpdateAttackAnimation(BossBase& boss, float deltaTime);

private: /// ---------- Attack整理用 ---------- ///

	/// <summary>
	/// 現在攻撃を取得
	/// AttackComponent が無ければ nullptr
	/// </summary>
	IBossAttack* GetCurrentAttack(const BossBase& boss) const;

	/// <summary>
	/// 攻撃アニメクラスを登録
	/// </summary>
	void RegisterAttackAnimation(std::unique_ptr<IBossAttackAnimation> attackAnimation);

public: /// --------- ポーズ構築 / 適用 ---------- ///

	/// <summary>
	/// 攻撃未判定時の基本ポーズ
	/// </summary>
	BossPose BuildDefaultAttackPose() const;

	/// <summary>
	/// Punch の目標ポーズを構築
	/// </summary>
	BossPose BuildPunchPose() const;

	/// <summary>
	/// HeavyPunch の目標ポーズを構築
	/// </summary>
	BossPose BuildHeavyPunchPose() const;

	/// <summary>Shockwave の溜め・叩きつけポーズを構築</summary>
	BossPose BuildShockwavePose() const;

	/// <summary>ChargeAttack の溜め・突進ポーズを構築</summary>
	BossPose BuildChargePose() const;

	/// <summary>
	/// 作った目標ポーズを各部位へ反映
	/// </summary>
	void ApplyPose(BossBase& boss, const BossPose& pose, float deltaTime);

private: /// ---------- 補助 ---------- ///

	/// <summary>
	/// 現在値を target へ滑らかに寄せる
	/// </summary>
	void Damp(float& value, float target, float speed, float deltaTime);

	/// <summary>
	/// 角度用の補間
	/// </summary>
	void DampAngle(float& value, float target, float speed, float deltaTime);

	/// <summary>
	/// 人型主要部位が揃っているか
	/// </summary>
	bool HasRequiredParts(const BossBase& boss) const;

private: /// ---------- 参照 ---------- ///

	// アニメ対象のボス本体。Pose の反映などで必要
	BossBase* owner_ = nullptr;

private: /// ---------- 攻撃アニメ一覧 ---------- ///

	/// <summary>
	/// Punch / HeavyPunch などの専用アニメクラス群
	/// </summary>
	std::vector<std::unique_ptr<IBossAttackAnimation>> attackAnimations_;

private: /// ---------- 時間 ---------- ///

	float walkAnimTime_ = 0.0f;	  // 歩行アニメの時間
	float attackAnimTime_ = 0.0f; // 攻撃アニメの時間
	float breathTime_ = 0.0f;	  // 呼吸アニメの時間

private: /// ---------- パラメータ ---------- ///

	// 歩行アニメの速度
	float walkAnimSpeed_ = 5.0f;
	float walkSwingAmplitude_ = 0.55f;
	float attackDuration_ = 0.85f;

	// Idle の呼吸強度
	float idleBreathHeight_ = 0.035f;
	float idleBreathPitch_ = 0.05f;
	float idleHeadSway_ = 0.03f;

	// 歩行時の体幹表現
	float moveBodyBobHeight_ = 0.06f;
	float moveBodyTwistYaw_ = 0.10f;
	float moveShoulderLean_ = 0.08f;

	// 補間速度
	float limbDampSpeed_ = 10.0f;
	float bodyDampSpeed_ = 8.0f;
	float headDampSpeed_ = 7.0f;
};