#include "HUDManager.h"
#include "Player.h"

#include <algorithm>

/// -------------------------------------------------------------
///				　			　 初期化処理
/// -------------------------------------------------------------
void HUDManager::Initialize()
{
	// リロード円の初期化
	reloadCircle_ = std::make_unique<ReloadCircle>();
	reloadCircle_->Initialize("reload-circle.png");

	// 十字照準の初期化
	crosshair_ = std::make_unique<Crosshair>();
	crosshair_->Initialize();

	// HP（ハート）の初期化
	hpWidget_ = std::make_unique<HPWidget>();
	hpWidget_->Initialize();
	// 位置やサイズは好みで調整OK
	hpWidget_->SetAnchorTopLeft({ 20.0f, 20.0f });
	hpWidget_->SetIconSize({ 22.0f, 22.0f });
	hpWidget_->SetPadding(6.0f);
	hpWidget_->SetHpPerHeart(10.0f); // 1ハート=10HP（必要なら変更）
}

/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void HUDManager::Update()
{
	/// ---------- リロード円：プレイヤーの武器リロード状態に同期 ---------- ///
	if (reloadCircle_)
	{
		bool isReloading = false;
		float reloadTimer = 0.0f;
		float reloadSec = 0.0f;

		const bool hasInfo = (player_ != nullptr) && player_->GetReloadUI(isReloading, reloadTimer, reloadSec);
		if (!hasInfo || reloadSec <= 1e-6f)
		{
			// 情報が取れない/未ロードなら非表示
			reloadCircle_->SetReloading(false, 0.0f);
			prevReloading_ = false;
		}
		else
		{
			// リロード開始時に「reloadTimerが残り時間か経過時間か」を判定
			if (isReloading && !prevReloading_)
			{
				// start直後に timer が reloadSec に近ければ「残り時間」扱い、0 に近ければ「経過時間」扱い
				reloadTimerIsRemaining_ = (reloadTimer > reloadSec * 0.5f);
			}

			float progress01 = 0.0f;
			if (isReloading)
			{
				const float t = std::clamp(reloadTimer / reloadSec, 0.0f, 1.0f);
				progress01 = reloadTimerIsRemaining_ ? (1.0f - t) : t;
			}

			reloadCircle_->SetReloading(isReloading, progress01);
			prevReloading_ = isReloading;
		}
	}

	if (hpWidget_) hpWidget_->Update();
	if (reloadCircle_) reloadCircle_->Update();
	if (crosshair_) crosshair_->Update();
}

/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void HUDManager::Draw()
{
	if (hpWidget_ && hpWidget_->IsVisible()) hpWidget_->Draw();
	if (reloadCircle_ && reloadCircle_->IsVisible()) reloadCircle_->Draw();
	if (crosshair_ && crosshair_->IsVisible()) crosshair_->Draw();
}

void HUDManager::SetHP(float hp, float maxHp)
{
	if (hpWidget_) hpWidget_->SetHP(hp, maxHp);
}

void HUDManager::NotifyPlayerHit(float strength01)
{
	if (hpWidget_) hpWidget_->NotifyHit(strength01);
}
