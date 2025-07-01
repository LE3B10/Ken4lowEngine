#pragma once
#include "Sprite.h"

#include <algorithm>
#include <memory>

/// ---------- 前方宣言 ---------- ///
class Weapon;


/// -------------------------------------------------------------
///				　		リロード円のクラス
/// -------------------------------------------------------------
class ReloadCircle
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(const std::string& texturePath);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

public: /// ---------- ゲッタ ---------- ///

	// 
	bool IsVisible() const { return isVisible_; }

public: /// ---------- セッタ ---------- ///

	// 可視化設定
	void SetVisible(bool visible) { isVisible_ = visible; }

	// 進捗設定
	void SetProgress(float progress);

	// 武器の登録（HUDManager側などから一度だけ呼ぶ）
	void SetWeapon(Weapon* weapon) { weapon_ = weapon; }

private: /// ---------- メンバ変数 ---------- ///

	Weapon* weapon_ = nullptr; // 武器へのポインタ

	std::unique_ptr<Sprite> sprite_;
	float progress_ = 0.0f;
	bool isVisible_ = false;

};

