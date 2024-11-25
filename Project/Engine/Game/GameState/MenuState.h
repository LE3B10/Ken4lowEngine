#pragma once
#include "IGameState.h"

#include <Windows.h>

/// -------------------------------------------------------------
///				ゲームメニュー画面を表すクラス
/// -------------------------------------------------------------
class MenuState : public IGameState
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize() override;

	// 更新処理
	void Update() override;

	// 描画処理
	void Draw() override;

	// 終了処理
	void Exit() override;

};

