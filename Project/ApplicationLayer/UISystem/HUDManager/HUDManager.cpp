#define NOMINMAX
#include "HUDManager.h"

#include "ApplicationLayer/Character/Player/Actor/PlayerActor.h"
#include "ApplicationLayer/Character/Player/IPlayerRuntime.h"

#include <algorithm>
#include <array>
#include <string>

HUDManager::~HUDManager() = default;

void HUDManager::Initialize()
{
	reloadCircle_ = std::make_unique<ReloadCircle>();
	reloadCircle_->Initialize("UI/Common/reload-circle.dds");

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

	damageIndicatorManager_ = std::make_unique<DamageIndicatorManager>();
	damageIndicatorManager_->Initialize();
	bossHudUI_.Initialize();
	stage1ObjectiveGuideUI_.Initialize();
	SetRuntimeWeaponHudVisible(runtimeWeaponHudVisible_); // 旧Player用HUDを生成せず、現役Runtime表示だけ初期化する。
}

void HUDManager::Update(float deltaTime)
{
	if (runtimeWeaponHudVisible_ && ResolvePlayerRuntime())
	{
		UpdateReloadCircleFromRuntime();
		UpdateWeaponSlotFromRuntime();
	}
	else if (reloadCircle_)
	{
		reloadCircle_->SetReloading(false, 0.0f);
		prevReloading_ = false;
	}

	if (runtimeWeaponHudVisible_ && reloadCircle_) reloadCircle_->Update();
	if (damageIndicatorManager_) damageIndicatorManager_->Update(deltaTime);
	if (waveUI_) waveUI_->Update(deltaTime);
	bossHudUI_.Update(deltaTime);
	stage1ObjectiveGuideUI_.Update(deltaTime);
}

void HUDManager::Draw()
{
	if (runtimeWeaponHudVisible_)
	{
		if (reloadCircle_ && reloadCircle_->IsVisible()) reloadCircle_->Draw();
		if (weaponSlot_) weaponSlot_->Draw();
	}
	if (waveUI_ && IsWaveUIDrawEnabled()) waveUI_->Draw();
	stage1ObjectiveGuideUI_.Draw();
	bossHudUI_.Draw();
	if (damageIndicatorManager_) damageIndicatorManager_->Draw();
}

void HUDManager::SetRuntimeWeaponHudVisible(bool visible)
{
	runtimeWeaponHudVisible_ = visible;
	if (!reloadCircle_) return;
	reloadCircle_->SetVisible(visible);
	if (!visible) reloadCircle_->SetReloading(false, 0.0f);
}

IPlayerRuntime* HUDManager::ResolvePlayerRuntime() const
{
	return playerRuntime_ ? playerRuntime_ : IPlayerRuntime::GetActiveRuntime();
}

bool HUDManager::UpdateReloadCircleFromRuntime()
{
	IPlayerRuntime* runtime = ResolvePlayerRuntime();
	if (!reloadCircle_ || !runtime)
	{
		if (reloadCircle_) reloadCircle_->SetReloading(false, 0.0f);
		return false;
	}
	const bool reloading = runtime->IsReloading();
	const float duration = runtime->GetReloadDuration();
	const float progress = reloading && duration > 1.0e-6f
		? std::clamp(runtime->GetReloadTimer() / duration, 0.0f, 1.0f)
		: 0.0f;
	reloadCircle_->SetVisible(runtimeWeaponHudVisible_);
	reloadCircle_->SetReloading(reloading, progress);
	prevReloading_ = reloading;
	return reloading;
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
				snapshot.slotStates[i].useAmmo = i == selectedSlot;
			}
		}
	}
	else
	{
		for (int i = 0; i < slotCount; ++i)
		{
			if (runtime->GetWeaponIdForSlot(i) < 0) continue;
			snapshot.slotStates[i].useAmmo = i == selectedSlot;
		}
	}

	snapshot.selectedIndex = std::clamp(selectedSlot, 0, WeaponSlot::kSlotCount - 1);
	auto& ammo = snapshot.slotStates[snapshot.selectedIndex].ammoInfo;
	ammo.currentAmmo = runtime->GetMagazineAmmo();
	ammo.reserveAmmo = runtime->GetReserveAmmo();
	weaponSlot_->Update(snapshot);
}

void HUDManager::SetBossHP(float hp, float maxHp, bool bossBattleActive) { bossHudUI_.SetBossHP(hp, maxHp, bossBattleActive); }
void HUDManager::SetStage1ObjectiveGuide(bool enabled, int destroyedCrystals, int totalCrystals, bool bossBattleActive, bool bossDefeated, bool tutorialActive) { stage1ObjectiveGuideUI_.SetGuide(enabled, destroyedCrystals, totalCrystals, bossBattleActive, bossDefeated, tutorialActive); }
void HUDManager::SetStage1ObjectiveTutorialAlpha(float alpha) { stage1ObjectiveGuideUI_.SetTutorialAlpha(alpha); }
void HUDManager::SetStage1ObjectiveTutorialPage(int page) { stage1ObjectiveGuideUI_.SetTutorialPage(page); }
void HUDManager::SetStage1ObjectiveTutorialProgress(float progress) { stage1ObjectiveGuideUI_.SetTutorialProgress(progress); }
void HUDManager::SetStage1TutorialItemMarker(int markerIndex, bool visible, const Ken4lowEngine::Vector2& screenPosition, int itemType) { stage1ObjectiveGuideUI_.SetTutorialItemMarker(markerIndex, visible, screenPosition, itemType); }
void HUDManager::NotifyStage1ObjectiveGuideStarted() { stage1ObjectiveGuideUI_.NotifyGuideStarted(); }
void HUDManager::NotifyStage1BossAppeared() { stage1ObjectiveGuideUI_.NotifyBossAppeared(); }
void HUDManager::SetBossGuide(const Ken4lowEngine::Vector3& playerPos, const Ken4lowEngine::Vector3& bossPos, const Ken4lowEngine::Vector3& cameraForward, bool bossBattleActive) { bossHudUI_.SetBossGuide(playerPos, bossPos, cameraForward, bossBattleActive); }
void HUDManager::NotifyBossIntroCompleted(const Ken4lowEngine::Vector3& bossPos) { bossHudUI_.NotifyBossIntroCompleted(bossPos); }
void HUDManager::SetWeaponSlotVisibleSlotCount(int count) { if (weaponSlot_) weaponSlot_->SetVisibleSlotCount(count); }
void HUDManager::SetWaveDisplayState(const WaveUI::DisplayState& state) { if (waveUI_) waveUI_->SetDisplayState(state); }
void HUDManager::NotifyWaveStarted(int waveNumber, bool isFinalWave) { if (waveUI_) waveUI_->NotifyWaveStarted(waveNumber, isFinalWave); }
void HUDManager::NotifyAllWavesCleared() { if (waveUI_) waveUI_->NotifyAllWavesCleared(); }
void HUDManager::SetWaveUIVisible(bool visible) { if (waveUI_) waveUI_->SetVisible(visible); }
bool HUDManager::IsWaveUIDrawEnabled() const { return !bossHudUI_.ShouldHideWaveUI() && waveUI_ && waveUI_->IsVisible(); }
void HUDManager::AddDamageIndicator(const Ken4lowEngine::Vector3& playerPos, const Ken4lowEngine::Vector3& attackerPos, const Ken4lowEngine::Vector3& cameraForward, const Ken4lowEngine::Vector3& cameraRight)
{
	if (damageIndicatorManager_) damageIndicatorManager_->AddIndicator(playerPos, attackerPos, cameraForward, cameraRight);
}
