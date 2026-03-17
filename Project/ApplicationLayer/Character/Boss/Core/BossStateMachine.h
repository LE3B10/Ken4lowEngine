#pragma once
#include "Core/BossTypes.h"

class BossBase;

/// -------------------------------------------------------------
/// ボス用ステートマシン
///
/// 役割:
/// - 現在状態 / 前回状態 を保持する
/// - 状態遷移の入口を一本化する
/// - 状態に入った瞬間 / 抜けた瞬間 / 状態中更新を管理する
///
/// 方針:
/// - 状態定義そのものは BossBase 側の BossState を使う
/// - まずは「状態の流れ」を安定させることを優先する
/// - 攻撃本体や移動本体は別コンポーネントに任せる
/// -------------------------------------------------------------
class BossStateMachine
{
public: /// ---------- 初期化 ---------- ///

	/// <summary>
	/// ステートマシンを初期化する
	/// </summary>
	/// <param name="initialState">開始状態</param>
	void Initialize(BossState initialState = BossState::Intro);

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize() {}

public: /// ---------- 更新 ---------- ///

	/// <summary>
	/// 毎フレーム更新
	/// </summary>
	void Update(BossBase& boss, float deltaTime);

public: /// ---------- 状態変更 ---------- ///

	/// <summary>
	/// 状態を変更する
	/// 同一状態への変更は無視する
	/// </summary>
	void ChangeState(BossBase& boss, BossState newState);

public: /// ---------- 参照 ---------- ///

	BossState GetCurrentState() const { return currentState_; }
	BossState GetPrevState() const { return prevState_; }

	/// <summary>
	/// 指定状態かどうか
	/// </summary>
	bool IsState(BossState state) const { return currentState_ == state; }

	/// <summary>
	/// 現在状態に入ってからの経過時間
	/// </summary>
	float GetStateTime() const { return stateTime_; }

private: /// ---------- 内部処理 ---------- ///

	/// <summary>
	/// 状態に入った瞬間の処理
	/// </summary>
	void OnEnter(BossBase& boss, BossState state);

	/// <summary>
	/// 状態を抜ける瞬間の処理
	/// </summary>
	void OnExit(BossBase& boss, BossState state);

	/// <summary>
	/// 状態中の更新
	/// </summary>
	void OnUpdate(BossBase& boss, BossState state, float deltaTime);

private: /// ---------- 内部状態 ---------- ///

	BossState currentState_ = BossState::Intro;
	BossState prevState_ = BossState::Intro;
	float stateTime_ = 0.0f;
};