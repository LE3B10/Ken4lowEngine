#include "PlayerWeaponController.h"

#include "PlayerWeaponComponent.h"
#include "PlayerWeaponVisualComponent.h"
#include "PlayerBrainComponent.h"
#include "PlayerStateMachines.h"

void PlayerWeaponController::Initialize(
	PlayerWeaponComponent* weapon,
	PlayerWeaponVisualComponent* visual,
	PlayerBrainComponent* brain,
	PlayerAPI* api)
{
	weapon_ = weapon;
	visual_ = visual;
	brain_ = brain;
	api_ = api;
}

void PlayerWeaponController::CancelReloadOnly()
{
	if (!weapon_)
	{
		return;
	}

	// WeaponComponent 側の実装差に耐える
	if constexpr (requires(PlayerWeaponComponent & w) { w.CancelReload(); })
	{
		weapon_->CancelReload();
	}
	else if constexpr (requires(PlayerWeaponComponent & w) { w.AbortReload(); })
	{
		weapon_->AbortReload();
	}
	else if constexpr (requires(PlayerWeaponComponent & w) { w.StopReload(); })
	{
		weapon_->StopReload();
	}
}

void PlayerWeaponController::HandleWheelSwitch(InputSnapshot& snap)
{
	if (!weapon_)
	{
		return;
	}

	if (weapon_->GetAllowedHotbarSlotCount() <= 1)
	{
		snap.weaponSwitch = 0;
		return;
	}

	bool switched = false;

	// ホイール切替だけを担当
	if (snap.weaponSwitch > 0)
	{
		weapon_->SwitchWeaponCategoryByDelta(-1);
		switched = true;
	}
	else if (snap.weaponSwitch < 0)
	{
		weapon_->SwitchWeaponCategoryByDelta(+1);
		switched = true;
	}

	if (!switched)
	{
		return;
	}

	// 切替時はリロードを即中断
	bool isReloading = false;
	float reloadTimer = 0.0f;
	float reloadSec = 0.0f;
	weapon_->GetReloadUI(isReloading, reloadTimer, reloadSec);

	if (isReloading)
	{
		CancelReloadOnly();

		if (brain_ && api_ && brain_->GetCurrentCombatId() == CombatId::Reload)
		{
			PlayerContext cancelCtx{ *api_, snap, 0.0f };
			brain_->GetBrain().combat.Change(cancelCtx, snap.aimHeld ? CombatId::Aim : CombatId::Hip);
		}
	}

	// ホイール入力はここで消費
	snap.weaponSwitch = 0;

	// 見た目を次フレームで必ず更新
	if (visual_)
	{
		visual_->ForceRefresh();
		visual_->StartEquipAnimation();
	}
}

void PlayerWeaponController::TryCancelReloadAndRestoreCombat(const InputSnapshot& snap, float deltaTime)
{
	CancelReloadOnly();

	if (!brain_ || !api_)
	{
		return;
	}

	if (brain_->GetCurrentCombatId() == CombatId::Reload)
	{
		PlayerContext cancelCtx{ *api_, snap, deltaTime };
		brain_->GetBrain().combat.Change(cancelCtx, snap.aimHeld ? CombatId::Aim : CombatId::Hip);
	}
}

bool PlayerWeaponController::IsCurrentWeaponMelee() const
{
	return false;
}
