#include "WeaponMasterDataLoader.h"
#include "WeaponMasterDataValidator.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

#include <unordered_set>
#include <system_error>

#include <json.hpp>
#include "WeaponMasterData.h"
using nlohmann::json;

namespace
{
	static bool IsUnderWeaponCategoryFolder(const std::filesystem::path& filePath, const std::filesystem::path& rootDire)
	{
		std::error_code ec;
		if (std::filesystem::equivalent(filePath.parent_path(), rootDire, ec)) return false;

		// カテゴリフォルダ配下のみ許可
		static const std::unordered_set<std::string> kAllowed = { "primary", "backup", "melee", "special", "sniper", "heavy" };

		ec.clear();

		const auto rel = std::filesystem::relative(filePath.parent_path(), rootDire, ec);
		if (ec) return false;

		auto it = rel.begin();
		if (it == rel.end()) return false;

		const std::string top = it->string();

		if (top.size() >= 5 && top.ends_with(".json"))
			return false; // 直下にファイルがある場合

		return kAllowed.contains(top);
	}
}

/// ---------- 小物ユーティリティ ---------- ///
static std::string ToLower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}

template<typename T>
static void ReadIfExists(const json& j, const char* key, T& outValue)
{
	if (j.contains(key) && !j.at(key).is_null())
	{
		outValue = j.at(key).get<T>();
	}
}

template<typename T>
static void ReadOptionalIfExists(const json& j, const char* key, std::optional<T>& outOpt)
{
	if (!j.contains(key) || j.at(key).is_null())
	{
		outOpt.reset();
		return;
	}
	outOpt = j.at(key).get<T>();
}

/// ---------- enum 変換（文字列 or 数値 両対応）---------- ///
static bool ParseEnum_EWeaponRarity(const json& v, EWeaponRarity& out)
{
	if (v.is_number_integer())
	{
		out = static_cast<EWeaponRarity>(v.get<int>());
		return true;
	}
	if (!v.is_string()) return false;

	const std::string s = ToLower(v.get<std::string>());
	if (s == "common") { out = EWeaponRarity::Common; return true; }
	if (s == "rare") { out = EWeaponRarity::Rare; return true; }
	if (s == "epic") { out = EWeaponRarity::Epic; return true; }
	if (s == "legendary") { out = EWeaponRarity::Legendary; return true; }
	if (s == "mythical") { out = EWeaponRarity::Mythical; return true; }
	return false;
}

static bool ParseEnum_EAmmoType(const json& v, EAmmoType& out)
{
	if (v.is_number_integer())
	{
		out = static_cast<EAmmoType>(v.get<int>());
		return true;
	}
	if (!v.is_string()) return false;

	const std::string s = ToLower(v.get<std::string>());
	if (s == "default") { out = EAmmoType::Default; return true; }
	if (s == "energy") { out = EAmmoType::Energy; return true; }
	if (s == "explosive") { out = EAmmoType::Explosive; return true; }
	if (s == "none") { out = EAmmoType::None; return true; }
	return false;
}

static bool ParseEnum_EWeaponCategory(const json& v, EWeaponCategory& out)
{
	if (v.is_number_integer())
	{
		out = static_cast<EWeaponCategory>(v.get<int>());
		return true;
	}
	if (!v.is_string()) return false;

	const std::string s = ToLower(v.get<std::string>());
	if (s == "primary") { out = EWeaponCategory::Primary; return true; }
	if (s == "backup") { out = EWeaponCategory::Backup; return true; }
	if (s == "melee") { out = EWeaponCategory::Melee; return true; }
	if (s == "special") { out = EWeaponCategory::Special; return true; }
	if (s == "sniper") { out = EWeaponCategory::Sniper; return true; }
	if (s == "heavy") { out = EWeaponCategory::Heavy; return true; }
	return false;
}

static bool ParseEnum_EWeaponAttribute(const json& v, EWeaponAttribute& out)
{
	if (v.is_number_integer())
	{
		out = static_cast<EWeaponAttribute>(v.get<int>());
		return true;
	}
	if (!v.is_string()) return false;

	const std::string s = ToLower(v.get<std::string>());
	if (s == "none") { out = EWeaponAttribute::None; return true; }
	if (s == "poison") { out = EWeaponAttribute::Poison; return true; }
	if (s == "burning") { out = EWeaponAttribute::Burning; return true; }
	if (s == "areadamage") { out = EWeaponAttribute::AreaDamage; return true; }
	if (s == "bouncing") { out = EWeaponAttribute::Bouncing; return true; }
	if (s == "lifesteal") { out = EWeaponAttribute::LifeSteal; return true; }
	if (s == "wallbreak") { out = EWeaponAttribute::WallBreak; return true; }
	if (s == "fixeddelay") { out = EWeaponAttribute::FixedDelay; return true; }
	return false;
}

/// ---------- struct の from_json 定義 ---------- ///
static void from_json(const json& j, FWeaponCore& v)
{
	ReadIfExists(j, "weaponID", v.weaponID);
	ReadIfExists(j, "weaponName", v.weaponName);

	if (j.contains("category"))
	{
		EWeaponCategory cat = v.category;
		if (ParseEnum_EWeaponCategory(j.at("category"), cat)) v.category = cat;
	}
	if (j.contains("rarity"))
	{
		EWeaponRarity rar = v.rarity;
		if (ParseEnum_EWeaponRarity(j.at("rarity"), rar)) v.rarity = rar;
	}
}

static void from_json(const json& j, FWeaponStats& v)
{
	ReadIfExists(j, "damage", v.damage);
	ReadIfExists(j, "fireRate", v.fireRate);
	ReadIfExists(j, "mobility", v.mobility);
	ReadIfExists(j, "capacity", v.capacity);
	ReadIfExists(j, "reloadTime", v.reloadTime);
	ReadIfExists(j, "ammoPerShot", v.ammoPerShot);
	ReadIfExists(j, "maxReserveAmmo", v.maxReserveAmmo);
	ReadIfExists(j, "criticalChance", v.criticalChance);
	ReadIfExists(j, "headshotMultiplier", v.headshotMultiplier);

	if (j.contains("ammoType"))
	{
		EAmmoType a = v.ammoType;
		if (ParseEnum_EAmmoType(j.at("ammoType"), a)) v.ammoType = a;
	}
}

static void from_json(const json& j, FWeaponHandling& v)
{
	ReadIfExists(j, "accuracy", v.accuracy);
	ReadIfExists(j, "spreadIncrease", v.spreadIncrease);
	ReadIfExists(j, "verticalRecoil", v.verticalRecoil);
	ReadIfExists(j, "horizontalRecoil", v.horizontalRecoil);
	ReadIfExists(j, "recoilRecovery", v.recoilRecovery);

	ReadIfExists(j, "adsZoomFov", v.adsZoomFov);
	ReadIfExists(j, "adsTransitionSpeed", v.adsTransitionSpeed);
	ReadIfExists(j, "fixedDelayTime", v.fixedDelayTime);
	ReadIfExists(j, "zoomLevel", v.zoomLevel);
}

static void from_json(const json& j, FWeaponProjectileData& v)
{
	ReadIfExists(j, "maxRange", v.maxRange);
	ReadIfExists(j, "bIsProjectile", v.bIsProjectile);
	ReadIfExists(j, "projectileSpeed", v.projectileSpeed);

	ReadIfExists(j, "pierceCount", v.pierceCount);
	ReadIfExists(j, "ricochetCount", v.ricochetCount);
	ReadIfExists(j, "splashRadius", v.splashRadius);
	ReadIfExists(j, "bCanDamageSelf", v.bCanDamageSelf);
}

static void from_json(const json& j, FBurstSettings& v)
{
	ReadIfExists(j, "count", v.count);
	ReadIfExists(j, "interval", v.interval);
}

static void from_json(const json& j, FChargeSettings& v)
{
	ReadIfExists(j, "maxChargeTime", v.maxChargeTime);
}

static void from_json(const json& j, FMeleeWeaponData& v)
{
	ReadIfExists(j, "attackRange", v.attackRange);
	ReadIfExists(j, "attackArc", v.attackArc);
	ReadIfExists(j, "verticalAngle", v.verticalAngle);

	ReadIfExists(j, "startupDelay", v.startupDelay);
	ReadIfExists(j, "activeFrames", v.activeFrames);
	ReadIfExists(j, "recoveryDelay", v.recoveryDelay);

	ReadIfExists(j, "maxComboCount", v.maxComboCount);
	ReadIfExists(j, "comboWindow", v.comboWindow);

	ReadIfExists(j, "dashImpulse", v.dashImpulse);
	ReadIfExists(j, "bCanCharge", v.bCanCharge);
	ReadIfExists(j, "maxChargeTime", v.maxChargeTime);
	ReadIfExists(j, "chargeDamageMultiplier", v.chargeDamageMultiplier);
}

static void from_json(const json& j, FWeaponEconomyData& v)
{
	ReadIfExists(j, "currencyType", v.currencyType);
	ReadIfExists(j, "purchasePrice", v.purchasePrice);
	ReadIfExists(j, "minLevelToUnlock", v.minLevelToUnlock);
	ReadIfExists(j, "upgradeCosts", v.upgradeCosts);
	ReadIfExists(j, "bIsLimitedTime", v.bIsLimitedTime);
	ReadIfExists(j, "discountRate", v.discountRate);
}

static void from_json(const json& j, FWeaponAssets& v)
{
	ReadIfExists(j, "modelPath", v.modelPath);
	ReadIfExists(j, "iconPath", v.iconPath);
}

static void from_json(const json& j, FWeaponSounds& v)
{
	ReadIfExists(j, "fireSoundPath", v.fireSoundPath);
	ReadIfExists(j, "reloadSoundPath", v.reloadSoundPath);
	ReadIfExists(j, "emptySoundPath", v.emptySoundPath);
	ReadIfExists(j, "equipSoundPath", v.equipSoundPath);
	ReadIfExists(j, "impactSoundPath", v.impactSoundPath);
}

static void from_json(const json& j, FWeaponMasterData& v)
{
	// struct側のデフォルト値を活かしたいので「存在する物だけ上書き」方式
	if (j.contains("coreData")) v.coreData = j.at("coreData").get<FWeaponCore>();
	if (j.contains("economyData")) v.economyData = j.at("economyData").get<FWeaponEconomyData>();

	if (j.contains("stats")) v.stats = j.at("stats").get<FWeaponStats>();
	if (j.contains("handling")) v.handling = j.at("handling").get<FWeaponHandling>();

	if (j.contains("assetData")) v.assetData = j.at("assetData").get<FWeaponAssets>();
	if (j.contains("soundData")) v.soundData = j.at("soundData").get<FWeaponSounds>();

	ReadOptionalIfExists(j, "meleeData", v.meleeData);
	ReadOptionalIfExists(j, "projectileData", v.projectileData);
	ReadOptionalIfExists(j, "burstSettings", v.burstSettings);
	ReadOptionalIfExists(j, "chargeSettings", v.chargeSettings);

	ReadIfExists(j, "bIsAutomatic", v.bIsAutomatic);
	ReadIfExists(j, "bCanToggleFireMode", v.bCanToggleFireMode);

	// attributes は文字列配列 or 数値配列対応
	if (j.contains("attributes") && j.at("attributes").is_array())
	{
		v.attributes.clear();
		for (const auto& a : j.at("attributes"))
		{
			EWeaponAttribute attr = EWeaponAttribute::None;
			if (ParseEnum_EWeaponAttribute(a, attr))
			{
				// Noneは入れない
				if (attr != EWeaponAttribute::None)
					v.attributes.push_back(attr);
			}
		}
	}
}

/// -------------------------------------------------------------
///		指定したJSONファイルから武器マスターデータを読み込み
/// -------------------------------------------------------------
bool WeaponMasterDataLoader::LoadFromFile(const std::filesystem::path& jsonPath, FWeaponMasterData& out, std::string* outError)
{
	std::ifstream ifs(jsonPath);
	if (!ifs)
	{
		return Fail(outError, "WeaponMasterDataLoader : ファイルを開けません: " + jsonPath.string());
	}

	json j;
	try
	{
		ifs >> j;
	}
	catch (const std::exception& e)
	{
		return Fail(outError, std::string("WeaponMasterDataLoader: JSON parse 失敗: ") + e.what());
	}

	try
	{
		// ここで from_json が走る
		out = j.get<FWeaponMasterData>();
	}
	catch (const std::exception& e)
	{
		return Fail(outError, std::string("WeaponMasterDataLoader: JSON->Struct変換失敗: ") + e.what());
	}

	std::string validateError;
	if (!WeaponMasterDataValidator::Validate(out, &validateError)) return Fail(outError, validateError);

	return true;
}

/// -------------------------------------------------------------
///	指定したディレクトリからすべての武器マスターデータを読み込み
/// -------------------------------------------------------------
bool WeaponMasterDataLoader::LoadAllFromDirectory(const std::filesystem::path& dirPath, std::unordered_map<int32_t, FWeaponMasterData>& outMap, std::string* outError)
{
	if (!std::filesystem::exists(dirPath))
	{
		return Fail(outError, "WeaponMasterDataLoader: ディレクトリが存在しません: " + dirPath.string());
	}

	outMap.clear();

	std::error_code ec;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath, ec))
	{
		if (!entry.is_regular_file()) continue;
		if (entry.path().extension() != ".json") continue;

		if (!IsUnderWeaponCategoryFolder(entry.path(), dirPath)) continue;

		if (ec)
		{
			return Fail(outError, "WeaponMasterDataLoader: ディレクトリ走査失敗: " + dirPath.string());
		}

		if (!entry.is_regular_file()) continue;
		if (entry.path().extension() != ".json") continue;

		FWeaponMasterData data;
		std::string err;
		if (!LoadFromFile(entry.path(), data, &err))
		{
			return Fail(outError, err + "(file: " + entry.path().string() + ")");
		}

		const int32_t id = data.coreData.weaponID;
		if (outMap.contains(id))
		{
			return Fail(outError, "WeaponMasterDataLoader: weaponIDが重複しています: " + std::to_string(id));
		}

		outMap.emplace(id, std::move(data));
	}

	return true;
}

/// -------------------------------------------------------------
///		エラーメッセージを設定し、失敗を示す false を返す
/// -------------------------------------------------------------
bool WeaponMasterDataLoader::Fail(std::string* outError, std::string_view msg)
{
	if (outError) *outError = std::string(msg);
	return false;
}
