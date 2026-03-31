#include "HUDManager.h"
#include "Player.h"

#include <algorithm>

/// -------------------------------------------------------------
///                    初期化処理
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
	hpWidget_->SetAnchorTopLeft({ 560.0f, 880.0f });
	hpWidget_->SetIconSize({ 22.0f, 22.0f });
	hpWidget_->SetPadding(6.0f);
	hpWidget_->SetHpPerHeart(10.0f); // 1ハート=10HP（必要なら変更）

	// 武器スロットHUDの初期化
	weaponSlot_ = std::make_unique<WeaponSlot>();
	weaponSlot_->Initialize("slot_frame.png", "slot_frame_selected.png");
	weaponSlot_->InitializeSlotNumbers("numbers02.png", 50.0f, 50.0f, { 8.0f, 8.0f }, 2.0f, 32, 32);

	// キューブアイコン
	// 武器カテゴリ別アイコン（スロット0..5）
	const std::array<std::string, WeaponSlot::kSlotCount> weaponIcons = {
		"icon/primary_icon.png",
		"icon/backup_icon.png",
		"icon/melee_icon.png",
		"icon/special_icon.png",
		"icon/sniper_icon.png",
		"icon/heavy_icon.png"
	};
	weaponSlot_->InitializeIcons(weaponIcons);

	weaponSlot_->InitializeAmmoDelimiter(
		"icon/slash_icon.png",
		{ 20.0f, 20.0f },   // 数字が20x20ならこれがちょうど良い
		{ 0.0f, 0.0f }      // 微調整したいならここでオフセット
	);

	// 弾薬表示初期化
	weaponSlot_->InitializeAmmoNumbers("Number.png",
		50, 50,
		{ 10, 10 },
		-5.0f,   // spacingは小さく
		20.0f, 20.0f); // drawサイズ

	waveUI_ = std::make_unique<WaveUI>();
	waveUI_->Initialize();
	waveUI_->SetVisible(true);

	damageIndicatorManager_ = std::make_unique<DamageIndicatorManager>();
	damageIndicatorManager_->Initialize();

	noAmmoUI_ = std::make_unique<NoAmmoUI>();
	noAmmoUI_->Initialize("white.png");
}

/// -------------------------------------------------------------
///                    更新処理
/// -------------------------------------------------------------
void HUDManager::Update(float deltaTime)
{
	bool isReloadingForHUD = false; // Crosshairへ渡す用

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
			isReloadingForHUD = false;
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

			isReloadingForHUD = isReloading;

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

	// ---------- クロスヘア：武器のレティクルデータを反映 ----------
	if (crosshair_ && player_)
	{
		FWeaponReticleData r{};
		float spread = 0.0f;
		bool isADS = false;

		if (player_->GetReticleUI(r, spread, isADS))
		{
			// 基本
			crosshair_->SetReticleType(static_cast<int>(r.reticleType));
			crosshair_->SetReticleTexture(r.reticleTexturePath);
			crosshair_->SetBaseSize(r.reticleBaseSize);
			crosshair_->SetMaxSize(r.reticleMaxSize);
			crosshair_->SetExpandPerShot(r.reticleExpandPerShot);
			crosshair_->SetRecoverSpeed(r.reticleRecoverSpeed);
			crosshair_->SetSpreadValue(spread);

			// 移動拡散（移動状態そのものは GamePlayScene などから SetCrosshairMovementState で渡す）
			crosshair_->SetMoveExpandEnabled(r.bEnableMoveReticleExpand);
			crosshair_->SetMoveExpandMultipliers(
				r.moveExpandMultiplier,
				r.sprintExpandMultiplier,
				r.airExpandMultiplier,
				r.landExpandImpulse);

			// ADS切替
			crosshair_->SetHideInADS(r.bHideReticleInADS);
			crosshair_->SetADSState(isADS);
			crosshair_->SetHideWhileReload(true);
			crosshair_->SetReloadState(isReloadingForHUD);
			crosshair_->SetUseADSReticleOverride(r.bUseAdsReticleOverride);
			crosshair_->SetADSReticleTexture(r.adsReticleTexturePath);
			crosshair_->SetUseADSCenterDot(r.bUseAdsCenterDot);
			crosshair_->SetADSCenterDotTexture(r.adsCenterDotTexturePath);
			crosshair_->SetADSBlendTime(r.adsReticleBlendTime);

			// ヒット / 撃破マーカー
			crosshair_->SetShowHitMarker(r.bShowHitMarker);
			crosshair_->SetHitMarkerTexture(r.hitMarkerTexturePath);
			crosshair_->SetUseHeadshotMarker(r.bUseHeadshotMarker);
			crosshair_->SetHeadshotHitMarkerTexture(r.headshotHitMarkerTexturePath);
			crosshair_->SetUseKillConfirmMarker(r.bUseKillConfirmMarker);
			crosshair_->SetKillConfirmMarkerTexture(r.killConfirmMarkerTexturePath);
			crosshair_->SetHitMarkerDuration(r.hitMarkerDuration);
			crosshair_->SetKillConfirmDuration(r.killConfirmDuration);
		}
	}

	if (hpWidget_) hpWidget_->Update();
	if (reloadCircle_) reloadCircle_->Update();
	if (crosshair_) crosshair_->Update();


	// ---------- 武器スロットHUD ----------
	if (weaponSlot_ && player_)
	{
		WeaponSlot::HudSnapshot snap{};
		if (player_->GetWeaponSlotHUD(snap))
		{
			weaponSlot_->Update(snap);
		}
	}

	if (waveUI_) waveUI_->Update(deltaTime);

	if (damageIndicatorManager_) damageIndicatorManager_->Update(deltaTime);

	if (noAmmoUI_ && player_)
	{
		bool showNoAmmo = false;
		player_->GetNoAmmoUI(showNoAmmo);
		noAmmoUI_->SetVisible(showNoAmmo);
		noAmmoUI_->Update(deltaTime);
	}
}

/// -------------------------------------------------------------
///                    描画処理
/// -------------------------------------------------------------
void HUDManager::Draw()
{
	if (hpWidget_ && hpWidget_->IsVisible()) hpWidget_->Draw();
	if (reloadCircle_ && reloadCircle_->IsVisible()) reloadCircle_->Draw();
	if (crosshair_ && crosshair_->IsVisible()) crosshair_->Draw();
	if (weaponSlot_) weaponSlot_->Draw();
	if (waveUI_ && waveUI_->IsVisible()) waveUI_->Draw();

	if (damageIndicatorManager_) damageIndicatorManager_->Draw();

	if (noAmmoUI_ && noAmmoUI_->IsVisible()) noAmmoUI_->Draw();
}

void HUDManager::SetHP(float hp, float maxHp)
{
	if (hpWidget_) hpWidget_->SetHP(hp, maxHp);
}

void HUDManager::NotifyPlayerHit(float strength01)
{
	if (hpWidget_) hpWidget_->NotifyHit(strength01);
}

void HUDManager::NotifyEnemyHit(bool isHeadshot)
{
	if (!crosshair_) return;
	crosshair_->NotifyEnemyHit(isHeadshot, false);
}

void HUDManager::NotifyEnemyKill(bool isHeadshot)
{
	if (!crosshair_) return;
	// kill優先（ヘッドショットキルなら killConfirm テクスチャにフォールバック）
	crosshair_->NotifyEnemyHit(isHeadshot, true);
}

void HUDManager::SetCrosshairMovementState(bool isMoving, bool isSprinting, bool isAirborne)
{
	if (!crosshair_) return;
	crosshair_->SetMovementState(isMoving, isSprinting, isAirborne);
}

void HUDManager::NotifyCrosshairLanded()
{
	if (!crosshair_) return;
	crosshair_->NotifyLanded();
}

void HUDManager::SetWaveDisplayState(const WaveUI::DisplayState& state)
{
	if (waveUI_) waveUI_->SetDisplayState(state);
}

void HUDManager::NotifyWaveStarted(int waveNumber, bool isFinalWave)
{
	if (waveUI_) waveUI_->NotifyWaveStarted(waveNumber, isFinalWave);
}

void HUDManager::NotifyAllWavesCleared()
{
	if (waveUI_) waveUI_->NotifyAllWavesCleared();
}

void HUDManager::SetWaveUIVisible(bool v)
{
	if (waveUI_) waveUI_->SetVisible(v);
}

void HUDManager::AddDamageIndicator(const K4E::Vector3& playerPos, const K4E::Vector3& attackerPos, const K4E::Vector3& cameraForward, const K4E::Vector3& cameraRight)
{
	if (damageIndicatorManager_)
	{
		damageIndicatorManager_->AddIndicator(playerPos, attackerPos, cameraForward, cameraRight);
	}
}

void HUDManager::SetCrosshairTargetingEnemy(bool v)
{
	if (!crosshair_) return;
	crosshair_->SetTargetingEnemy(v);
}
