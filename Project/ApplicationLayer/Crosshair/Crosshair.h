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

	// ヒットマーカーの更新
	void ShowHitMarker(); // ヒットマーカー表示を開始

	// 表示ON/OFF切替
	void SetVisible(bool visible) { isVisible_ = visible; }
	bool IsVisible() const { return isVisible_; }

private: /// ---------- メンバ変数 ---------- ///

	std::unique_ptr<Sprite> sprite_;

	std::string textureName_;
	Vector2 size_ = { 16, 16 }; // サイズ（ピクセル）
	bool isVisible_ = true;

	bool showHitMarker_ = false;
	float hitMarkerTimer_ = 0.0f;
	const float hitMarkerDuration_ = 0.2f;

	// アニメーション用の拡大率
	float hitMarkerScale_ = 1.0f;
	float hitMarkerScaleVelocity_ = 0.0f;

	std::unique_ptr<Sprite> hitMarkerSprite_;
};
