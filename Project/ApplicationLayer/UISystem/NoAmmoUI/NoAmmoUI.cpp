#include "NoAmmoUI.h"
#include <DirectXCommon.h>
#include <cmath>

void NoAmmoUI::Initialize(const std::string& texturePath)
{
	sprite_ = std::make_unique<K4E::Sprite>();
	sprite_->Initialize(texturePath);
	sprite_->SetAnchorPoint({ 0.5f, 0.5f });

	auto* dx = K4E::DirectXCommon::GetInstance();
	const float w = static_cast<float>(dx->GetClientWidth());
	const float h = static_cast<float>(dx->GetClientHeight());

	// 画面中央やや下
	sprite_->SetPosition({ w * 0.5f, h * 0.65f });
	sprite_->SetSize({ 240.0f, 64.0f });
	sprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
}

void NoAmmoUI::Update(float deltaTime)
{
	if (!sprite_) return;

	blinkTimer_ += deltaTime;

	float alpha = 0.0f;
	if (visible_)
	{
		// ふわっと点滅
		alpha = 0.55f + 0.45f * std::sinf(blinkTimer_ * 8.0f);
		if (alpha < 0.0f) alpha = 0.0f;
		if (alpha > 1.0f) alpha = 1.0f;
	}

	sprite_->SetColor({ 1.0f, 0.2f, 0.2f, alpha });
	sprite_->Update();
}

void NoAmmoUI::Draw()
{
	if (!visible_) return;
	if (!sprite_) return;
	sprite_->Draw();
}