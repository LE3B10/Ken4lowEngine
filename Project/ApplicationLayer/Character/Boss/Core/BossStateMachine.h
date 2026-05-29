#pragma once
#include "BossTypes.h"

/// ---------- 前方宣言 ---------- ///
class BossBase;

/// -------------------------------------------------------------
///						ボス用ステートマシン
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

	// 現在状態を取得
	BossState GetCurrentState() const { return currentState_; }

	// 前回状態を取得
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

	BossState currentState_ = BossState::Intro; // 現在状態
	BossState prevState_ = BossState::Intro;	// 前回状態
	float stateTime_ = 0.0f;					// 現在状態に入ってからの経過時間
};