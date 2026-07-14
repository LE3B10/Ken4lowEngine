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

	void SetMoveSpeed(float moveSpeed) { moveSpeed_ = moveSpeed; }
	float GetMoveSpeed() const { return moveSpeed_; }
	void SetTurnSpeed(float turnSpeed) { turnSpeed_ = turnSpeed; }
	float GetTurnSpeed() const { return turnSpeed_; }
	void SetStopDistance(float stopDistance) { stopDistance_ = stopDistance; }
	float GetStopDistance() const { return stopDistance_; }

	/// BossActorが所有する共通Movement Componentへの非所有参照を返す。
	Ken4lowEngine::CharacterMovementComponent* GetCharacterMovementComponent() { return characterMovement_; }
	const Ken4lowEngine::CharacterMovementComponent* GetCharacterMovementComponent() const { return characterMovement_; }

private: /// ---------- 内部処理 ---------- ///

	void FaceToTarget(BossBase& boss, float deltaTime) const;
	void MoveTowardsTarget(BossBase& boss, float deltaTime);

private: /// ---------- パラメータ ---------- ///

	float moveSpeed_ = 2.0f;
	float turnSpeed_ = 4.0f;
	float stopDistance_ = 3.0f;
	Ken4lowEngine::CharacterMovementComponent* characterMovement_ = nullptr; // 実体はBossのActor Componentが所有し、ここでは参照だけ保持する。
};
