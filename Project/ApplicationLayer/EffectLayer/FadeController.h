#pragma once
#include <Sprite.h>

#include <memory>
#include <vector>
#include <string>
#include <functional>

/// -------------------------------------------------------------
///						フェードコントローラー
/// -------------------------------------------------------------
class FadeController
{
public: /// ---------- 列挙型 ---------- ///

	// フェード状態
	enum class FadeMode
	{
		BlackFade,		// 黒フェード
		WhiteFlash,		// 白フェード
		WipeHorizontal, // 横ワイプ
		LetterBox,		// レターボックス
		GridColumns,	// 格子状
		Checkerboard,	// チェッカーボード
		CircleScale,	// 円拡大
	};
	FadeMode mode_ = FadeMode::BlackFade; // フェードモード

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(float screenWidth, float screenHeight, const std::string& texturePath);

	// 更新処理
	void Update(float deltaTime);

	// 描画処理
	void Draw();

public: /// ---------- フェード開始関数 ---------- ///

	// フェードイン開始
	void StartFadeIn(float duration);

	// フェードアウト開始
	void StartFadeOut(float duration);

	// フェードモード設定
	void SetFadeMode(FadeMode mode) { mode_ = mode; }

	// グリッド列数設定
	void SetGrid(int columns, int rows);

	// ワイプ方向設定 (0:左->右, 1:右->左, 2:上->下, 3:下->上)
	void SetDirection(int direction);

	// チェッカーボードの遅延設定
	void SetCheckerDelay(float delay);

public: /// ---------- セッター ---------- ///

	// テクスチャセット
	void SetTexture(const std::string& texturePath);

	// 色セット
	void SetTintColor(const Vector4& color);

	void SetOnComplete(std::function<void()> onComplete) { onComplete_ = std::move(onComplete); }

public: /// ---------- ゲッター ---------- ///

	// フェード中かどうか取得
	bool IsFading() const { return isFading_; }

	// フェードイン中かどうか取得
	bool IsFadeIn() const { return isFadeIn_; }

	// 不透明度取得
	float GetAlpha() const { return alpha_; }

private: /// ---------- メンバ関数 ---------- ///

	void RebuildGrid(); // グリッドの再構築

private: /// ---------- メンバ変数 ---------- ///

	std::unique_ptr<Sprite> fadeSprite_; // フェード用スプライト
	float screenWidth_ = 1280.0f; // 画面幅
	float screenHeight_ = 720.0f; // 画面高さ

	float duration_ = 1.0f;   // フェードの所要時間
	float timer_ = 0.0f;      // 経過時間
	float alpha_ = 0.0f;      // 不透明度 (0.0f:透明 - 1.0f:不透明)
	bool isFading_ = false;   // フェード中かどうか
	bool isFadeIn_ = true;	  // フェードインかどうか

	std::function<void()> onComplete_ = nullptr; // フェード完了時のコールバック

private: /// ---------- レターボックス用メンバ変数 ---------- ///

	std::unique_ptr<Sprite> topBar_, bottomBar_; // レターボックス用スプライト
	float barHeight_ = 0.0f; // レターボックスの高さ
	float wipeWidth_ = 0.0f; // ワイプの幅

private: /// ---------- グリッド用メンバ変数 ---------- ///

	// グリッドタイルの構造体
	struct Tile
	{
		std::unique_ptr<Sprite> sprite;
		float x = 0.0f;
		float y = 0.0f;
		float width = 0.0f;
		float height = 0.0f;
	};
	std::vector<Tile> tiles_; // グリッドタイル
	int columns_ = 12; // 列数
	int rows_ = 8;    // 行数
	bool needRebuildGrid_ = false; // グリッド再構築フラグ
	int direction_ = 0; // ワイプ方向 (0:左->右, 1:右->左, 2:上->下, 3:下->上)
	float checkerDelayStep_ = 0.05f; // チェッカーボードの遅延ステップ
};

