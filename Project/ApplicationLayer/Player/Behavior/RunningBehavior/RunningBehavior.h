#pragma once
#include "PlayerBehavior.h"

/// -------------------------------------------------------------
///				走行状態のプレイヤーの振る舞い
/// -------------------------------------------------------------
class RunningBehavior : public PlayerBehavior
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(Player* player) override;

	// 更新処理
	void Update(Player* player) override;

	// 描画処理
	void Draw(Player* player) override;
};

