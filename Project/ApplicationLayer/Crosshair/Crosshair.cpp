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
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void Crosshair::Update()
{
	sprite_->Update();
}


/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void Crosshair::Draw()
{
	sprite_->Draw();
}
