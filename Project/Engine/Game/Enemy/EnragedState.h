#pragma once
#include "IBossState.h"

#include <Windows.h>

/// -------------------------------------------------------------
///						怒り状態を表すクラス
/// -------------------------------------------------------------
class EnragedState : public IBossState
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

