#define NOMINMAX
#include "WeaponSystem.h"

#include <algorithm>
#include "WeaponMasterData.h"

bool WeaponSystem::Load(const std::filesystem::path& rootDir, std::string* outError)
{
	equippedWeaponId_ = 0;
	db_.Clear();

	if (!db_.LoadFromDirectory(rootDir, outError))
	{
		return false;
	}

	return true;
}

bool WeaponSystem::EquipFirst(std::string* outError)
{
	if (!db_.IsLoaded() || db_.Size() == 0)
	{
		if (outError) *outError = "WeaponSystem: database is not loaded or empty.";
		return false;
	}

	const auto ids = db_.GetSortedIDList();
	if (ids.empty())
	{
		if (outError) *outError = "WeaponSystem: ID list is empty.";
		return false;
	}

	return EquipById(ids.front(), outError);
}

bool WeaponSystem::EquipById(int32_t weaponId, std::string* outError)
{
	const FWeaponMasterData* md = db_.FindByID(weaponId);
	if (!md)
	{
		if (outError) *outError = "WeaponSystem: weaponId not found.";
		return false;
	}

	WeaponParams p = BuildParams(*md);
	weapon_.Equip(p);
	equippedWeaponId_ = weaponId;
	return true;
}

WeaponParams WeaponSystem::BuildParams(const FWeaponMasterData& md)
{
	WeaponParams p{};

	p.weaponID = md.coreData.weaponID;
	p.isAutomatic = md.bIsAutomatic;
	p.canToggleFireMode = md.bCanToggleFireMode;

	// Stats
	p.damage = md.stats.damage;

	// RPM -> sec/shot
	const float rpm = md.stats.fireRate;
	p.secPerShot = (rpm > 1e-3f) ? (60.0f / rpm) : 0.1f;

	p.magCapacity = md.stats.capacity;
	p.ammoPerShot = std::max(1, md.stats.ammoPerShot);
	p.maxReserveAmmo = std::max(0, md.stats.maxReserveAmmo);
	p.reloadSec = std::max(0.0f, md.stats.reloadTime);

	// Handling
	p.accuracy = md.handling.accuracy;
	p.spreadIncrease = md.handling.spreadIncrease;
	p.recoilRecovery = md.handling.recoilRecovery;

	// ADS
	p.adsZoomFov = md.handling.adsZoomFov;
	p.adsTransitionSpeed = md.handling.adsTransitionSpeed;

	// Projectile
	if (md.projectileData.has_value())
	{
		p.isProjectile = md.projectileData->bIsProjectile;
		p.projectileSpeed = md.projectileData->projectileSpeed;
		p.maxRange = (md.projectileData->maxRange > 0.0f) ? md.projectileData->maxRange : p.maxRange;
	}

	// Burst
	if (md.burstSettings.has_value() && md.burstSettings->count >= 2)
	{
		p.burstCount = md.burstSettings->count;
		p.burstIntervalSec = std::max(0.0f, md.burstSettings->interval);
	}

	// Charge
	if (md.chargeSettings.has_value())
	{
		p.maxChargeTime = std::max(0.0f, md.chargeSettings->maxChargeTime);
	}

	// ※ muzzleForwardOffset はエンジン/モデルに依存するので、必要なら別途調整項目を追加
	p.muzzleForwardOffset = 0.35f;

	return p;
}
