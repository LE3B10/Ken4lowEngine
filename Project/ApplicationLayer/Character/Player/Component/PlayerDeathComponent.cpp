#define NOMINMAX
#include "PlayerDeathComponent.h"

#include "Player.h"
#include "HUDManager.h"
#include "PlayerDamageCollider.h"
#include "PlayerMotorComponent.h"
#include "PlayerVfx.h"
#include "PlayerViewComponent.h"
#include "PlayerWeaponVisualComponent.h"

void PlayerDeathComponent::Start(
	Player& owner,
	const K4E::Vector3& launchDirWorld,
	PlayerViewComponent& view,
	PlayerWeaponVisualComponent& weaponVisual,
	InputSnapshot& inputSnap,
	bool& runCarry,
	const std::function<void()>& cancelReloadFn)
{
	if (active_)
	{
		return;
	}

	active_ = true;
	finished_ = false;
	settled_ = false;
	timer_ = 0.0f;
	gameOverReady_ = false;
	gameOverNotified_ = false;

	inputSnap = InputSnapshot{};
	if (cancelReloadFn)
	{
		cancelReloadFn();
	}
	weaponVisual.ForceRefresh();

	// 一人称で隠していた本体パーツを全部戻す
	owner.SetBodyActive(true);
	owner.SetAllPartsActive(true);

	auto* tr = owner.GetWorldTransform();
	if (!tr)
	{
		return;
	}

	startRotate_ = tr->rotate_;

	launchDir_ = launchDirWorld;
	launchDir_.y = 0.0f;

	if (K4E::Vector3::Length(launchDir_) <= 0.0001f)
	{
		launchDir_ = { 0.0f, 0.0f, -1.0f };
	}
	else
	{
		launchDir_ = K4E::Vector3::Normalize(launchDir_);
	}

	velocity_ = launchDir_ * (launchHorizontal_ * 3.0f) + K4E::Vector3{ 0.0f, launchUp_ * 1.2f, 0.0f };

	runCarry = false;
	view.SetAiming(false);

	if (auto* cam = view.GetCamera())
	{
		deathCameraPos_ = cam->GetTranslate();
		deathLookTarget_ = owner.GetWorldTransform()->translate_;
	}

	const float rollSign = (launchDir_.x >= 0.0f) ? -1.0f : 1.0f;
	view.StartDeathCamera(-maxPitchRad_, maxRollRad_ * rollSign);
}

void PlayerDeathComponent::Update(
	Player& owner,
	float deltaTime,
	PlayerMotorComponent& motor,
	PlayerViewComponent& view,
	PlayerWeaponVisualComponent& weaponVisual,
	PlayerDamageCollider& damageCollider,
	PlayerVfx& vfx,
	HUDManager* hudManager,
	float hp,
	float maxHp)
{
	timer_ += deltaTime;

	auto* tr = owner.GetWorldTransform();
	if (!tr)
	{
		owner.BaseCharacter::Update(deltaTime);
		return;
	}

	if (!settled_)
	{
		velocity_.y -= gravity_ * deltaTime;
		tr->translate_ += velocity_ * deltaTime;

		if (motor.IsGrounded() && velocity_.y <= 0.0f)
		{
			velocity_.y = 0.0f;

			const float damp = std::max(0.0f, 1.0f - groundFriction_ * deltaTime);
			velocity_.x *= damp;
			velocity_.z *= damp;

			const float speedXZSq = velocity_.x * velocity_.x + velocity_.z * velocity_.z;
			if (speedXZSq < 0.01f)
			{
				velocity_.x = 0.0f;
				velocity_.z = 0.0f;
				settled_ = true;
			}
		}
	}

	float t = timer_ / tiltTime_;
	if (t < 0.0f) t = 0.0f;
	if (t > 1.0f) t = 1.0f;

	const float rollSign = (launchDir_.x >= 0.0f) ? -1.0f : 1.0f;
	tr->rotate_.x = startRotate_.x + maxPitchRad_ * t;
	tr->rotate_.z = startRotate_.z + maxRollRad_ * rollSign * t;

	view.UpdateDeathCamera(deltaTime, t);

	owner.SetCenterPosition(tr->translate_);

	if (useFixedDeathCamera_)
	{
		if (auto* cam = view.GetCamera())
		{
			deathLookTarget_ = tr->translate_;

			K4E::Vector3 toTarget = deathLookTarget_ - deathCameraPos_;
			if (K4E::Vector3::Length(toTarget) > 0.0001f)
			{
				toTarget = K4E::Vector3::Normalize(toTarget);
			}

			// ここは Camera 側APIに合わせて調整
			cam->SetTranslate(deathCameraPos_);

			// もし LookAt 系APIがあるならそれを使う
			cam->SetForward(toTarget);
		}
	}
	else
	{
		view.BindBodyTransform(tr);
		view.SyncToPlayer();
		view.SyncViewModeToFirstPersonFlag();
	}

	weaponVisual.Update(deltaTime, false);
	damageCollider.SyncFromOwner();
	vfx.Update(deltaTime);
	owner.BaseCharacter::Update(deltaTime);

	if (hudManager)
	{
		hudManager->SetHP(hp, maxHp);
	}

	if (timer_ >= duration_)
	{
		finished_ = true;
		gameOverReady_ = true;
	}
}
