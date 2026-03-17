#pragma once
#include <Vector3.h>

namespace K4E = Ken4lowEngine;

class BossBase;

/// -------------------------------------------------------------
/// ボス用移動コンポーネント
///
/// 役割:
/// - ターゲット方向へ向く
/// - ターゲットへ近づく
/// - 一定距離まで来たら止まる
///
/// 方針:
/// - まずは BossBase の位置 / 向き / ターゲット座標だけ使う
/// - 複雑な回り込みやダッシュは後回し
/// - 「ボスが前進できる」状態を先に作る
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
	void Finalize() {}

public: /// ---------- 更新 ---------- ///

	/// <summary>
	/// 毎フレーム更新
	/// </summary>
	void Update(BossBase& boss, float deltaTime);

public: /// ---------- パラメータ設定 ---------- ///

	void SetMoveSpeed(float moveSpeed) { moveSpeed_ = moveSpeed; }
	void SetTurnSpeed(float turnSpeed) { turnSpeed_ = turnSpeed; }
	void SetStopDistance(float stopDistance) { stopDistance_ = stopDistance; }

public: /// ---------- 参照 ---------- ///

	float GetMoveSpeed() const { return moveSpeed_; }
	float GetTurnSpeed() const { return turnSpeed_; }
	float GetStopDistance() const { return stopDistance_; }

private: /// ---------- 内部処理 ---------- ///

	/// <summary>
	/// ターゲット方向へ向く
	/// </summary>
	void FaceToTarget(BossBase& boss, float deltaTime);

	/// <summary>
	/// ターゲットへ近づく
	/// </summary>
	void MoveTowardsTarget(BossBase& boss, float deltaTime);

private: /// ---------- パラメータ ---------- ///

	float moveSpeed_ = 2.0f;
	float turnSpeed_ = 4.0f;
	float stopDistance_ = 3.0f;
};