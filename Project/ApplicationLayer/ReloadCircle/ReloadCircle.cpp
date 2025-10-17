#include "ReloadCircle.h"
#include "Weapon.h"
#include "winApp.h"

#include <numbers>


/// -------------------------------------------------------------
///				　		初期化処理
/// -------------------------------------------------------------
void ReloadCircle::Initialize(const std::string& texturePath)
{
	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(texturePath);
	sprite_->SetAnchorPoint({ 0.5f, 0.5f }); // アンカーを中央に設定
	sprite_->SetPosition({
		static_cast<float>(WinApp::kClientWidth / 2.0f),
		static_cast<float>(WinApp::kClientHeight / 2.0f) });
	sprite_->SetSize({ 48.0f, 48.0f }); // サイズは適宜調整
	sprite_->SetRotation(-std::numbers::pi_v<float> / 2.0f); // 初期回転
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void ReloadCircle::Update()
{
	// 武器とスプライトがセットされていない場合は処理しない
	if (!sprite_ || !weapon_) return;

	// 武器の状態を直接参照
	/*bool isReloading = weapon_->IsReloading();
	float progress = weapon_->GetReloadProgress();*/

	// スプライトに情報を渡す
	/*sprite_->SetReloadProgress(isReloading, progress);
	sprite_->Update();*/
}


/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void ReloadCircle::Draw()
{
	if (isVisible_) sprite_->Draw();
}


/// -------------------------------------------------------------
///				　	　リロードの進捗を設定
/// -------------------------------------------------------------
void ReloadCircle::SetProgress(float progress)
{
	progress_ = std::clamp(progress, 0.0f, 1.0f);
	if (sprite_) sprite_->SetReloadProgress(true, progress); // HLSLへ進行度を反映
}
