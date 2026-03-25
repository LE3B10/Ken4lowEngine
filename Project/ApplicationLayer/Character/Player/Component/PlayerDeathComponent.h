#pragma once
#include <functional>

#include "PlayerInputSnapshot.h"
#include "Vector3.h"


namespace K4E = ::Ken4lowEngine;

class Player;
class HUDManager;
class PlayerMotorComponent;
class PlayerViewComponent;
class PlayerWeaponVisualComponent;
class PlayerHurtboxComponent;
class PlayerVfx;

class PlayerDeathComponent
{
public:
	void Start(
		Player& owner,
		const K4E::Vector3& launchDirWorld,
		PlayerViewComponent& view,
		PlayerWeaponVisualComponent& weaponVisual,
		InputSnapshot& inputSnap,
		bool& runCarry,
		const std::function<void()>& cancelReloadFn);

	void Update(
		Player& owner,
		float deltaTime,
		PlayerMotorComponent& motor,
		PlayerViewComponent& view,
		PlayerWeaponVisualComponent& weaponVisual,
		PlayerHurtboxComponent& hurtbox,
		PlayerVfx& vfx,
		HUDManager* hudManager,
		float hp,
		float maxHp);

	bool IsActive() const { return active_; }
	bool IsFinished() const { return finished_; }
	bool IsGameOverReady() const { return gameOverReady_; }

	bool ConsumeGameOverReady()
	{
		if (!gameOverReady_ || gameOverNotified_)
		{
			return false;
		}
		gameOverNotified_ = true;
		return true;
	}

private:
	bool active_ = false;
	bool finished_ = false;
	bool settled_ = false;
	float timer_ = 0.0f;

	K4E::Vector3 velocity_{ 0.0f, 0.0f, 0.0f };
	K4E::Vector3 launchDir_{ 0.0f, 0.0f, -1.0f };
	K4E::Vector3 startRotate_{ 0.0f, 0.0f, 0.0f };

	float duration_ = 1.25f;
	float gravity_ = 24.0f;
	float groundFriction_ = 8.0f;
	float launchHorizontal_ = 7.5f;
	float launchUp_ = 5.0f;
	float tiltTime_ = 0.28f;
	float maxPitchRad_ = 0.35f;
	float maxRollRad_ = 1.10f;

	bool gameOverReady_ = false;
	bool gameOverNotified_ = false;
};
