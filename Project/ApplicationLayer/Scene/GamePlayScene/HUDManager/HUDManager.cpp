#include "HUDManager.h"

void HUDManager::Initialize()
{
	// スコア表示用のスプライトドロワーを初期化
	scoreDrawer_ = std::make_unique<NumberSpriteDrawer>();
	scoreDrawer_->Initialize(texturePath_);

	// キル数表示用のスプライトドロワーを初期化
	killDrawer_ = std::make_unique<NumberSpriteDrawer>();
	killDrawer_->Initialize(texturePath_);

	// 弾薬数表示用のスプライトドロワーを初期化
	ammoDrawer_ = std::make_unique<NumberSpriteDrawer>();
	ammoDrawer_->Initialize(texturePath_);
}

void HUDManager::Update()
{
}

void HUDManager::Draw()
{
	// リセット
	scoreDrawer_->Reset();

	// スコアの描画
	scoreDrawer_->DrawNumber(score_, { 600.0f, 20.0f });

	// リセット
	killDrawer_->Reset();

	// キル数の描画
	killDrawer_->DrawNumber(kills_, { 600.0f, 80.0f });

	// リセット
	ammoDrawer_->Reset();

	// 弾薬数の描画
	ammoDrawer_->DrawNumber(ammoInClip_, { 1000.0f, 620.0f });
	ammoDrawer_->DrawNumber(ammoReserve_, { 1120.0f, 620.0f });
}
