#include "WeaponMasterDataValidator.h"

/// -------------------------------------------------------------
///				　		武器マスターデータ検証
/// -------------------------------------------------------------
bool WeaponMasterDataValidator::Validate(const FWeaponMasterData& data, std::string* outError)
{
	bool ok = true;

	/// ---------- コアデータ検証 ---------- ///
	if (data.coreData.weaponID <= 0) { ok = false; Append(outError, "coreData.weaponID must be > 0"); }
	if (data.coreData.weaponName.empty()) { ok = false; Append(outError, "coreData.weaponName is empty"); }

	/// ---------- 経済データ検証 ---------- ///
	if (data.economyData.purchasePrice < 0) { ok = false; Append(outError, "economyData.purchasePrice must be >= 0"); }
	if (data.economyData.minLevelToUnlock < 0) { ok = false; Append(outError, "economyData.minLevelToUnlock must be >= 0"); }
	if (data.economyData.discountRate < 0.0f || data.economyData.discountRate > 1.0f) { ok = false; Append(outError, "economyData.discountRate must be in [0, 1]"); }

	/// ---------- 共通データ検証 ---------- ///
	if (data.stats.damage < 0.0f) { ok = false; Append(outError, "stats.damage must be >= 0"); }
	if (data.stats.reloadTime < 0.0f) { ok = false; Append(outError, "stats.reloadTime must be >= 0"); }
	if (data.stats.fireRate < 0.0f) { ok = false; Append(outError, "stats.fireRate must be >= 0"); }
	if (data.stats.capacity < 0) { ok = false; Append(outError, "stats.capacity must be >= 0"); }
	if (data.stats.ammoPerShot <= 0) { ok = false; Append(outError, "stats.ammoPerShot must be > 0"); }
	if (data.stats.maxReserveAmmo < 0) { ok = false; Append(outError, "stats.maxReserveAmmo must be >= 0"); }
	if (data.stats.criticalChance < 0.0f || data.stats.criticalChance > 1.0f) { ok = false; Append(outError, "stats.criticalChance must be in [0, 1]"); }
	if (data.stats.headshotMultiplier < 0.0f) { ok = false; Append(outError, "stats.headshotMultiplier must be >= 0.0"); }

	/// ---------- 操作・反動データ検証 ---------- ///
	if (data.handling.accuracy < 0.0f || data.handling.accuracy > 1.0f) { ok = false; Append(outError, "handling.accuracy must be in [0, 1]"); }
	if (data.handling.fixedDelayTime < 0.0f) { ok = false; Append(outError, "handling.fixedDelayTime must be >= 0"); }
	if (data.handling.adsZoomFov < 0.0f) { ok = false; Append(outError, "handling.adsZoomFov must be >= 0"); }
	if (data.handling.zoomLevel < 0.0f) { ok = false; Append(outError, "handling.zoomLevel must be >= 0"); }

	// 近接武器かどうか
	const bool bIsMelee = (data.coreData.category == EWeaponCategory::Melee) || data.meleeData.has_value();

	if (bIsMelee)
	{
		// ---------- 近接武器データ検証 ---------- ///
		if (data.stats.ammoType != EAmmoType::None) { ok = false; Append(outError, "Melee weapon stats.ammoType should be None"); }
		if (data.projectileData.has_value()) { ok = false; Append(outError, "Melee weapon projectileData should be null"); }
	}
	else
	{
		// ---------- 射撃武器データ検証 ---------- ///
		if (data.stats.ammoType == EAmmoType::None) { ok = false; Append(outError, "Ranged Weapon: stats.ammoType must not be None"); }
		if (data.stats.capacity <= 0) { ok = false; Append(outError, "Ranged Weapon: stats.capacity must be > 0"); }
		if (data.stats.ammoPerShot <= 0) { ok = false; Append(outError, "Ranged Weapon: stats.ammoPerShot must be > 0"); }

		/// ---------- 弾道データ検証 ---------- ///
		if (data.projectileData)
		{
			if (data.projectileData->maxRange < 0.0f) { ok = false; Append(outError, "projectileData.maxRange must be >= 0"); }
			if (data.projectileData->bIsProjectile && data.projectileData->projectileSpeed <= 0.0f) { ok = false; Append(outError, "projectileData.projectileSpeed must be > 0 for projectile weapons"); }
			if (data.projectileData->pierceCount < 0) { ok = false; Append(outError, "projectileData.pierceCount must be >= 0"); }
			if (data.projectileData->ricochetCount < 0) { ok = false; Append(outError, "projectileData.ricochetCount must be >= 0"); }
			if (data.projectileData->splashRadius < 0.0f) { ok = false; Append(outError, "projectileData.splashRadius must be >= 0"); }
		}
	}

	/// ---------- バーストとチャージ設定検証 ---------- ///
	if (data.burstSettings)
	{
		if (data.burstSettings->count <= 0) { ok = false; Append(outError, "burstSettings.count must be > 0"); }
		if (data.burstSettings->interval < 0.0f) { ok = false; Append(outError, "burstSettings.interval must be >= 0"); }
	}
	if (data.chargeSettings)
	{
		if (data.chargeSettings->maxChargeTime <= 0.0f) { ok = false; Append(outError, "chargeSettings.maxChargeTime must be > 0"); }
	}

	return ok;
}

/// -------------------------------------------------------------
///		エラーメッセージを追加し、操作の成否を返す
/// -------------------------------------------------------------
void WeaponMasterDataValidator::Append(std::string* outError, const std::string& msg)
{
	if (!outError) return;
	if (!outError->empty()) *outError += "\n";
	*outError += msg;
}
