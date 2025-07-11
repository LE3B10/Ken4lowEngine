#pragma once

/// ---------- 前方宣言 ---------- ///
class Player;

/// -------------------------------------------------------------
///				　	プレイヤーの振る舞い基底クラス
/// -------------------------------------------------------------
class PlayerBehavior
{
public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	virtual ~PlayerBehavior() = default;

	// 初期化処理
	virtual void Initialize(Player* player) = 0;

	// 更新処理
	virtual void Update(Player* player) = 0;

	// 描画処理
	virtual void Draw(Player* player) = 0;
};

