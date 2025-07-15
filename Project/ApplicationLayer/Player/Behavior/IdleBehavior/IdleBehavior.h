#pragma once
#include "PlayerBehavior.h"

/// -------------------------------------------------------------
///				アイドル状態のプレイヤーの振る舞い
/// -------------------------------------------------------------
class IdleBehavior : public PlayerBehavior
{
public: /// ---------- メンバ関数 ---------- ///

	// プレイヤーの初期化
	void Initialize(Player* player) override;

	// プレイヤーの更新
	void Update(Player* player) override;

	// プレイヤーの描画
	void Draw(Player* player) override;
};

