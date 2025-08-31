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
	void Initialize(const std::string& texturePath = "Crosshair/crosshair_circle_dot.png");

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
	std::unique_ptr<Sprite> shadow_;

	std::string textureName_;
	Vector2 size_ = { 16, 16 }; // サイズ（ピクセル）
	bool isVisible_ = true;

	bool showHitMarker_ = false;
	float hitMarkerTimer_ = 0.0f;
	const float hitMarkerDuration_ = 0.25f;

	// アニメーション用の拡大率
	float hitMarkerScale_ = 1.0f;
	float hitMarkerScaleVelocity_ = 0.0f;

	std::unique_ptr<Sprite> hitMarkerSprite_;

	std::unique_ptr<Sprite> hitMarkerShadow_;   // ヒットマーカー影
	float hitAlpha_ = 0.0f;                     // フェード用

	// ヒットマーカーの基準サイズ（大きさの元）
	float hitBaseSize_ = 112.0f;   // 96〜144で好みに調整
	float hitShadowPad_ = 6.0f;    // 影のはみ出し量
};
