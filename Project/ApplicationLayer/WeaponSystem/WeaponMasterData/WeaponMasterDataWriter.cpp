#include "WeaponMasterDataWriter.h"
#include <fstream>
#include <json.hpp>
#include "WeaponMasterData.h"
using nlohmann::json;

namespace
{
	static const char* CategoryFolder(EWeaponCategory c)
	{
		switch (c)
		{
		case EWeaponCategory::Primary: return "primary";
		case EWeaponCategory::Backup:  return "backup";
		case EWeaponCategory::Melee:   return "melee";
		case EWeaponCategory::Special: return "special";
		case EWeaponCategory::Sniper:  return "sniper";
		case EWeaponCategory::Heavy:   return "heavy";
		default: return "unknown";
		}
	}

	static std::string RarityToStr(EWeaponRarity r)
	{
		switch (r)
		{
		case EWeaponRarity::Common: return "common";
		case EWeaponRarity::Rare: return "rare";
		case EWeaponRarity::Epic: return "epic";
		case EWeaponRarity::Legendary: return "legendary";
		case EWeaponRarity::Mythical: return "mythical";
		default: return "common";
		}
	}

	static std::string AmmoToStr(EAmmoType a)
	{
		switch (a)
		{
		case EAmmoType::Default: return "default";
		case EAmmoType::Energy: return "energy";
		case EAmmoType::Explosive: return "explosive";
		case EAmmoType::None: return "none";
		default: return "default";
		}
	}

	static std::string CategoryToStr(EWeaponCategory c)
	{
		return CategoryFolder(c); // 同じ文字でOK
	}

	static bool EndsWith(const std::string& str, const std::string& suffix)
	{
		if (str.size() < suffix.size()) return false;
		return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
	}
}

/// -------------------------------------------------------------
///		武器マスターデータベース内の全データをカテゴリ別に rootDir 配下へ保存します
/// -------------------------------------------------------------
bool WeaponMasterDataWriter::SaveAllByCategory(const WeaponMasterDataDatabase& database, const std::filesystem::path& rootDir, std::string* outError)
{
	for (const auto& [id, d] : database.GetAll())
	{
		// 古いファイルをs買う所
		DeleteFilesForWeapon(rootDir, d, outError);

		const auto file = MakeWeaponFilePath(rootDir, d);
		if (!SaveOne(file, d, outError)) return false;
	}
	return true;
}

/// -------------------------------------------------------------
///		指定されたファイルパスに武器マスターデータを保存する
/// -------------------------------------------------------------
bool WeaponMasterDataWriter::SaveOne(const std::filesystem::path& filePath, const FWeaponMasterData& data, std::string* outError)
{
	json j;

	j["coreData"] = {
		{"weaponID", data.coreData.weaponID},
		{"weaponName", data.coreData.weaponName},
		{"category", CategoryToStr(data.coreData.category)},
		{"rarity", RarityToStr(data.coreData.rarity)},
	};

	j["economyData"] = {
		{"currencyType", data.economyData.currencyType},
		{"purchasePrice", data.economyData.purchasePrice},
		{"minLevelToUnlock", data.economyData.minLevelToUnlock},
		{"upgradeCosts", data.economyData.upgradeCosts},
		{"bIsLimitedTime", data.economyData.bIsLimitedTime},
		{"discountRate", data.economyData.discountRate},
	};

	j["stats"] = {
		{"damage", data.stats.damage},
		{"fireRate", data.stats.fireRate},
		{"mobility", data.stats.mobility},
		{"capacity", data.stats.capacity},
		{"ammoType", AmmoToStr(data.stats.ammoType)},
		{"reloadTime", data.stats.reloadTime},
		{"ammoPerShot", data.stats.ammoPerShot},
		{"maxReserveAmmo", data.stats.maxReserveAmmo},
		{"criticalChance", data.stats.criticalChance},
		{"headshotMultiplier", data.stats.headshotMultiplier},
	};

	j["handling"] = {
		{"accuracy", data.handling.accuracy},
		{"spreadIncrease", data.handling.spreadIncrease},
		{"verticalRecoil", data.handling.verticalRecoil},
		{"horizontalRecoil", data.handling.horizontalRecoil},
		{"recoilRecovery", data.handling.recoilRecovery},
		{"adsZoomFov", data.handling.adsZoomFov},
		{"adsTransitionSpeed", data.handling.adsTransitionSpeed},
		{"fixedDelayTime", data.handling.fixedDelayTime},
		{"zoomLevel", data.handling.zoomLevel},
	};

	j["assetData"] = {
		{"modelPath", data.assetData.modelPath},
		{"iconPath", data.assetData.iconPath},
	};

	j["soundData"] = {
		{"fireSoundPath", data.soundData.fireSoundPath},
		{"reloadSoundPath", data.soundData.reloadSoundPath},
		{"emptySoundPath", data.soundData.emptySoundPath},
		{"equipSoundPath", data.soundData.equipSoundPath},
		{"impactSoundPath", data.soundData.impactSoundPath},
	};

	if (data.projectileData) j["projectileData"] = {
		{"maxRange", data.projectileData->maxRange},
		{"bIsProjectile", data.projectileData->bIsProjectile},
		{"projectileSpeed", data.projectileData->projectileSpeed},
		{"pierceCount", data.projectileData->pierceCount},
		{"ricochetCount", data.projectileData->ricochetCount},
		{"splashRadius", data.projectileData->splashRadius},
		{"bCanDamageSelf", data.projectileData->bCanDamageSelf},
	};

	if (data.burstSettings) j["burstSettings"] = {
		{"count", data.burstSettings->count},
		{"interval", data.burstSettings->interval},
	};

	if (data.chargeSettings) j["chargeSettings"] = {
		{"maxChargeTime", data.chargeSettings->maxChargeTime},
	};

	if (data.meleeData) j["meleeData"] = {
		{"attackRange", data.meleeData->attackRange},
		{"attackArc", data.meleeData->attackArc},
		{"verticalAngle", data.meleeData->verticalAngle},
		{"startupDelay", data.meleeData->startupDelay},
		{"activeFrames", data.meleeData->activeFrames},
		{"recoveryDelay", data.meleeData->recoveryDelay},
		{"maxComboCount", data.meleeData->maxComboCount},
		{"comboWindow", data.meleeData->comboWindow},
		{"dashImpulse", data.meleeData->dashImpulse},
		{"bCanCharge", data.meleeData->bCanCharge},
		{"maxChargeTime", data.meleeData->maxChargeTime},
		{"chargeDamageMultiplier", data.meleeData->chargeDamageMultiplier},
		{"bCanBlock", data.meleeData->bCanBlock},
		{"blockDamageReduction", data.meleeData->blockDamageReduction},
		{"hitStopDuration", data.meleeData->hitStopDuration},
		{"knockbackForce", data.meleeData->knockbackForce},
	};

	j["bIsAutomatic"] = data.bIsAutomatic;
	j["bCanToggleFireMode"] = data.bCanToggleFireMode;
	j["attributes"] = json::array();
	for (auto a : data.attributes) j["attributes"].push_back((int)a); // ※ loaderが文字列対応なら文字列でもOK

	std::error_code ec;
	std::filesystem::create_directories(filePath.parent_path(), ec);

	std::ofstream ofs(filePath);
	if (!ofs)
	{
		if (outError) *outError = "Failed to open: " + filePath.string();
		return false;
	}
	ofs << j.dump(4);
	return true;
}

std::filesystem::path WeaponMasterDataWriter::MakeWeaponFilePath(const std::filesystem::path& rootDir, const FWeaponMasterData& data)
{
	const auto dir = rootDir / CategoryFolder(data.coreData.category);

	const std::string stem = SanitizeFileStem(data.coreData.weaponName);
	const std::string idStr = std::to_string(data.coreData.weaponID);

	// 末尾が _<id>なら、もうIDが入ってる扱いにして二重付与しない
	const std::string suffix = "_" + idStr;
	const bool already = (stem.size() >= suffix.size()) && (stem.compare(stem.size() - suffix.size(), suffix.size(), suffix) == 0);

	const std::string fileStem = already ? stem : (stem + "_" + idStr);
	return dir / (fileStem + ".json");
}

std::string WeaponMasterDataWriter::SanitizeFileStem(std::string s)
{
	const char* bad = "\\/:*?\"<>|";
	for (char& c : s)
	{
		if ((unsigned char)c < 32) c = '_';
		for (const char* p = bad; *p; ++p)
		{
			if (c == *p) { c = '_'; break; }
		}
	}
	while (!s.empty() && (s.back() == '.' || s.back() == ' ')) s.pop_back();
	if (s.empty()) s = "Weapon";
	return s;
}

/// -------------------------------------------------------------
///		指定したルートディレクトリ内で、指定した武器IDに対応するファイルを削除します
/// -------------------------------------------------------------
bool WeaponMasterDataWriter::DeleteFilesByWeaponID(const std::filesystem::path& rootDir, int32_t weaponID, std::string* outError)
{
	namespace fs = std::filesystem;
	std::error_code ec;

	if (!fs::exists(rootDir, ec))
		return true;

	const std::string legacy = std::to_string(weaponID) + ".json";		 // 旧形式
	const std::string suffix = "_" + std::to_string(weaponID) + ".json"; // 新形式

	bool deletedAny = false;

	for (auto it = fs::directory_iterator(rootDir, ec); !ec && it != fs::directory_iterator(); it.increment(ec))
	{
		if (!it->is_directory()) continue;

		for (auto fi = fs::directory_iterator(it->path(), ec); !ec && fi != fs::directory_iterator(); fi.increment(ec))
		{
			if (!fi->is_regular_file()) continue;

			const std::string fn = fi->path().filename().string();
			if (fn == legacy || EndsWith(fn, suffix))
			{
				fs::remove(fi->path(), ec);
				if (!ec) deletedAny = true;
				else if (outError) *outError = "Failed to remove: " + fi->path().string();
			}
		}
	}

	return true;
}

bool WeaponMasterDataWriter::DeleteFilesForWeapon(const std::filesystem::path& rootDir, const FWeaponMasterData& data, std::string* outError)
{
	// まずID系を掃除（name_id.json や 1.json）
	DeleteFilesByWeaponID(rootDir, data.coreData.weaponID, outError);

	// さらに name.json が残るケースを掃除（Weapon_1.json など）
	namespace fs = std::filesystem;
	std::error_code ec;

	if (!fs::exists(rootDir, ec)) return true;

	const std::string stem = SanitizeFileStem(data.coreData.weaponName);
	const std::string nameOnly = stem + ".json";

	for (auto it = fs::directory_iterator(rootDir, ec);
		!ec && it != fs::directory_iterator();
		it.increment(ec))
	{
		if (!it->is_directory()) continue;

		const fs::path p = it->path() / nameOnly;
		if (fs::exists(p, ec))
		{
			fs::remove(p, ec);
			if (ec && outError) *outError = "Failed to remove: " + p.string();
		}
	}
	return true;
}
