#define NOMINMAX
#include "HUDManager.h"

#include "ApplicationLayer/Character/Player/Actor/PlayerActor.h"
#include "ApplicationLayer/Character/Player/IPlayerRuntime.h"
#include "FontAtlasLoader.h"
#include "TextSpriteDrawer.h"

#include <Sprite.h>

#include <algorithm>
#include <array>
#include <string>

namespace K4E = ::Ken4lowEngine;

namespace
{
	constexpr K4E::Vector2 kObjectiveCenter{ 250.0f, 104.0f };
	constexpr K4E::Vector2 kObjectivePanelSize{ 430.0f, 86.0f };
	constexpr float kObjectiveProgressWidth = 360.0f;
}

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
	InitializeStageObjectiveUI();
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
	UpdateStageObjectiveUI(deltaTime);
}

void HUDManager::Draw()
{
	if (runtimeWeaponHudVisible_)
	{
		if (reloadCircle_ && reloadCircle_->IsVisible()) reloadCircle_->Draw();
		if (weaponSlot_) weaponSlot_->Draw();
	}
	if (waveUI_ && IsWaveUIDrawEnabled()) waveUI_->Draw();
	DrawStageObjectiveUI();
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

void HUDManager::InitializeStageObjectiveUI()
{
	stageObjectiveBackSprite_ = std::make_unique<K4E::Sprite>();
	stageObjectiveBackSprite_->Initialize("Effects/white.dds");
	stageObjectiveBackSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	stageObjectiveAccentSprite_ = std::make_unique<K4E::Sprite>();
	stageObjectiveAccentSprite_->Initialize("Effects/white.dds");
	stageObjectiveAccentSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	stageObjectiveProgressBackSprite_ = std::make_unique<K4E::Sprite>();
	stageObjectiveProgressBackSprite_->Initialize("Effects/white.dds");
	stageObjectiveProgressBackSprite_->SetAnchorPoint({ 0.0f, 0.5f });

	stageObjectiveProgressFillSprite_ = std::make_unique<K4E::Sprite>();
	stageObjectiveProgressFillSprite_->Initialize("Effects/white.dds");
	stageObjectiveProgressFillSprite_->SetAnchorPoint({ 0.0f, 0.5f });

	stageObjectiveTextDrawer_ = std::make_unique<K4E::TextSpriteDrawer>();
	stageObjectiveTextReady_ = false;
	try
	{
		auto fontDefJP = K4E::FontAtlasLoader::LoadFromJson(
			"UI/Font/JP/DotGothic16-Regular_atlas.dds",
			"Resources/Fonts/Compiled/JP/DotGothic16-Regular.json",
			32.0f,
			32.0f,
			U'?');
		stageObjectiveTextDrawer_->Initialize(fontDefJP);
		stageObjectiveTextReady_ = true;
	}
	catch (...)
	{
		stageObjectiveTextReady_ = false;
	}
}

void HUDManager::UpdateStageObjectiveUI(float deltaTime)
{
	const float targetAlpha = stageObjectiveDisplayState_.visible ? 0.82f : 0.0f;
	const float approach = std::clamp(deltaTime * 8.0f, 0.0f, 1.0f);
	stageObjectiveAlpha_ += (targetAlpha - stageObjectiveAlpha_) * approach;

	K4E::Vector4 accentColor{ 0.35f, 0.86f, 1.0f, stageObjectiveAlpha_ };
	if (stageObjectiveDisplayState_.cleared) accentColor = { 0.38f, 1.0f, 0.55f, stageObjectiveAlpha_ };
	else if (stageObjectiveDisplayState_.failed) accentColor = { 1.0f, 0.36f, 0.32f, stageObjectiveAlpha_ };

	if (stageObjectiveBackSprite_)
	{
		stageObjectiveBackSprite_->SetPosition(kObjectiveCenter);
		stageObjectiveBackSprite_->SetSize(kObjectivePanelSize);
		stageObjectiveBackSprite_->SetColor({ 0.03f, 0.035f, 0.045f, stageObjectiveAlpha_ * 0.86f });
		stageObjectiveBackSprite_->Update();
	}
	if (stageObjectiveAccentSprite_)
	{
		stageObjectiveAccentSprite_->SetPosition({ kObjectiveCenter.x, kObjectiveCenter.y + kObjectivePanelSize.y * 0.5f - 4.0f });
		stageObjectiveAccentSprite_->SetSize({ kObjectivePanelSize.x * 0.92f, 4.0f });
		stageObjectiveAccentSprite_->SetColor(accentColor);
		stageObjectiveAccentSprite_->Update();
	}

	const float progress = std::clamp(stageObjectiveDisplayState_.normalizedProgress, 0.0f, 1.0f);
	const K4E::Vector2 progressPosition{ kObjectiveCenter.x - kObjectiveProgressWidth * 0.5f, kObjectiveCenter.y + 29.0f };
	if (stageObjectiveProgressBackSprite_)
	{
		stageObjectiveProgressBackSprite_->SetPosition(progressPosition);
		stageObjectiveProgressBackSprite_->SetSize({ kObjectiveProgressWidth, 5.0f });
		stageObjectiveProgressBackSprite_->SetColor({ 0.12f, 0.14f, 0.18f, stageObjectiveDisplayState_.showProgress ? stageObjectiveAlpha_ : 0.0f });
		stageObjectiveProgressBackSprite_->Update();
	}
	if (stageObjectiveProgressFillSprite_)
	{
		stageObjectiveProgressFillSprite_->SetPosition(progressPosition);
		stageObjectiveProgressFillSprite_->SetSize({ kObjectiveProgressWidth * progress, 5.0f });
		stageObjectiveProgressFillSprite_->SetColor({ accentColor.x, accentColor.y, accentColor.z, stageObjectiveDisplayState_.showProgress ? stageObjectiveAlpha_ : 0.0f });
		stageObjectiveProgressFillSprite_->Update();
	}
}

void HUDManager::DrawStageObjectiveUI()
{
	if (!stageObjectiveDisplayState_.visible || stageObjectiveAlpha_ <= 0.01f)
	{
		return;
	}

	if (stageObjectiveBackSprite_) stageObjectiveBackSprite_->Draw();
	if (stageObjectiveAccentSprite_) stageObjectiveAccentSprite_->Draw();
	if (stageObjectiveDisplayState_.showProgress)
	{
		if (stageObjectiveProgressBackSprite_) stageObjectiveProgressBackSprite_->Draw();
		if (stageObjectiveProgressFillSprite_) stageObjectiveProgressFillSprite_->Draw();
	}
	if (!stageObjectiveTextReady_ || !stageObjectiveTextDrawer_)
	{
		return;
	}

	stageObjectiveTextDrawer_->Reset();
	stageObjectiveTextDrawer_->SetLetterSpacing(1.0f);
	stageObjectiveTextDrawer_->SetScale(0.66f);
	stageObjectiveTextDrawer_->SetColor({ 0.96f, 0.98f, 1.0f, stageObjectiveAlpha_ });
	stageObjectiveTextDrawer_->DrawTextCentered(stageObjectiveDisplayState_.title, { kObjectiveCenter.x, kObjectiveCenter.y - 18.0f });
	stageObjectiveTextDrawer_->SetScale(0.48f);
	stageObjectiveTextDrawer_->SetColor({ 0.58f, 0.90f, 1.0f, stageObjectiveAlpha_ });
	stageObjectiveTextDrawer_->DrawTextCentered(stageObjectiveDisplayState_.detail, { kObjectiveCenter.x, kObjectiveCenter.y + 8.0f });
}

void HUDManager::SetStageObjectiveDisplayState(const StageObjectiveDisplayState& state)
{
	stageObjectiveDisplayState_ = state;
	stageObjectiveDisplayState_.normalizedProgress = std::clamp(state.normalizedProgress, 0.0f, 1.0f); // Stage別実装から範囲外の進捗が来てもHUD形状を崩さない。
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
