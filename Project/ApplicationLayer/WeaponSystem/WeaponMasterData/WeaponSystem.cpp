#define NOMINMAX
#include "WeaponSystem.h"

#include <algorithm>
#include <filesystem>

#include "WeaponMasterData.h"
#include "WeaponMasterDataValidator.h"

namespace
{
	bool ContainsFireMode(const std::vector<EFireMode>& modes, EFireMode target)
	{
		return std::find(modes.begin(), modes.end(), target) != modes.end();
	}
}

std::filesystem::path WeaponSystem::ResolveWeaponsRoot(const std::filesystem::path& inputPath)
{
	if (inputPath.empty()) return inputPath;

	const auto name = inputPath.filename().string();
	if (name == "weapons") return inputPath;
	if (name == "JSON" || name == "json") return inputPath / "weapons";
	return inputPath;
}

bool WeaponSystem::Load(const std::filesystem::path& rootDir, std::string* outError)
{
	equippedWeaponId_ = 0;
	db_.Clear();

	const auto weaponsRoot = ResolveWeaponsRoot(rootDir);

	if (!db_.LoadFromDirectory(weaponsRoot, outError))
	{
		return false;
	}
	return true;
}

bool WeaponSystem::ReloadAndReequip(std::string* outError)
{
	if (!db_.IsLoaded())
	{
		if (outError) *outError = "WeaponSystem: database is not loaded.";
		return false;
	}

	const int32_t prev = equippedWeaponId_;

	if (!db_.Reload(outError))
	{
		return false;
	}

	if (prev > 0 && db_.ContainsID(prev))
	{
		return EquipById(prev, outError);
	}

	equippedWeaponId_ = 0;
	if (db_.Size() == 0)
	{
		if (outError) outError->clear();
		return true;
	}
	return EquipFirst(outError);
}

bool WeaponSystem::RebuildEquippedFromDatabase(std::string* outError)
{
	if (equippedWeaponId_ <= 0)
	{
		if (outError) *outError = "WeaponSystem: no weapon is currently equipped.";
		return false;
	}
	return EquipById(equippedWeaponId_, outError);
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
	const FWeaponMasterData* src = db_.FindByID(weaponId);
	if (!src)
	{
		if (outError) *outError = "WeaponSystem: weaponId not found.";
		return false;
	}

	// Runtime適用前にカテゴリ整形 + Validate（Editorで編集中の事故防止）
	FWeaponMasterData md = *src;
	WeaponMasterDataDatabase::NormalizeByCategory(md);

	std::string validateErr;
	if (!WeaponMasterDataValidator::Validate(md, &validateErr))
	{
		if (outError)
		{
			*outError = "WeaponSystem: invalid master data for equip (weaponId=" +
				std::to_string(weaponId) + ")\n" + validateErr;
		}
		return false;
	}

	WeaponParams p = BuildParams(md);
	weapon_.Equip(p);

	// 追加：HUD用に現在武器のレティクル設定を保持
	equippedReticleData_ = md.reticleData;

	equippedWeaponId_ = weaponId;
	return true;
}

bool WeaponSystem::EquipNext(std::string* outError)
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

	// 現在未装備なら先頭を装備
	if (equippedWeaponId_ <= 0)
	{
		return EquipById(ids.front(), outError);
	}

	auto it = std::find(ids.begin(), ids.end(), equippedWeaponId_);
	if (it == ids.end())
	{
		return EquipById(ids.front(), outError);
	}

	++it;
	if (it == ids.end())
	{
		it = ids.begin(); // 末尾なら先頭へループ
	}

	return EquipById(*it, outError);
}

bool WeaponSystem::EquipPrev(std::string* outError)
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

	// 現在未装備なら末尾を装備
	if (equippedWeaponId_ <= 0)
	{
		return EquipById(ids.back(), outError);
	}

	auto it = std::find(ids.begin(), ids.end(), equippedWeaponId_);
	if (it == ids.end())
	{
		return EquipById(ids.back(), outError);
	}

	if (it == ids.begin())
	{
		it = ids.end();
	}
	--it;

	return EquipById(*it, outError);
}

WeaponParams WeaponSystem::BuildParams(const FWeaponMasterData& md)
{
	WeaponParams p{};

	p.weaponID = md.coreData.weaponID;
	p.isAutomatic = md.bIsAutomatic;
	p.canToggleFireMode = md.bCanToggleFireMode;

	// Stats
	p.damage = md.stats.damage;

	const float rpm = md.stats.fireRate;
	p.secPerShot = (rpm > 1e-3f) ? (60.0f / rpm) : 0.1f;

	p.magCapacity = std::max(1, md.stats.capacity);
	p.ammoPerShot = std::max(1, md.stats.ammoPerShot);
	p.maxReserveAmmo = std::max(0, md.stats.maxReserveAmmo);

	// リロード詳細
	p.reloadSec = std::max(0.0f, md.stats.reloadTime);
	p.tacticalReloadSec = std::max(0.0f, md.stats.tacticalReloadTime);
	p.emptyReloadSec = std::max(0.0f, md.stats.emptyReloadTime);
	p.canInterruptReload = md.stats.bCanInterruptReload;

	// ペレット
	p.pelletCount = std::max(1, md.stats.pelletCount);
	p.pelletSpreadAngle = std::max(0.0f, md.stats.pelletSpreadAngle);

	// Handling
	p.accuracy = md.handling.accuracy;
	p.spreadIncrease = std::max(0.0f, md.handling.spreadIncrease);
	p.recoilRecovery = std::max(0.0f, md.handling.recoilRecovery);

	// 散布界（実際に使う）
	p.baseHipSpreadDeg = std::max(0.0f, md.handling.baseHipSpread);
	p.baseAdsSpreadDeg = std::max(0.0f, md.handling.baseAdsSpread);
	p.spreadRecoveryRate = std::max(0.0f, md.handling.spreadRecoveryRate);
	p.maxSpreadDeg = std::max(0.0f, md.handling.maxSpread);

	// ADS
	p.adsZoomFov = md.handling.adsZoomFov;
	p.adsTransitionSpeed = md.handling.adsTransitionSpeed;
	p.adsMoveSpeedMultiplier = std::clamp(md.handling.adsMoveSpeedMultiplier, 0.0f, 2.0f);

	// Projectile
	if (md.projectileData.has_value())
	{
		p.isProjectile = md.projectileData->bIsProjectile;
		p.projectileSpeed = md.projectileData->projectileSpeed;
		p.projectileLifeTime = std::max(0.01f, md.projectileData->projectileLifeTime);
		p.maxRange = (md.projectileData->maxRange > 0.0f) ? md.projectileData->maxRange : p.maxRange;
		p.traceRadius = std::max(0.0f, md.projectileData->traceRadius);
		p.splashRadius = std::max(0.0f, md.projectileData->splashRadius);
		p.splashDamage = (p.splashRadius > 0.0f) ? std::max(1, static_cast<int>(md.stats.damage)) : 0;
		p.splashCanDamageSelf = md.projectileData->bCanDamageSelf;

		// ✅ ここが重要（今まで固定値だった）
		if (md.projectileData->spawnForwardOffset > 0.0f)
			p.muzzleForwardOffset = md.projectileData->spawnForwardOffset;
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

	return p;
}