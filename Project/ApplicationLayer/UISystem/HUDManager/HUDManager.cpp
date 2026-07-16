#define NOMINMAX
#include "HUDManager.h"
#include "Player.h"
#include "ApplicationLayer/Character/Player/IPlayerRuntime.h"
#include "ApplicationLayer/Character/Player/Actor/PlayerActor.h"

#include <algorithm>
#include <array>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

HUDManager::~HUDManager() = default;

/// -------------------------------------------------------------
///                    初期化処理
/// -------------------------------------------------------------
void HUDManager::Initialize()
{
	// 個別HUDはHUDManagerが所有し、GamePlayWorldからはHUDManager経由でまとめて更新/描画する。
	reloadCircle_ = std::make_unique<ReloadCircle>();
	reloadCircle_->Initialize("UI/Common/reload-circle.dds");

	// 十字照準の初期化
	crosshair_ = std::make_unique<Crosshair>();
	crosshair_->Initialize();

	// HP（ハート）の初期化
	hpWidget_ = std::make_unique<HPWidget>();
	hpWidget_->Initialize();

	// HPは固定内部解像度に合わせた左下寄り配置で、ハート数が増えても横方向に伸びる。
	hpWidget_->SetAnchorTopLeft({ 560.0f, 880.0f });
	hpWidget_->SetIconSize({ 22.0f, 22.0f });
	hpWidget_->SetPadding(6.0f);
	hpWidget_->SetHpPerHeart(10.0f);

	// 武器スロットHUDの初期化
	weaponSlot_ = std::make_unique<WeaponSlot>();
	weaponSlot_->Initialize("UI/Common/slot_frame.dds", "UI/Common/slot_frame_selected.dds");
	weaponSlot_->InitializeSlotNumbers("UI/Common/numbers02.dds", 50.0f, 50.0f, { 8.0f, 8.0f }, 2.0f, 32, 32);

	const std::array<std::string, WeaponSlot::kSlotCount> weaponIcons = {
		"UI/Icons/primary_icon.dds",
		"UI/Icons/backup_icon.dds",
		"UI/Icons/melee_icon.dds",
		"UI/Icons/special_icon.dds",
		"UI/Icons/sniper_icon.dds",
		"UI/Icons/heavy_icon.dds"
	};
	weaponSlot_->InitializeIcons(weaponIcons);
	weaponSlot_->InitializeAmmoDelimiter("UI/Icons/slash_icon.dds", { 20.0f, 20.0f }, { 0.0f, 0.0f });
	weaponSlot_->InitializeAmmoNumbers("UI/Common/Number.dds", 50, 50, { 10, 10 }, -5.0f, 20.0f, 20.0f);

	/*waveUI_ = std::make_unique<WaveUI>();
	waveUI_->Initialize();
	waveUI_->SetVisible(true);*/

	damageIndicatorManager_ = std::make_unique<DamageIndicatorManager>();
	damageIndicatorManager_->Initialize();

	noAmmoUI_ = std::make_unique<NoAmmoUI>();
	noAmmoUI_->Initialize();

	controlGuideUI_ = std::make_unique<ControlGuideUI>();
	controlGuideUI_->Initialize(
		"UI/Common/ammo_icon.dds",
		"UI/Common/mouse_leftClick.dds",
		"UI/Common/reticle_icon.dds",
		"UI/Common/mouse_rightClick.dds",
		"UI/Common/R_key_icon.dds",
		"UI/Common/reload_icon.dds");
	controlGuideUI_->SetVisible(true);
	controlGuideUI_->SetAnchorTopLeft({ 1500.0f, 930.0f });

	bossHudUI_.Initialize();
	stage1ObjectiveGuideUI_.Initialize();
	SetLegacyPlayerHudVisible(legacyPlayerHudVisible_);
	SetRuntimeWeaponHudVisible(runtimeWeaponHudVisible_); // 新PlayerではReloadCircleとWeaponSlotだけを旧素材から再利用する。
}

void HUDManager::SetLegacyPlayerHudVisible(bool visible)
{
	legacyPlayerHudVisible_ = visible;
	prevReloading_ = false;

	if (hpWidget_) hpWidget_->SetVisible(visible);
	if (crosshair_) crosshair_->SetVisible(visible);
	if (controlGuideUI_) controlGuideUI_->SetVisible(visible);
	if (reloadCircle_) reloadCircle_->SetVisible(runtimeWeaponHudVisible_ || visible);
	if (!visible && noAmmoUI_) noAmmoUI_->SetVisible(false);
}

void HUDManager::SetRuntimeWeaponHudVisible(bool visible)
{
	runtimeWeaponHudVisible_ = visible;
	if (reloadCircle_)
	{
		reloadCircle_->SetVisible(visible || legacyPlayerHudVisible_);
		if (!visible && !legacyPlayerHudVisible_) reloadCircle_->SetReloading(false, 0.0f);
	}
}

IPlayerRuntime* HUDManager::ResolvePlayerRuntime() const
{
	return playerRuntime_ ? playerRuntime_ : IPlayerRuntime::GetActiveRuntime();
}

/// -------------------------------------------------------------
///                    更新処理
/// -------------------------------------------------------------
void HUDManager::Update(float deltaTime)
{
	bool isReloadingForHUD = false;
	if (runtimeWeaponHudVisible_ && ResolvePlayerRuntime())
	{
		isReloadingForHUD = UpdateReloadCircleFromRuntime();
		UpdateWeaponSlotFromRuntime();
	}
	else if (legacyPlayerHudVisible_)
	{
		isReloadingForHUD = UpdateReloadCircleFromPlayer();
		UpdateWeaponSlotFromPlayer();
	}

	if ((runtimeWeaponHudVisible_ || legacyPlayerHudVisible_) && reloadCircle_) reloadCircle_->Update();

	if (legacyPlayerHudVisible_)
	{
		UpdateCrosshairFromPlayer(isReloadingForHUD);
		if (hpWidget_) hpWidget_->Update();
		if (crosshair_) crosshair_->Update();
		UpdateNoAmmoFromPlayer(deltaTime);
		if (controlGuideUI_) controlGuideUI_->Update(deltaTime);
	}

	// World側フィードバックは新Player HUDへ切り替えた後もHUDManagerで継続する。
	if (damageIndicatorManager_) damageIndicatorManager_->Update(deltaTime);
	if (waveUI_) waveUI_->Update(deltaTime);
	bossHudUI_.Update(deltaTime);
	stage1ObjectiveGuideUI_.Update(deltaTime);
}

bool HUDManager::UpdateReloadCircleFromRuntime()
{
	IPlayerRuntime* runtime = ResolvePlayerRuntime();
	if (!reloadCircle_ || !runtime)
	{
		if (reloadCircle_) reloadCircle_->SetReloading(false, 0.0f);
		return false;
	}

	const bool isReloading = runtime->IsReloading();
	const float reloadDuration = runtime->GetReloadDuration();
	const float progress01 = isReloading && reloadDuration > 1.0e-6f
		? std::clamp(runtime->GetReloadTimer() / reloadDuration, 0.0f, 1.0f)
		: 0.0f;

	reloadCircle_->SetVisible(runtimeWeaponHudVisible_ || legacyPlayerHudVisible_);
	reloadCircle_->SetReloading(isReloading, progress01); // 新WeaponComponentは経過時間を正本として0→1へ進める。
	prevReloading_ = isReloading;
	return isReloading;
}

void HUDManager::UpdateWeaponSlotFromRuntime()
{
	IPlayerRuntime* runtime = ResolvePlayerRuntime();
	if (!weaponSlot_ || !runtime) return;

	WeaponSlot::HudSnapshot snapshot{};
	int selectedSlot = runtime->GetSelectedWeaponSlot();
	int slotCount = std::clamp(runtime->GetWeaponSlotCount(), 1, WeaponSlot::kSlotCount);

	if (const auto* actor = dynamic_cast<const Ken4lowEngine::PlayerActor*>(runtime))
	{
		if (const auto* inventory = actor->GetInventoryComponent())
		{
			selectedSlot = inventory->GetSelectedSlot();
			slotCount = std::min(static_cast<int>(inventory->GetSlots().size()), WeaponSlot::kSlotCount);
			for (int i = 0; i < slotCount; ++i)
			{
				if (inventory->GetSlots()[i] < 0) continue;
				snapshot.slotStates[i].useAmmo = (i == selectedSlot);
			}
		}
	}
	else
	{
		for (int i = 0; i < slotCount; ++i)
		{
			if (runtime->GetWeaponIdForSlot(i) < 0) continue;
			snapshot.slotStates[i].useAmmo = (i == selectedSlot);
		}
	}

	snapshot.selectedIndex = std::clamp(selectedSlot, 0, WeaponSlot::kSlotCount - 1);
	if (snapshot.selectedIndex >= 0 && snapshot.selectedIndex < WeaponSlot::kSlotCount)
	{
		auto& ammo = snapshot.slotStates[snapshot.selectedIndex].ammoInfo;
		ammo.currentAmmo = runtime->GetMagazineAmmo();
		ammo.reserveAmmo = runtime->GetReserveAmmo();
	}
	weaponSlot_->Update(snapshot); // 選択枠と弾薬だけを新Inventory/Weapon状態から同期する。
}

bool HUDManager::UpdateReloadCircleFromPlayer()
{
	if (!reloadCircle_) return false;

	bool isReloading = false;
	float reloadTimer = 0.0f;
	float reloadSec = 0.0f;
	const bool hasInfo = (player_ != nullptr) && player_->GetReloadUI(isReloading, reloadTimer, reloadSec);
	if (!hasInfo || reloadSec <= 1e-6f)
	{
		reloadCircle_->SetReloading(false, 0.0f);
		prevReloading_ = false;
		return false;
	}

	if (isReloading && !prevReloading_) reloadTimerIsRemaining_ = (reloadTimer > reloadSec * 0.5f);
	float progress01 = 0.0f;
	if (isReloading)
	{
		const float t = std::clamp(reloadTimer / reloadSec, 0.0f, 1.0f);
		progress01 = reloadTimerIsRemaining_ ? (1.0f - t) : t;
	}

	reloadCircle_->SetReloading(isReloading, progress01);
	prevReloading_ = isReloading;
	return isReloading;
}

void HUDManager::UpdateCrosshairFromPlayer(bool isReloadingForHUD)
{
	if (!crosshair_ || !player_) return;

	FWeaponReticleData r{};
	float spread = 0.0f;
	bool isADS = false;
	if (!player_->GetReticleUI(r, spread, isADS)) return;

	crosshair_->SetReticleType(static_cast<int>(r.reticleType));
	crosshair_->SetReticleTexture(r.reticleTexturePath);
	crosshair_->SetBaseSize(r.reticleBaseSize);
	crosshair_->SetMaxSize(r.reticleMaxSize);
	crosshair_->SetExpandPerShot(r.reticleExpandPerShot);
	crosshair_->SetRecoverSpeed(r.reticleRecoverSpeed);
	crosshair_->SetSpreadValue(spread);
	crosshair_->SetMoveExpandEnabled(r.bEnableMoveReticleExpand);
	crosshair_->SetMoveExpandMultipliers(r.moveExpandMultiplier, r.sprintExpandMultiplier, r.airExpandMultiplier, r.landExpandImpulse);
	crosshair_->SetHideInADS(r.bHideReticleInADS);
	crosshair_->SetADSState(isADS);
	crosshair_->SetHideWhileReload(true);
	crosshair_->SetReloadState(isReloadingForHUD);
	crosshair_->SetUseADSReticleOverride(r.bUseAdsReticleOverride);
	crosshair_->SetADSReticleTexture(r.adsReticleTexturePath);
	crosshair_->SetUseADSCenterDot(r.bUseAdsCenterDot);
	crosshair_->SetADSCenterDotTexture(r.adsCenterDotTexturePath);
	crosshair_->SetADSBlendTime(r.adsReticleBlendTime);
	crosshair_->SetShowHitMarker(r.bShowHitMarker);
	crosshair_->SetHitMarkerTexture(r.hitMarkerTexturePath);
	crosshair_->SetUseHeadshotMarker(r.bUseHeadshotMarker);
	crosshair_->SetHeadshotHitMarkerTexture(r.headshotHitMarkerTexturePath);
	crosshair_->SetUseKillConfirmMarker(r.bUseKillConfirmMarker);
	crosshair_->SetKillConfirmMarkerTexture(r.killConfirmMarkerTexturePath);
	crosshair_->SetHitMarkerDuration(r.hitMarkerDuration);
	crosshair_->SetKillConfirmDuration(r.killConfirmDuration);
}

void HUDManager::UpdateWeaponSlotFromPlayer()
{
	if (!weaponSlot_ || !player_) return;

	WeaponSlot::HudSnapshot snap{};
	if (player_->GetWeaponSlotHUD(snap)) weaponSlot_->Update(snap);
}

void HUDManager::UpdateNoAmmoFromPlayer(float deltaTime)
{
	if (!noAmmoUI_ || !player_) return;

	bool showNoAmmo = false;
	player_->GetNoAmmoUI(showNoAmmo);
	noAmmoUI_->SetVisible(showNoAmmo);
	noAmmoUI_->Update(deltaTime);
}

/// -------------------------------------------------------------
///                    描画処理
/// -------------------------------------------------------------
void HUDManager::Draw()
{
	if (legacyPlayerHudVisible_)
	{
		if (hpWidget_ && hpWidget_->IsVisible()) hpWidget_->Draw();
		if (crosshair_ && crosshair_->IsVisible()) crosshair_->Draw();
		if (noAmmoUI_ && noAmmoUI_->IsVisible()) noAmmoUI_->Draw();
		if (controlGuideUI_ && controlGuideUI_->IsVisible()) controlGuideUI_->Draw();
	}

	if (runtimeWeaponHudVisible_ || legacyPlayerHudVisible_)
	{
		if (reloadCircle_ && reloadCircle_->IsVisible()) reloadCircle_->Draw();
		if (weaponSlot_) weaponSlot_->Draw();
	}

	if (waveUI_ && IsWaveUIDrawEnabled()) waveUI_->Draw();
	stage1ObjectiveGuideUI_.Draw();
	bossHudUI_.Draw();

	// 被弾方向表示は新PlayerHudPresenterに未移行のためWorld HUDとして残す。
	if (damageIndicatorManager_) damageIndicatorManager_->Draw();
}

void HUDManager::SetHP(float hp, float maxHp)
{
	if (hpWidget_) hpWidget_->SetHP(hp, maxHp);
}

void HUDManager::SetBossHP(float hp, float maxHp, bool bossBattleActive)
{
	bossHudUI_.SetBossHP(hp, maxHp, bossBattleActive);
}

void HUDManager::SetStage1ObjectiveGuide(bool enabled, int destroyedCrystals, int totalCrystals, bool bossBattleActive, bool bossDefeated, bool tutorialActive)
{
	stage1ObjectiveGuideUI_.SetGuide(enabled, destroyedCrystals, totalCrystals, bossBattleActive, bossDefeated, tutorialActive);
}

void HUDManager::SetStage1ObjectiveTutorialAlpha(float alpha) { stage1ObjectiveGuideUI_.SetTutorialAlpha(alpha); }
void HUDManager::SetStage1ObjectiveTutorialPage(int page) { stage1ObjectiveGuideUI_.SetTutorialPage(page); }
void HUDManager::SetStage1ObjectiveTutorialProgress(float progress) { stage1ObjectiveGuideUI_.SetTutorialProgress(progress); }
void HUDManager::SetStage1TutorialItemMarker(int markerIndex, bool visible, const K4E::Vector2& screenPosition, int itemType) { stage1ObjectiveGuideUI_.SetTutorialItemMarker(markerIndex, visible, screenPosition, itemType); }
void HUDManager::NotifyStage1ObjectiveGuideStarted() { stage1ObjectiveGuideUI_.NotifyGuideStarted(); }
void HUDManager::NotifyStage1BossAppeared() { stage1ObjectiveGuideUI_.NotifyBossAppeared(); }

void HUDManager::SetBossGuide(const K4E::Vector3& playerPos, const K4E::Vector3& bossPos, const K4E::Vector3& cameraForward, bool bossBattleActive)
{
	bossHudUI_.SetBossGuide(playerPos, bossPos, cameraForward, bossBattleActive);
}

void HUDManager::NotifyBossIntroCompleted(const K4E::Vector3& bossPos) { bossHudUI_.NotifyBossIntroCompleted(bossPos); }

void HUDManager::SetWeaponSlotVisibleSlotCount(int count)
{
	if (weaponSlot_) weaponSlot_->SetVisibleSlotCount(count);
}

void HUDManager::NotifyPlayerHit(float strength01)
{
	if (hpWidget_) hpWidget_->NotifyHit(strength01);
}

void HUDManager::NotifyEnemyHit(bool isHeadshot)
{
	if (crosshair_) crosshair_->NotifyEnemyHit(isHeadshot, false);
}

void HUDManager::NotifyEnemyKill(bool isHeadshot)
{
	if (crosshair_) crosshair_->NotifyEnemyHit(isHeadshot, true);
}

void HUDManager::SetCrosshairMovementState(bool isMoving, bool isSprinting, bool isAirborne)
{
	if (crosshair_) crosshair_->SetMovementState(isMoving, isSprinting, isAirborne);
}

void HUDManager::NotifyCrosshairLanded()
{
	if (crosshair_) crosshair_->NotifyLanded();
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

bool HUDManager::IsWaveUIDrawEnabled() const
{
	if (bossHudUI_.ShouldHideWaveUI()) return false;
	return waveUI_ && waveUI_->IsVisible();
}

void HUDManager::AddDamageIndicator(const K4E::Vector3& playerPos, const K4E::Vector3& attackerPos, const K4E::Vector3& cameraForward, const K4E::Vector3& cameraRight)
{
	if (damageIndicatorManager_) damageIndicatorManager_->AddIndicator(playerPos, attackerPos, cameraForward, cameraRight);
}

void HUDManager::SetCrosshairTargetingEnemy(bool v)
{
	if (crosshair_) crosshair_->SetTargetingEnemy(v);
}

void HUDManager::SetCrosshairTargetColors(const K4E::Vector4& normalColor, const K4E::Vector4& targetColor)
{
	if (crosshair_) crosshair_->SetTargetColors(normalColor, targetColor);
}