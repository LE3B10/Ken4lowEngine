#pragma once

/// ---------- 前方宣言 ---------- ///
class Player;


/// -------------------------------------------------------------
///				　	プレイヤーの振る舞い基底クラス
/// -------------------------------------------------------------
class PlayerBehavior
{
public: /// ---------- 純粋仮想関数 ---------- ///

	// デストラクタ
	virtual ~PlayerBehavior() = default;

	// プレイヤーの初期化
	virtual void Initialize(Player* player) = 0;

	// プレイヤーの更新
	virtual void Update(Player* player) = 0;

	// プレイヤーの描画
	virtual void Draw(Player* player) = 0;
};

