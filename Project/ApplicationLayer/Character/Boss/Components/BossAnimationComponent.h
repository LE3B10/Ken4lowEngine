#pragma once
#include "Core/BossTypes.h"
#include "Animations/IBossAttackAnimation.h"

#include <memory>
#include <vector>

/// ---------- 前方宣言 ---------- ///
class BossBase;
class IBossAttack;

/// -------------------------------------------------------------
/// ボス用アニメーションコンポーネント
///
/// 役割:
/// - Boss の状態に応じて見た目のポーズを更新する
/// - 歩行アニメ / 攻撃アニメ / 待機姿勢への復帰を担当する
///
/// ポイント:
/// - まだ「本物のアニメーションクリップ再生」ではなく、
///   各部位の回転値を直接いじる簡易方式
/// - 後で AnimationClip / StateGraph に差し替えやすいよう
///   Boss 本体から分離しておく
/// -------------------------------------------------------------
class BossAnimationComponent
{
private: /// ---------- 内部ポーズ構造 ---------- ///

	/// <summary>
	/// 1フレーム分の目標ポーズ
	/// 目標値だけを持つ
	/// 実際のDamp適用は ApplyPose() で行う
	/// </summary>
	struct BossPose
	{
		float bodyPitch = 0.0f;
		float bodyRoll = 0.0f;

		float headYaw = 0.0f;
		float headPitch = 0.0f;

		float leftArmX = -0.05f;
		float rightArmX = -0.05f;
		float leftArmZ = 0.0f;
		float rightArmZ = 0.0f;

		float leftLegX = 0.0f;
		float rightLegX = 0.0f;
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

	void UpdateIdle(BossBase& boss, float deltaTime);
	void UpdateMove(BossBase& boss, float deltaTime);
	void UpdateAttack(BossBase& boss, float deltaTime);
	void UpdateStagger(BossBase& boss, float deltaTime);
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

	BossBase* owner_ = nullptr;

private: /// ---------- 攻撃アニメ一覧 ---------- ///

	/// <summary>
	/// Punch / HeavyPunch などの専用アニメクラス群
	/// </summary>
	std::vector<std::unique_ptr<IBossAttackAnimation>> attackAnimations_;

private: /// ---------- 時間 ---------- ///

	float walkAnimTime_ = 0.0f;
	float attackAnimTime_ = 0.0f;
	float breathTime_ = 0.0f;

private: /// ---------- パラメータ ---------- ///

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