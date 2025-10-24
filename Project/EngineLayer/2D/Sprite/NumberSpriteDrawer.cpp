#include "NumberSpriteDrawer.h"
#include <TextureManager.h>
#include <string>
#include <cassert>

/// -------------------------------------------------------------
///				　			初期化処理
/// -------------------------------------------------------------
void NumberSpriteDrawer::Initialize(const std::string& texturePath, float digitWidth, float digitHeight)
{
	// 引数をメンバ変数に設定
	digitWidth_ = digitWidth;	// テクスチャの幅
	digitHeight_ = digitHeight; // テクスチャの高さ
	texturePath_ = texturePath; // テクスチャパス
	currentIndex_ = 0;

	// テクスチャの読み込み
	TextureManager::GetInstance()->LoadTexture(texturePath);

	// スプライトの初期化
	reusableSprites_.clear();
}

/// -------------------------------------------------------------
///				　	 左詰めで数字を描画
/// -------------------------------------------------------------
void NumberSpriteDrawer::DrawNumberLeftAligned(int value, const Vector2& position, float spacing)
{
	// 桁数の制限
	std::string numberStr = std::to_string(value);
	float x = position.x;

	// 文字列の長さを制限
	for (char c : numberStr)
	{
		// 文字を数字に変換
		int digit = c - '0';

		// 数字以外は無視
		if (digit < 0 || digit > 9) continue;

		// スプライトのインデックスをリセット
		if (currentIndex_ >= reusableSprites_.size())
		{
			// スプライトの再利用が必要な場合、新しいスプライトを作成
			auto sprite = std::make_unique<Sprite>();
			sprite->Initialize(texturePath_);				// スプライトの初期化
			sprite->SetSize({ digitWidth_, digitHeight_ }); // スプライトのサイズを設定
			reusableSprites_.push_back(std::move(sprite));  // スプライトを追加
		}

		// スプライトのインデックスを取得
		auto& sprite = reusableSprites_[currentIndex_++];

		int row = digit / 5; // 5行に分けているので、行は5で割る
		int col = digit % 5; // 5列に分けているので、列は5で割った余り

		// UV座標の計算
		Vector2 uvPos =
		{
			static_cast<float>(col) * digitWidth_, // UV座標左上
			static_cast<float>(row) * digitHeight_ // UV座標左上
		};

		// UV座標テクスチャサイズ
		Vector2 size = { digitWidth_, digitHeight_ };

		// スプライトの設定
		sprite->SetTextureLeftTop(uvPos);		// UV座標の設定
		sprite->SetTextureSize(size);			// UVサイズの設定
		sprite->SetPosition({ x, position.y }); // スプライトの位置設定

		// スプライトの更新
		sprite->Update();

		// スプライトの描画
		sprite->Draw();

		// 次の桁の位置を計算
		x += spacing;
	}
}

/// -------------------------------------------------------------
///				　	 中央揃えで数字を描画
/// -------------------------------------------------------------
void NumberSpriteDrawer::DrawNumberCentered(int value, const Vector2& centerPosition, float spacing)
{
	// 数字を文字列に変換
	std::string numberStr = std::to_string(value);

	// 合計幅を計算
	float totalWidth = static_cast<float>(numberStr.size()) * spacing;

	// 開始位置を計算
	float x = centerPosition.x - totalWidth / 2.0f;

	// 左詰めで描画
	DrawNumberLeftAligned(value, { x, centerPosition.y }, spacing);
}

/// -------------------------------------------------------------
///				　	 右詰めで数字を描画
/// -------------------------------------------------------------
void NumberSpriteDrawer::DrawNumberRightAligned(int value, Vector2 rightPosition, float spacing)
{
	// 数字を文字列に変換
	std::string numberStr = std::to_string(value);

	// 合計幅を計算
	float totalWidth = static_cast<float>(numberStr.size()) * spacing;

	// 開始位置を計算
	float x = rightPosition.x - totalWidth;

	// 左詰めで描画
	DrawNumberLeftAligned(value, { x, rightPosition.y }, spacing);
}
