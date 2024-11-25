#pragma once

/// ---------- 前方宣言 ---------- ///
class Input;

/// -------------------------------------------------------------
///					プレイヤーの状態抽象クラス
/// -------------------------------------------------------------
class IPlayerState
{
public: /// ---------- 純粋仮想関数 ---------- ///

	// デストラクタ
	virtual ~IPlayerState() = default;

	// プレイヤーの初期化処理
	virtual void Initialize() = 0;

	// プレイヤーの状態更新処理
	virtual void Update() = 0;

	// プレイヤーの入力処理
	virtual void HandleInput(Input* input) = 0;

	// プレイヤーの描画処理
	virtual void Draw() = 0;

	// 状態から抜ける際の処理
	virtual void Exit() = 0;

};

