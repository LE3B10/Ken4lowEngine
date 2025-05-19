#pragma once
#include <memory>
#include <array>
#include <string>
#include "Sprite.h"
#include "Vector2.h"


/// --------------------------------------------------------------
///				　			　 数字スプライト描画クラス
/// --------------------------------------------------------------
class NumberSpriteDrawer
{
public: /// ---------- メンバ関数 ---------------- ///

	/// <summary>
	/// 初期化処理
	/// </summary>
	/// <param name="texturePath">テクスチャパス</param>
	/// <param name="digitWidth">幅</param>
	/// <param name="digitHeight">高さ</param>
	void Initialize(const std::string& texturePath = "Resources/number.png", float digitWidth = 160.0f, float digitHeight = 160.0f);

	/// <summary>
	/// 描画処理
	/// </summary>
	/// <param name="value"></param>
	/// <param name="position"></param>
	void DrawNumber(int value, const Vector2& position);

private: /// ---------- メンバ変数 ---------------- ///

	std::array<std::unique_ptr<Sprite>, 10> digitSprites_; // 0〜9
	std::vector<std::unique_ptr<Sprite>> tempSprites_; // 一時的なスプライト
	float digitWidth_ = 160.0f;
	float digitHeight_ = 160.0f;

	std::string texturePath_ = "Resources/number.png"; // テクスチャパス
};
