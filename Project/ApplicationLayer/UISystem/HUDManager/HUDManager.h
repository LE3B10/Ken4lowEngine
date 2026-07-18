#pragma once

#include "BossHudUI.h"
#include "Crosshair.h"
#include "DamageIndicatorManager.h"
#include "HPWidget.h"
#include "ReloadCircle.h"
#include "Stage1ObjectiveGuideUI.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "WaveUI.h"
#include "WeaponSlot.h"

#include <memory>

class IPlayerRuntime;

/// World/Tutorial/Boss表示とPlayerActorの武器HUDを管理する。
class HUDManager
{
public:
	~HUDManager();

	void Initialize();
	void Update(float deltaTime);
	void Draw();

	void SetPlayerRuntime(IPlayerRuntime* playerRuntime) { playerRuntime_ = playerRuntime; }
	void SetRuntimeWeaponHudVisible(bool visible);
	bool IsRuntimeWeaponHudVisible() const { return runtimeWeaponHudVisible_; }

	void SetBossHP(float hp, float maxHp, bool bossBattleActive);
	void SetStage1ObjectiveGuide(bool enabled, int destroyedCrystals, int totalCrystals, bool bossBattleActive, bool bossDefeated, bool tutorialActive);
	void SetStage1ObjectiveTutorialAlpha(float alpha);
	void SetStage1ObjectiveTutorialPage(int page);
	void SetStage1ObjectiveTutorialProgress(float progress);
	void SetStage1TutorialItemMarker(int markerIndex, bool visible, const Ken4lowEngine::Vector2& screenPosition, int itemType);
	void NotifyStage1ObjectiveGuideStarted();
	void NotifyStage1BossAppeared();
	void SetBossGuide(const Ken4lowEngine::Vector3& playerPos, const Ken4lowEngine::Vector3& bossPos, const Ken4lowEngine::Vector3& cameraForward, bool bossBattleActive);
	void NotifyBossIntroCompleted(const Ken4lowEngine::Vector3& bossPos);
	void SetWeaponSlotVisibleSlotCount(int count);

	void SetWaveDisplayState(const WaveUI::DisplayState& state);
	void NotifyWaveStarted(int waveNumber, bool isFinalWave);
	void NotifyAllWavesCleared();
	void SetWaveUIVisible(bool visible);
	bool IsWaveUIDrawEnabled() const;

	void AddDamageIndicator(const Ken4lowEngine::Vector3& playerPos,
		const Ken4lowEngine::Vector3& attackerPos,
		const Ken4lowEngine::Vector3& cameraForward,
		const Ken4lowEngine::Vector3& cameraRight);

	void SetReloadCircleVisible(bool visible) { if (reloadCircle_) reloadCircle_->SetVisible(visible); }
	ReloadCircle* GetReloadCircle() const { return reloadCircle_.get(); }
	WeaponSlot* GetWeaponSlot() const { return weaponSlot_.get(); }
	bool IsBossHPBarDrawEnabled() const { return bossHudUI_.IsHpBarDrawEnabled(); }

	// Editorの旧HUD状態表示はAPI互換だけ維持し、PlayerActor UIとの二重確保は行わない。
	HPWidget* GetHPWidget() const { return nullptr; }
	Crosshair* GetCrosshair() const { return nullptr; }
	void SetHP(float, float) {}
	void NotifyPlayerHit(float = 1.0f) {}
	void NotifyEnemyHit(bool = false) {}
	void NotifyEnemyKill(bool = false) {}
	void SetCrosshairMovementState(bool, bool, bool) {}
	void NotifyCrosshairLanded() {}
	void SetCrosshairTargetingEnemy(bool) {}
	void SetCrosshairTargetColors(const Ken4lowEngine::Vector4&, const Ken4lowEngine::Vector4&) {}
	void SetCrosshairVisible(bool) {}
	void SetHPVisible(bool) {}
	void SetControlGuideVisible(bool) {}

private:
	bool UpdateReloadCircleFromRuntime();
	void UpdateWeaponSlotFromRuntime();
	IPlayerRuntime* ResolvePlayerRuntime() const;

	IPlayerRuntime* playerRuntime_ = nullptr;
	bool runtimeWeaponHudVisible_ = true;
	bool prevReloading_ = false;

	std::unique_ptr<ReloadCircle> reloadCircle_;
	std::unique_ptr<WeaponSlot> weaponSlot_;
	std::unique_ptr<WaveUI> waveUI_;
	std::unique_ptr<DamageIndicatorManager> damageIndicatorManager_;
	BossHudUI bossHudUI_;
	Stage1ObjectiveGuideUI stage1ObjectiveGuideUI_;
};
