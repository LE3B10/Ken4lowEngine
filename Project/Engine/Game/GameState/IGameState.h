#pragma once

/// -------------------------------------------------------------
///					ステートインターフェース
/// -------------------------------------------------------------
class IGameState
{
public: /// ---------- 純粋仮想関数 ---------- ///

	// デストラクタ
	virtual ~IGameState() = default;

	// ゲーム初期化処理
	virtual void Initialize() = 0;

	// ゲームの状態更新処理
	virtual void Update() = 0;

	// 描画処理
	virtual void Draw() = 0;

	// 状態から抜ける際の処理
	virtual void Exit() = 0;
};

