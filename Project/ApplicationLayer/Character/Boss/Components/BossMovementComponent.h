#pragma once

#include <Scene/Actor/Character/CharacterMovementComponent.h>

/// ---------- 前方宣言 ---------- ///
class BossBase;

/// -------------------------------------------------------------
///					ボス用移動コンポーネント
/// -------------------------------------------------------------
class BossMovementComponent
{
public: /// ---------- 初期化 ---------- ///

	/// <summary>
	/// 移動パラメータを初期化する
	/// </summary>
	void Initialize(float moveSpeed, float turnSpeed, float stopDistance);

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

public: /// ---------- 更新 ---------- ///

	/// <summary>
	/// 毎フレーム更新
	/// </summary>
	void Update(BossBase& boss, float deltaTime);

public: /// ---------- アクセッサ ---------- ///

	// 移動速度を設定
	void SetMoveSpeed(float moveSpeed) { moveSpeed_ = moveSpeed; }

	// 移動速度を取得
	float GetMoveSpeed() const { return moveSpeed_; }

	// 回転速度を設定
	void SetTurnSpeed(float turnSpeed) { turnSpeed_ = turnSpeed; }

	// 回転速度を取得
	float GetTurnSpeed() const { return turnSpeed_; }

	// ターゲットに近づくのを止める距離を設定
	void SetStopDistance(float stopDistance) { stopDistance_ = stopDistance; }

	// ターゲットに近づくのを止める距離を取得
	float GetStopDistance() const { return stopDistance_; }

	/// 旧Boss移動APIが委譲している共通Movement Componentを返す。
	Ken4lowEngine::CharacterMovementComponent* GetCharacterMovementComponent() { return &characterMovement_; }

	/// 旧Boss移動APIが委譲している共通Movement Componentを返すconst版。
	const Ken4lowEngine::CharacterMovementComponent* GetCharacterMovementComponent() const { return &characterMovement_; }

private: /// ---------- 内部処理 ---------- ///

	/// <summary>
	/// ターゲット方向へ向く
	/// </summary>
	void FaceToTarget(BossBase& boss, float deltaTime) const;

	/// <summary>
	/// ターゲットへ近づく
	/// </summary>
	void MoveTowardsTarget(BossBase& boss, float deltaTime);

private: /// ---------- パラメータ ---------- ///

	float moveSpeed_ = 2.0f;	// 移動速度
	float turnSpeed_ = 4.0f;	// 回転速度
	float stopDistance_ = 3.0f; // ターゲットに近づくのを止める距離
	Ken4lowEngine::CharacterMovementComponent characterMovement_{}; // 移行中はBoss専用判断から得た速度を共通Componentへ渡す。
};
