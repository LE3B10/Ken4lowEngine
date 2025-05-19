#pragma once
#include <memory>
#include <array>
#include <string>
#include "Sprite.h"
#include "Vector2.h"


/// --------------------------------------------------------------
///				　	数字スプライト描画クラス
/// --------------------------------------------------------------
class NumberSpriteDrawer
{
public: /// ---------- メンバ関数 ---------------- ///

	/// <summary>
	/// 数字スプライト描画クラスの初期化
	/// </summary>
	/// <param name="texturePath">テクスチャパス</param>
	/// <param name="digitWidth">テクスチャの幅</param>
	/// <param name="digitHeight">テクスチャの高さ</param>
	void Initialize(const std::string& texturePath, float digitWidth = 50.0f, float digitHeight = 50.0f);

	/// <summary>
	/// 数字テクスチャの描画
	/// </summary>
	/// <param name="value">数字</param>
	/// <param name="position">座標</param>
	void DrawNumber(int value, const Vector2& position);

	/// <summary>
	/// インデックスをリセット
	/// </summary>
	void Reset() { currentIndex_ = 0; }

private: /// ---------- メンバ変数 ---------------- ///

	std::string texturePath_; // テクスチャパス
	float digitWidth_ = 50.0f; // テクスチャの幅
	float digitHeight_ = 50.0f; // テクスチャの幅と高さ
	size_t maxDigits_ = 4; // 最大桁数
	std::vector<std::unique_ptr<Sprite>> reusableSprites_; // 桁数分のスプライト
	size_t currentIndex_ = 0; // 再利用管理用
};
