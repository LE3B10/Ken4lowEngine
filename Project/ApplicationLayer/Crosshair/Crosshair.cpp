#include "Crosshair.h"
#include <DirectXCommon.h>
#include <TextureManager.h>
#include "WinApp.h"


/// -------------------------------------------------------------
///				　		初期化処理
/// -------------------------------------------------------------
void Crosshair::Initialize(const std::string& texturePath)
{
	TextureManager::GetInstance()->LoadTexture(texturePath);

	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(texturePath);

	sprite_->SetAnchorPoint({ 0.5f, 0.5f }); // アンカーを中央に設定
	sprite_->SetPosition({
		static_cast<float>(WinApp::kClientWidth / 2.0f),
		static_cast<float>(WinApp::kClientHeight / 2.0f) });
	sprite_->SetSize({ 48.0f, 48.0f });

	// 影（黒半透明を少し大きく描く）
	shadow_ = std::make_unique<Sprite>();
	shadow_->Initialize(texturePath);
	shadow_->SetAnchorPoint({ 0.5f,0.5f });
	shadow_->SetPosition(sprite_->GetPosition());
	shadow_->SetSize({ 52.0f,52.0f });
	shadow_->SetColor({ 0,0,0,0.6f });

	// ヒットマーカー読み込み
	std::string hitTexture = "Crosshair/hitmarker_x_glow.png"; // 生成した×画像のパス
	TextureManager::GetInstance()->LoadTexture(hitTexture);
	hitMarkerSprite_ = std::make_unique<Sprite>();
	hitMarkerSprite_->Initialize(hitTexture);
	hitMarkerSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	hitMarkerSprite_->SetPosition({ static_cast<float>(WinApp::kClientWidth / 2.0f), static_cast<float>(WinApp::kClientHeight / 2.0f) });
	hitMarkerSprite_->SetSize({ 128.0f, 128.0f });

	// 影
	hitMarkerShadow_ = std::make_unique<Sprite>();
	hitMarkerShadow_->Initialize(hitTexture);
	hitMarkerShadow_->SetAnchorPoint({ 0.5f,0.5f });
	hitMarkerShadow_->SetPosition(sprite_->GetPosition());
	hitMarkerShadow_->SetSize({ 132.0f,132.0f });
	hitMarkerShadow_->SetColor({ 0,0,0,0.6f });
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void Crosshair::Update()
{
	sprite_->Update();
	shadow_->Update();

	if (showHitMarker_)
	{
		hitMarkerTimer_ -= 1.0f / 60.0f;

		// スケール変化アニメーション
		hitMarkerScale_ += hitMarkerScaleVelocity_ * (1.0f / 60.0f);
		if (hitMarkerScale_ < 1.0f) hitMarkerScale_ = 1.0f;

		// アルファをタイマーから計算（0→1→0 ではなく 1→0）
		hitAlpha_ = std::clamp(hitMarkerTimer_ / hitMarkerDuration_, 0.0f, 1.0f);

		if (hitMarkerTimer_ <= 0.0f)
		{
			showHitMarker_ = false;
			hitMarkerScale_ = 1.0f; // リセット
			hitAlpha_ = 0.0f;
		}
	}

	// ヒットマーカーの表示制御
	// サイズ＆色を更新してから Update（スプライト実装がUpdate前提のため）
	if (showHitMarker_) {
		const Vector2 sz = { hitBaseSize_ * hitMarkerScale_, hitBaseSize_ * hitMarkerScale_ };
		hitMarkerShadow_->SetSize({ sz.x + 4.0f, sz.y + 4.0f });
		hitMarkerSprite_->SetSize(sz);
		hitMarkerShadow_->SetColor({ 0,0,0,0.6f * hitAlpha_ });
		hitMarkerSprite_->SetColor({ 1,1,1, hitAlpha_ }); // 白ベース画像をαでフェード

	}
	hitMarkerShadow_->Update();
	hitMarkerSprite_->Update();
}


/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void Crosshair::Draw()
{
	// 十字カーソルの描画
	if (isVisible_) { shadow_->Draw(); sprite_->Draw(); } // 影→本体の順
	if (showHitMarker_ && hitMarkerSprite_)
	{
		// 影を先に → 本体を後に（1回だけ）
		hitMarkerShadow_->Draw();
		hitMarkerSprite_->Draw();
	}
}

/// -------------------------------------------------------------
///				　		ヒットマーカー表示開始
/// -------------------------------------------------------------
void Crosshair::ShowHitMarker()
{
	showHitMarker_ = true; // 表示フラグON
	hitMarkerTimer_ = hitMarkerDuration_; // タイマーリセット

	hitMarkerScale_ = 1.8f;          // もう少し大きく
	hitMarkerScaleVelocity_ = -4.0f; // キュッと収束

	hitAlpha_ = 1.0f; // 初期は不透明
}
