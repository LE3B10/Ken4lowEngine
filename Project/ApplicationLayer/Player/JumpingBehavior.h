#pragma once
#include "PlayerBehavior.h"

/// -------------------------------------------------------------
///				　	ジャンプ状態の振る舞い
/// -------------------------------------------------------------
class JumpingBehavior : public PlayerBehavior
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(Player* player) override;

	// 更新処理
	void Update(Player* player) override;

	// 描画処理
	void Draw(Player* player) override;

private: /// ---------- メンバ変数 ---------- ///

	float jumpVelocity_ = 0.1f; // ジャンプの初速
	bool isFalling_ = false; // 落下中かどうか
	float gravity_; // 重力加速度
	float groundY_; // 地面のY座標
};

