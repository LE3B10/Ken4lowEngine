#pragma once
#include <NumberSpriteDrawer.h>

#include <algorithm>

/// -------------------------------------------------------------
/// 				　		HUDマネージャー
/// -------------------------------------------------------------
class HUDManager
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

public: /// ---------- セッター ---------- ///

	// スコアの設定
	void SetScore(int score) { score_ = score; }

	// キル数の設定
	void SetKills(int kills) { kills_ = kills; }

	// 弾薬数の設定
	void SetAmmo(int current, int reserve) { ammoInClip_ = current; ammoReserve_ = reserve; }

	// HPの設定
	void SetHP(int current, int max) { hp_ = current; maxHP_ = max; }

private: /// ---------- メンバ関数 ---------- ///

	void DrawDebugHUD(); // デバッグ用HUDの描画

private: /// ---------- メンバ変数 ---------- ///

	std::unique_ptr<NumberSpriteDrawer> scoreDrawer_; // スコア表示用
	std::unique_ptr<NumberSpriteDrawer> killDrawer_;  // キル数表示用
	std::unique_ptr<NumberSpriteDrawer> ammoDrawer_;  // 弾薬数表示用
	std::unique_ptr<NumberSpriteDrawer> hpDrawer_;	  // HP表示用

	std::unique_ptr<Sprite> hpBarBase_; // グレー背景
	std::unique_ptr<Sprite> hpBarFill_; // 緑バー

	// スコア
	int score_ = 0;

	// キル数
	int kills_ = 0;

	// HP
	int hp_ = 100; // プレイヤーのHP
	int maxHP_ = 100;

	// 弾薬数
	int ammoInClip_ = 0;  // 現在の弾薬数
	int ammoReserve_ = 0; // 予備の弾薬数

	const std::string texturePath_ = "Resources/number.png"; // 数字のテクスチャパス
};

