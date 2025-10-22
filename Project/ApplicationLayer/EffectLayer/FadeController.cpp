#define NOMINMAX
#include "FadeController.h"
#include <LinearInterpolation.h>

#include <algorithm>

/// -------------------------------------------------------------
///						初期化処理
/// -------------------------------------------------------------
void FadeController::Initialize(float screenWidth, float screenHeight, const std::string& texturePath)
{
	// 画面サイズの保存
	screenWidth_ = screenWidth;
	screenHeight_ = screenHeight;

	// フェード用スプライトの生成
	fadeSprite_ = std::make_unique<Sprite>();
	fadeSprite_->Initialize(texturePath);
	fadeSprite_->SetAnchorPoint({ 0.0f, 0.0f }); // 左上基準
	fadeSprite_->SetPosition({ 0.0f, 0.0f });
	fadeSprite_->SetSize({ screenWidth_, screenHeight_ });
	fadeSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });

	// レターボックス用スプライトの生成
	topBar_ = std::make_unique<Sprite>();
	topBar_->Initialize(texturePath);
	topBar_->SetAnchorPoint({ 0.0f, 0.0f }); // 左上基準
	topBar_->SetSize({ screenWidth_, 0.0f });

	bottomBar_ = std::make_unique<Sprite>();
	bottomBar_->Initialize(texturePath);
	bottomBar_->SetAnchorPoint({ 0.0f, 0.0f }); // 左上基準
	bottomBar_->SetSize({ screenWidth_, 0.0f });

	alpha_ = 0.0f; // 初期状態は透明
	isFading_ = false; // フェード中ではない
	isFadeIn_ = true;  // フェードイン状態
}

/// -------------------------------------------------------------
///						更新処理
/// -------------------------------------------------------------
void FadeController::Update(float deltaTime)
{
	// フェード中でなければ処理しない
	if (!isFading_) return;

	// 経過時間を更新
	timer_ += deltaTime;
	float t = std::clamp(timer_ / std::max(0.001f, duration_), 0.0f, 1.0f); // 0.0f ~ 1.0fに正規化してクランプ

	if (needRebuildGrid_ && (mode_ == FadeMode::GridColumns || mode_ == FadeMode::Checkerboard))
	{
		RebuildGrid();
	}

	switch (mode_)
	{
	case FadeController::FadeMode::BlackFade:
	{
		// 黒フェードは黒色で
		alpha_ = isFadeIn_
			? Lerp(1.0f, 0.0f, t)  // フェードイン
			: Lerp(0.0f, 1.0f, t); // フェードアウト

		// スプライトの色を更新
		Vector4 color = fadeSprite_->GetColor();
		color = { 0.0f, 0.0f, 0.0f, alpha_ }; // 黒色に固定
		fadeSprite_->SetColor(color);
		fadeSprite_->SetSize({ screenWidth_, screenHeight_ });
		break;
	}
	case FadeController::FadeMode::WhiteFlash:
	{
		// 白フェードは白色で
		alpha_ = isFadeIn_
			? Lerp(1.0f, 0.0f, t)  // フェードイン
			: Lerp(0.0f, 1.0f, t); // フェードアウト

		// スプライトの色を更新
		Vector4 color = fadeSprite_->GetColor();
		color = { 1.0f, 1.0f, 1.0f, alpha_ }; // 白色に固定
		fadeSprite_->SetColor(color);
		fadeSprite_->SetSize({ screenWidth_, screenHeight_ });
		break;
	}
	case FadeController::FadeMode::WipeHorizontal:
	{
		// 横ワイプ
		float from = isFadeIn_ ? screenWidth_ : 0.0f; // フェードイン開始位置
		float to = isFadeIn_ ? 0.0f : screenWidth_;   // フェードイン終了位置
		wipeWidth_ = Lerp(from, to, t); // ワイプの幅を線形補間

		Vector4 color = fadeSprite_->GetColor();
		color = { 0.0f, 0.0f, 0.0f, 1.0f }; // 黒色に固定
		fadeSprite_->SetColor(color);
		fadeSprite_->SetPosition({}); // 左上基準
		fadeSprite_->SetSize({ std::max(0.0f,wipeWidth_), screenHeight_ });
		break;
	}
	case FadeController::FadeMode::LetterBox:
	{
		// 上下バーの高さを計算
		float from = isFadeIn_ ? (screenHeight_ / 2.0f) : 0.0f; // フェードイン開始高さ
		float to = isFadeIn_ ? 0.0f : (screenHeight_ / 2.0f);   // フェードイン終了高さ
		barHeight_ = Lerp(from, to, t);

		Vector4 color = { 0.0f,0.0f,0.0f,1.0f }; // 黒色に固定
		topBar_->SetColor(color); bottomBar_->SetColor(color); // 色設定

		topBar_->SetPosition({ 0.0f, 0.0f }); // 上バーは上端に固定
		topBar_->SetSize({ screenWidth_, std::max(0.0f,barHeight_) });

		bottomBar_->SetPosition({ 0.0f, screenHeight_ - std::max(0.0f,barHeight_) }); // 下バーは下端に固定
		bottomBar_->SetSize({ screenWidth_, std::max(0.0f,barHeight_) });
		break;
	}
	case FadeMode::GridColumns:
	{
		// 列単位に α=0→1（FadeOut）/ 1→0（FadeIn）
		// direction_ で進行方向を反転
		float progress = std::clamp(timer_ / std::max(0.001f, duration_), 0.0f, 1.0f);
		int nColsVisible = (int)std::round(progress * columns_);
		for (int r = 0; r < rows_; ++r)
		{
			for (int c = 0; c < columns_; ++c)
			{
				int idx = r * columns_ + c;
				int colIndex = (direction_ >= 0) ? c : (columns_ - 1 - c);
				bool covered = (colIndex < nColsVisible); // FadeOut: 覆っていく
				float a = covered ? 1.0f : 0.0f;
				if (isFadeIn_) a = 1.0f - a;              // FadeInは逆
				auto col = tiles_[idx].sprite->GetColor(); col.x = 0; col.y = 0; col.z = 0; col.w = a;
				tiles_[idx].sprite->SetColor(col);
				tiles_[idx].sprite->Update();
			}
		}
		break;
	}
	case FadeMode::Checkerboard:
	{
		// rows_, cols_, checkerDelayStep_, tiles_[] を使用している前提
		const int maxIndex = (rows_ - 1) + (columns_ - 1);
		const float maxDelay = checkerDelayStep_ * maxIndex;
		const float anim = std::max(0.001f, duration_ - maxDelay); // ★共通アニメ時間

		// 各タイルごとに delay をずらして anim 秒かけて 0→1（FadeOut）/ 1→0（FadeIn）
		for (int r = 0; r < rows_; ++r)
		{
			for (int c = 0; c < columns_; ++c)
			{
				const int idx = r * columns_ + c;
				const float delay = float(r + c) * checkerDelayStep_;
				// 開始までは0、開始後に anim 秒かけて 0→1
				float local = (timer_ - delay) / anim;
				local = std::clamp(local, 0.0f, 1.0f);

				float a = isFadeIn_ ? (1.0f - local) : local; // Inは逆向き
				Vector4 col = tiles_[idx].sprite->GetColor();
				col.x = col.y = col.z = 0.0f; col.w = a;
				tiles_[idx].sprite->SetColor(col);
				tiles_[idx].sprite->Update();
			}
		}
		break;
	}

	case FadeMode::CircleScale:
	{
		// 円PNGをスケールで開閉（αは常に1）
		// FadeOut: 小→大で覆う / FadeIn: 大→小で開く
		float sFrom = isFadeIn_ ? 1.3f : 0.6f;
		float sTo = isFadeIn_ ? 0.6f : 1.3f;
		float s = Lerp(sFrom, sTo, std::clamp(timer_ / std::max(0.001f, duration_), 0.0f, 1.0f));
		fadeSprite_->SetColor({ 0,0,0,1 });              // PNGのアルファに任せる
		// 画面中心に合わせたい場合は Position を中央寄せに
		fadeSprite_->SetPosition({ 0,0 });
		fadeSprite_->SetSize({ screenWidth_ * s, screenHeight_ * s });
		fadeSprite_->Update();
		break;
	}
	}

	if (fadeSprite_) fadeSprite_->Update();
	if (topBar_)     topBar_->Update();
	if (bottomBar_)  bottomBar_->Update();

	if (t >= 1.0f)
	{
		isFading_ = false; // フェード完了
		if (onComplete_) onComplete_(); // コールバックが設定されていれば呼び出す
	}
}

/// -------------------------------------------------------------
///						描画処理
/// -------------------------------------------------------------
void FadeController::Draw()
{
	switch (mode_)
	{
	case FadeMode::BlackFade:	// 黒フェード
	case FadeMode::WhiteFlash:	// 白フェード
	case FadeMode::WipeHorizontal: // 横ワイプ
	{
		if (fadeSprite_) fadeSprite_->Draw();
		break;
	}
	case FadeMode::LetterBox:
	{
		if (topBar_)    topBar_->Draw();
		if (bottomBar_) bottomBar_->Draw();
		break;
	}
	case FadeMode::GridColumns:
	case FadeMode::Checkerboard:
	{
		for (auto& t : tiles_) t.sprite->Draw();
		break;
	}
	case FadeMode::CircleScale:
	{
		if (fadeSprite_) fadeSprite_->Draw();
		break;
	}
	}
}

/// -------------------------------------------------------------
///					フェード開始関数
/// -------------------------------------------------------------
void FadeController::StartFadeIn(float duration)
{
	if (isFading_) return; // すでにフェード中なら無視
	isFading_ = true;
	isFadeIn_ = true;
	duration_ = duration;
	timer_ = 0.0f;
	alpha_ = 1.0f; // フェードイン開始時は不透明
}

/// -------------------------------------------------------------
///					フェード開始関数
/// -------------------------------------------------------------
void FadeController::StartFadeOut(float duration)
{
	if (isFading_) return; // すでにフェード中なら無視
	isFading_ = true;
	isFadeIn_ = false;
	duration_ = duration;
	timer_ = 0.0f;
	alpha_ = 0.0f; // フェードアウト開始時は透明
}

/// -------------------------------------------------------------
///						　グリッド設定
/// -------------------------------------------------------------
void FadeController::SetGrid(int columns, int rows)
{
	columns_ = std::max(1, columns); // 1未満は不可
	rows_ = std::max(1, rows); // 1未満は不可
	needRebuildGrid_ = true; // 次回更新時にグリッド再構築
}

/// -------------------------------------------------------------
///						ワイプ方向設定
/// -------------------------------------------------------------
void FadeController::SetDirection(int direction)
{
	direction_ = (direction >= 0) ? 1 : -1; // 0以上なら正方向、負なら逆方向
}

/// -------------------------------------------------------------
///					チェッカーボードの遅延設定
/// -------------------------------------------------------------
void FadeController::SetCheckerDelay(float delay)
{
	checkerDelayStep_ = std::max(0.0f, delay); // 負の値は不可
}

/// -------------------------------------------------------------
///						テクスチャセット
/// -------------------------------------------------------------
void FadeController::SetTexture(const std::string& texturePath)
{
	if (!fadeSprite_) return; // フェード用スプライトがなければ処理しない
	fadeSprite_->Initialize(texturePath); // テクスチャ設定
	fadeSprite_->SetSize({ screenWidth_, screenHeight_ }); // 画面サイズに合わせる
}

/// -------------------------------------------------------------
///						　色セット
/// -------------------------------------------------------------
void FadeController::SetTintColor(const Vector4& color)
{
	if (!fadeSprite_) return; // フェード用スプライトがなければ処理しない
	fadeSprite_->SetColor(color); // 色設定
}

/// -------------------------------------------------------------
///						グリッドの再構築
/// -------------------------------------------------------------
void FadeController::RebuildGrid()
{
	tiles_.clear();
	tiles_.reserve(columns_ * rows_);

	const float tw = screenWidth_ / columns_; // タイル幅
	const float th = screenHeight_ / rows_;	  // タイル高さ

	// タイルを生成
	for (int r = 0; r < rows_; ++r)
	{
		for (int c = 0; c < columns_; ++c)
		{
			Tile t;
			t.x = c * tw; t.y = r * th; t.width = tw; t.height = th; // 位置とサイズ設定
			t.sprite = std::make_unique<Sprite>();
			t.sprite->Initialize("white.png");
			t.sprite->SetAnchorPoint({ 0,0 });
			t.sprite->SetPosition({ t.x, t.y });
			t.sprite->SetSize({ t.width, t.height });
			t.sprite->SetColor({ 0,0,0,0 }); // 黒×透明スタート
			tiles_.push_back(std::move(t));
		}
	}
	needRebuildGrid_ = false;
}
