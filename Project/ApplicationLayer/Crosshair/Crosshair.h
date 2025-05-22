#pragma once
#include <Sprite.h>

#include <memory>
#include <string>


/// -------------------------------------------------------------
///				　		十字カーソルクラス
/// -------------------------------------------------------------
class Crosshair
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化（テクスチャロード）
	void Initialize(const std::string& texturePath = "Resources/crosshair-white.png");

	// 更新（将来のアニメーション用：今は空でOK）
	void Update();

	// 描画（中心に描画）
	void Draw();

	// 表示ON/OFF切替
	void SetVisible(bool visible) { isVisible_ = visible; }
	bool IsVisible() const { return isVisible_; }

private: /// ---------- メンバ変数 ---------- ///

	std::unique_ptr<Sprite> sprite_;

	std::string textureName_;
	Vector2 size_ = { 16, 16 }; // サイズ（ピクセル）
	bool isVisible_ = true;

};
