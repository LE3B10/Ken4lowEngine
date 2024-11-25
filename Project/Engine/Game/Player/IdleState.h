#pragma once
#include "IPlayerState.h"

#include <Windows.h>

/// ---------- 前方宣言 ---------- ///
class Input;

/// -------------------------------------------------------------
///					プレイヤーの状態抽象クラス
/// -------------------------------------------------------------
class IdleState : public IPlayerState
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize() override;

	// 更新処理
	void Update() override; 

	// 入力処理
	void HandleInput(Input* input) override;

	// 描画処理
	void Draw() override;

	// 終了処理
	void Exit() override;

};

