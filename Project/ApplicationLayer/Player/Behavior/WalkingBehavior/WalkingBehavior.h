#pragma once
#include "PlayerBehavior.h"

/// -------------------------------------------------------------
///				歩行状態のプレイヤーの振る舞い
/// --------------------------------------------------------------
class WalkingBehavior : public PlayerBehavior
{
public: /// ---------- メンバ関数 ---------- ///

	// プレイヤーの初期化
	void Initialize(Player* player) override;

	// プレイヤーの更新
	void Update(Player* player) override;

	// プレイヤーの描画
	void Draw(Player* player) override;
};

