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

	// ヒットマーカー読み込み
	std::string hitTexture = "Resources/hitmarker.png"; // 生成した×画像のパス
	TextureManager::GetInstance()->LoadTexture(hitTexture);
	hitMarkerSprite_ = std::make_unique<Sprite>();
	hitMarkerSprite_->Initialize(hitTexture);
	hitMarkerSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	hitMarkerSprite_->SetPosition({ static_cast<float>(WinApp::kClientWidth / 2.0f), static_cast<float>(WinApp::kClientHeight / 2.0f) });
	hitMarkerSprite_->SetSize({ 48.0f, 48.0f });
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void Crosshair::Update()
{
	sprite_->Update();

	if (showHitMarker_)
	{
		hitMarkerTimer_ -= 1.0f / 60.0f;

		// スケール変化アニメーション
		hitMarkerScale_ += hitMarkerScaleVelocity_ * (1.0f / 60.0f);
		if (hitMarkerScale_ < 1.0f) hitMarkerScale_ = 1.0f;

		if (hitMarkerTimer_ <= 0.0f)
		{
			showHitMarker_ = false;
			hitMarkerScale_ = 1.0f; // リセット
		}
	}

	// ヒットマーカーの表示制御
	hitMarkerSprite_->Update();
}


/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void Crosshair::Draw()
{
	// 十字カーソルの描画
	if (isVisible_) sprite_->Draw();
	if (showHitMarker_ && hitMarkerSprite_)
	{
		hitMarkerSprite_->SetSize({ 48.0f * hitMarkerScale_, 48.0f * hitMarkerScale_ });
		hitMarkerSprite_->Draw();
	}
}

void Crosshair::ShowHitMarker()
{
	showHitMarker_ = true;
	hitMarkerTimer_ = hitMarkerDuration_;

	// アニメーション開始値
	hitMarkerScale_ = 1.5f;             // 最初は少し大きめに
	hitMarkerScaleVelocity_ = -2.5f;    // 徐々に小さくする
}
