#pragma once
#include "PlayerBehavior.h"

/// -------------------------------------------------------------
///				ジャンプ状態のプレイヤーの振る舞い
/// -------------------------------------------------------------
class JumpingBehavior : public PlayerBehavior
{
public: /// ---------- メンバ関数 ---------- ///

	// プレイヤーの初期化
	void Initialize(Player* player) override;

	// プレイヤーの更新
	void Update(Player* player) override;

	// プレイヤーの描画
	void Draw(Player* player) override;
};

