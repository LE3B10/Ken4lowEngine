#pragma once
#include "IBossState.h"

#include <Windows.h>

/// -------------------------------------------------------------
///					ボスが行動不能になるクラス
/// -------------------------------------------------------------
class BossDownState : public IBossState
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

