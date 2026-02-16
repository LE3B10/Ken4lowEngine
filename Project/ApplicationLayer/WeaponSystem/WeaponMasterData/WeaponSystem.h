#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

#include "WeaponInstance.h"

// マスターデータ
#if __has_include("WeaponMasterData/WeaponMasterDataDatabase.h")
#include "WeaponMasterData/WeaponMasterDataDatabase.h"
#elif __has_include("WeaponMasterDataDatabase.h")
#include "WeaponMasterDataDatabase.h"
#else
// プロジェクト側のincludeパスに合わせて修正してください
#include "WeaponMasterDataDatabase.h"
#endif
#include "WeaponMasterData.h"

/// -------------------------------------------------------------
///  WeaponSystem
///  - WeaponMasterDataDatabase をロード
///  - MasterData -> WeaponParams へ変換して WeaponInstance に適用
/// -------------------------------------------------------------
class WeaponSystem
{
public:
	bool Load(const std::filesystem::path& rootDir, std::string* outError = nullptr);

	/// 最初の武器IDを装備（ロード後に呼ぶ）
	bool EquipFirst(std::string* outError = nullptr);

	bool EquipById(int32_t weaponId, std::string* outError = nullptr);

	void Tick(float dt) { weapon_.Tick(dt); }

	WeaponInstance& Weapon() { return weapon_; }
	const WeaponInstance& Weapon() const { return weapon_; }

	bool IsLoaded() const { return db_.IsLoaded(); }

	std::vector<int32_t> GetWeaponIdListSorted() const { return db_.GetSortedIDList(); }

	// カテゴリ別のIDリスト（昇順）
	std::vector<int32_t> GetWeaponIdListSortedByCategory(EWeaponCategory category) const { return db_.GetSortedIDListByCategory(category); }

	int32_t GetEquippedWeaponId() const { return equippedWeaponId_; }

private:
	static WeaponParams BuildParams(const FWeaponMasterData& md);

private:
	WeaponMasterDataDatabase db_{};
	WeaponInstance weapon_{};
	int32_t equippedWeaponId_ = 0;
};
