#pragma once
#include "Core/BossTypes.h"

class BossBase;

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
public:
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

private: /// ---------- 状態別更新 ---------- ///

	void UpdateIdle(BossBase& boss, float deltaTime);
	void UpdateMove(BossBase& boss, float deltaTime);
	void UpdateAttack(BossBase& boss, float deltaTime);
	void UpdateStagger(BossBase& boss, float deltaTime);
	void UpdateDead(BossBase& boss, float deltaTime);

private: /// ---------- 実アニメ処理 ---------- ///

	/// <summary>
	/// 歩行モーション
	/// 腕脚を交互に振る
	/// </summary>
	void UpdateWalkAnimation(BossBase& boss, float deltaTime);

	/// <summary>
	/// 攻撃モーション
	/// 右腕を振り下ろす簡易モーション
	/// </summary>
	void UpdateAttackAnimation(BossBase& boss, float deltaTime);

private:
	BossBase* owner_ = nullptr;

	// 歩行アニメ時間
	float walkAnimTime_ = 0.0f;

	// 攻撃アニメ時間
	float attackAnimTime_ = 0.0f;

	// 歩行速度
	float walkAnimSpeed_ = 6.0f;

	// 歩行時の振り幅
	float walkSwingAmplitude_ = 0.55f;

	// 攻撃アニメ全体時間
	float attackDuration_ = 0.85f;
};