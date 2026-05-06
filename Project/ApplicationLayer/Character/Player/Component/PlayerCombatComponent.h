#pragma once

#include <functional>
#include "PlayerInputSnapshot.h"
#include "Segment.h"

namespace Ken4lowEngine { class Camera; }
namespace K4E = ::Ken4lowEngine;

class BulletManager;
class CollisionManager;
class PlayerWeaponComponent;
class PlayerWeaponVisualComponent;
class PlayerViewComponent;

/// -------------------------------------------------------------
/// PlayerCombatComponent
/// - 発射処理、リコイル、ショットデバッグの責務を持つ
/// - 「被弾」はまだ Player に残し、まずは攻撃側だけを分離する
/// -------------------------------------------------------------
class PlayerCombatComponent
{
public:
	void BindDependencies(PlayerWeaponComponent* weapon, PlayerViewComponent* view);
	void BindWeaponVisual(PlayerWeaponVisualComponent* weaponVisual) { weaponVisual_ = weaponVisual; }

	// Player の音コールバックをそのまま受け取る
	void SetAudioCallbacks(std::function<void()>* onFire, std::function<void()>* onReload);

	// デバッグ線タイマー更新
	void Tick(float deltaTime);

	// CombatFSM から呼ばれる「1発撃つ」処理
	void FireOnce(
		const InputSnapshot& input,
		BulletManager* bulletManager,
		CollisionManager* collisionManager,
		const std::function<void(const K4E::Segment&)>& setDebugSegment);

	// ImGui から調整
	void DrawImGui();

private:
	PlayerWeaponComponent* weapon_ = nullptr;
	PlayerViewComponent* view_ = nullptr;
	PlayerWeaponVisualComponent* weaponVisual_ = nullptr;

	std::function<void()>* onFireSE_ = nullptr;
	std::function<void()>* onReloadSE_ = nullptr;

	// ---- 射撃/リコイル調整 ----
	float recoilPitchDegHip_ = 0.35f;
	float recoilYawDegHip_ = 0.75f;
	float recoilPitchDegAds_ = 0.18f;
	float recoilYawDegAds_ = 0.25f;

	// ---- ヒットスキャン/ショットデバッグ ----
	float hitscanRange_ = 200.0f;
	float shotDebugTimer_ = 0.0f;
};
