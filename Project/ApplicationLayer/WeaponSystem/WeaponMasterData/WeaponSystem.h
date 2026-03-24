#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

#include "WeaponInstance.h"

// マスターデータ
#include "WeaponMasterDataDatabase.h"
#include "WeaponMasterData.h"
#include "WeaponParams.h"

/// -------------------------------------------------------------
///  WeaponSystem
///  - WeaponMasterDataDatabase をロード
///  - MasterData -> WeaponParams へ変換して WeaponInstance に適用
/// -------------------------------------------------------------
class WeaponSystem
{
public:
	/// rootDir は "Resources/JSON/weapons" でも "Resources/JSON" でもOK（内部で補正）
	bool Load(const std::filesystem::path& rootDir, std::string* outError = nullptr);

	/// DBを再読込して、装備中IDが残っていれば再装備。無ければ先頭を装備。
	bool ReloadAndReequip(std::string* outError = nullptr);

	/// 現在装備中のIDをDBから再構築して再適用（EditorのApplyボタン向け）
	bool RebuildEquippedFromDatabase(std::string* outError = nullptr);

	/// 最初の武器IDを装備（ロード後に呼ぶ）
	bool EquipFirst(std::string* outError = nullptr);

	bool EquipById(int32_t weaponId, std::string* outError = nullptr);

	void Tick(float dt) { weapon_.Tick(dt); }

	WeaponInstance& Weapon() { return weapon_; }
	const WeaponInstance& Weapon() const { return weapon_; }

	/// Editor連携用（DBを直接触りたい時）
	WeaponMasterDataDatabase& Database() { return db_; }
	const WeaponMasterDataDatabase& Database() const { return db_; }

	bool IsLoaded() const { return db_.IsLoaded(); }

	std::vector<int32_t> GetWeaponIdListSorted() const { return db_.GetSortedIDList(); }

	// カテゴリ別のIDリスト（昇順）
	std::vector<int32_t> GetWeaponIdListSortedByCategory(EWeaponCategory category) const
	{
		return db_.GetSortedIDListByCategory(category);
	}

	int32_t GetEquippedWeaponId() const { return equippedWeaponId_; }
	bool HasEquippedWeapon() const { return equippedWeaponId_ > 0; }

	const FWeaponReticleData& GetEquippedReticleData() const { return equippedReticleData_; }

	bool EquipNext(std::string* outError = nullptr);
	bool EquipPrev(std::string* outError = nullptr);

private:
	static WeaponParams BuildParams(const FWeaponMasterData& md);
	static std::filesystem::path ResolveWeaponsRoot(const std::filesystem::path& inputPath);

private:
	WeaponMasterDataDatabase db_{};
	WeaponInstance weapon_{};
	int32_t equippedWeaponId_ = 0;

	FWeaponReticleData equippedReticleData_{};
};