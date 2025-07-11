#pragma once
#include "PlayerBehavior.h"

/// -------------------------------------------------------------
///				　	アイドル状態の振る舞い
/// -------------------------------------------------------------
class IdleBehavior : public PlayerBehavior
{
public: /// ---------- メンバ関数 ---------- ///
	
	// 初期化処理
	void Initialize(Player* player) override;

	// 更新処理
	void Update(Player* player) override;
	
	// 描画処理
	void Draw(Player* player) override;
};

