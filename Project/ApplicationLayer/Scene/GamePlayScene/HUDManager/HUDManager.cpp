#include "HUDManager.h"


/// -------------------------------------------------------------
///				　		初期化処理
/// -------------------------------------------------------------
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

	// HP表示用のスプライトドロワーを初期化
	hpDrawer_ = std::make_unique<NumberSpriteDrawer>();
	hpDrawer_->Initialize(texturePath_);

	// グレー背景のスプライトを初期化
	hpBarBase_ = std::make_unique<Sprite>();
	hpBarBase_->Initialize("Resources/white.png");

	// 緑バーのスプライトを初期化
	hpBarFill_ = std::make_unique<Sprite>();
	hpBarFill_->Initialize("Resources/white.png");

	reloadCircle_ = std::make_unique<ReloadCircle>();
	reloadCircle_->Initialize("Resources/reload-circle.png");
}


/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void HUDManager::Update()
{
	// --- 位置・サイズの定義 ---
	const Vector2 barPosition = { 60.0f, 630.0f };
	const Vector2 barSize = { 200.0f, 30.0f };

	// --- 背景バー（グレー） ---
	hpBarBase_->SetPosition(barPosition);
	hpBarBase_->SetSize(barSize);
	hpBarBase_->SetColor({ 0.1f, 0.1f, 0.1f, 1.0f });
	hpBarBase_->Update();

	// --- 現在のHP割合から幅を計算 ---
	float ratio = static_cast<float>(hp_) / maxHP_;
	float filledWidth = barSize.x * std::clamp(ratio, 0.0f, 1.0f);

	// --- 色（緑〜赤に変化） ---
	Vector4 fillColor = { 1.0f - ratio, ratio, 0.0f, 1.0f };

	// --- HPバー（可変） ---
	hpBarFill_->SetPosition(barPosition); // 左端を起点に縮む
	hpBarFill_->SetSize({ filledWidth, barSize.y });
	hpBarFill_->SetColor(fillColor);
	hpBarFill_->Update();

	reloadCircle_->Update();
}


/// -------------------------------------------------------------
///				　			描画処理
/// -------------------------------------------------------------
void HUDManager::Draw()
{
	// リセット
	scoreDrawer_->Reset();

	// スコアの描画
	scoreDrawer_->DrawNumberCentered(score_, { 628.0f, 20.0f }, 24.0f);

	// リセット
	killDrawer_->Reset();

	// キル数の描画
	killDrawer_->DrawNumberCentered(kills_, { 628.0f, 80.0f });

	// リセット
	ammoDrawer_->Reset();

	// 弾薬数の描画
	ammoDrawer_->DrawNumberRightAligned(ammoInClip_, { 1060.0f, 620.0f });
	ammoDrawer_->DrawNumberRightAligned(ammoReserve_, { 1140.0f, 620.0f });


	// グレー背景の描画
	hpBarBase_->Draw();

	// 緑バーの描画
	hpBarFill_->Draw();

	// リロード円の描画
	reloadCircle_->Draw();

	// HPの描画
	DrawDebugHUD();
}


/// -------------------------------------------------------------
///				　			デバッグ用HUDの描画
/// -------------------------------------------------------------
void HUDManager::DrawDebugHUD()
{
#ifdef _DEBUG
	// リセット
	hpDrawer_->Reset();

	// HPの描画
	int percent = static_cast<int>((static_cast<float>(hp_) / maxHP_) * 100.0f);
	hpDrawer_->DrawNumberLeftAligned(percent, { 85.0f, 620.0f });
#endif // _DEBUG
}


/// -------------------------------------------------------------
///				　			リロード中の円を表示
/// -------------------------------------------------------------
void HUDManager::SetReloading(bool isReloading, float progress)
{
	if (reloadCircle_)
	{
		reloadCircle_->SetVisible(isReloading);
		reloadCircle_->SetProgress(progress);
	}
}