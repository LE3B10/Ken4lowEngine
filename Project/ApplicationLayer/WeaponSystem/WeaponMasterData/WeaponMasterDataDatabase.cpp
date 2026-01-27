#include "WeaponMasterDataDatabase.h"
#include "WeaponMasterDataLoader.h"
#include <string_view>
#include <array>
#include <cstdint>
#include "WeaponMasterData.h"

/// -------------------------------------------------------------
///		指定したディレクトリからデータを読み込む関数
/// -------------------------------------------------------------
bool WeaponMasterDataDatabase::LoadFromDirectory(const std::filesystem::path& dirPath, std::string* outError)
{
	if (!std::filesystem::exists(dirPath))
	{
		return Fail(outError, "WeaponMasterDataDatabase: 指定されたディレクトリが存在しません: " + dirPath.string());
	}

	// weapons 直下の旧式 json は読まない。カテゴリフォルダ配下のみ読む。
	static constexpr std::array<std::string_view, 6> kCategoryFolders = {
		"primary", "backup", "melee", "special", "sniper", "heavy"
	};

	std::unordered_map<int32_t, FWeaponMasterData> tempMap;
	std::unordered_map<int32_t, std::filesystem::path> idToFileMap; // 重複チェック用

	auto loadFolder = [&](const std::filesystem::path& folderPath) -> bool
		{
			if (!std::filesystem::exists(folderPath) || !std::filesystem::is_directory(folderPath))
				return true; // 無いカテゴリはスキップ

			for (const auto& entry : std::filesystem::directory_iterator(folderPath))
			{
				if (!entry.is_regular_file()) continue;
				if (entry.path().extension() != ".json") continue;

				FWeaponMasterData weaponData;
				std::string errorMsg;
				if (!WeaponMasterDataLoader::LoadFromFile(entry.path(), weaponData, &errorMsg))
				{
					return Fail(outError, errorMsg + "(file:" + entry.path().string() + ")");
				}

				const int32_t weaponID = weaponData.coreData.weaponID;
				if (weaponID <= 0)
				{
					return Fail(outError, "WeaponMasterDataDatabase: weaponID must be > 0 (file: " + entry.path().string() + ")");
				}

				if (idToFileMap.contains(weaponID))
				{
					return Fail(outError,
						"WeaponMasterDataDatabase: weaponID duplicate: " + std::to_string(weaponID) +
						"\n  first : " + idToFileMap[weaponID].string() +
						"\n  second: " + entry.path().string());
				}

				idToFileMap.emplace(weaponID, entry.path());
				tempMap.emplace(weaponID, std::move(weaponData));
			}

			return true;
		};

	// ✅ weapons 直下は見ない。primary/backup/... の中だけ読む。
	for (auto sv : kCategoryFolders)
	{
		const std::filesystem::path folder = dirPath / std::string(sv);
		if (!loadFolder(folder))
		{
			return false;
		}
	}

	// 読めたものだけ反映
	weaponDataMap_ = std::move(tempMap);
	sourceDirectory_ = dirPath; // Reload は root(".../weapons") を基準にする
	isLoaded_ = true;
	return true;
}

/// -------------------------------------------------------------
///					リロード処理を実行します
/// -------------------------------------------------------------
bool WeaponMasterDataDatabase::Reload(std::string* outError)
{
	if (!isLoaded_ || sourceDirectory_.empty())
	{
		return Fail(outError, "WeaponMasterDataDatabase: データがロードされていないか、ソースディレクトリが不明です。");
	}
	return LoadFromDirectory(sourceDirectory_, outError);
}

/// -------------------------------------------------------------
///			オブジェクトやコンテナの内容をクリアする
/// -------------------------------------------------------------
void WeaponMasterDataDatabase::Clear()
{
	weaponDataMap_.clear();
	isLoaded_ = false;
	sourceDirectory_.clear();
}

/// -------------------------------------------------------------
///		新しい32ビットIDを生成して返すメンバー関数
/// -------------------------------------------------------------
int32_t WeaponMasterDataDatabase::CreateNewID()
{
	const int32_t id = GenerateNewID();
	FWeaponMasterData data{};
	data.coreData.weaponID = id;
	data.coreData.weaponName = "Weapon" + std::to_string(id);
	weaponDataMap_[id] = std::move(data);
	return id;
}

/// -------------------------------------------------------------
///		指定されたIDのオブジェクトを複製し、新しいオブジェクトのIDを返します
/// -------------------------------------------------------------
int32_t WeaponMasterDataDatabase::Duplicate(int32_t srcID)
{
	auto it = weaponDataMap_.find(srcID);
	if (it == weaponDataMap_.end()) return 0;

	const int32_t id = GenerateNewID();
	FWeaponMasterData copy = it->second;
	copy.coreData.weaponID = id;
	copy.coreData.weaponName = "_Copy_" + std::to_string(id);
	weaponDataMap_[id] = std::move(copy);
	return id;
}

/// -------------------------------------------------------------
///		指定された武器IDに対応する要素を削除
/// -------------------------------------------------------------
bool WeaponMasterDataDatabase::RemoveByID(int32_t weaponID)
{
	return weaponDataMap_.erase(weaponID) > 0;
}

/// -------------------------------------------------------------
///		指定された名前を持つ新しい武器マスターデータを作成し、そのIDを返す
/// -------------------------------------------------------------
int32_t WeaponMasterDataDatabase::CreateNewWithName(const std::string& name)
{
	const int32_t id = GenerateNewID();

	FWeaponMasterData data{};
	data.coreData.weaponID = id;

	data.coreData.weaponName = name.empty() ? "Weapon" + std::to_string(id) : name;

	weaponDataMap_[id] = std::move(data);
	return id;
}

/// -------------------------------------------------------------
///		指定された武器IDに対応する武器マスターデータを検索
/// -------------------------------------------------------------
const FWeaponMasterData* WeaponMasterDataDatabase::FindByID(int32_t weaponID) const
{
	auto it = weaponDataMap_.find(weaponID);
	if (it == weaponDataMap_.end()) return nullptr;
	return &it->second;
}

/// -------------------------------------------------------------
///		指定した武器IDに対応する変更可能な FWeaponMasterData オブジェクトを検索
/// -------------------------------------------------------------
FWeaponMasterData* WeaponMasterDataDatabase::FindMutableByID(int32_t weaponID)
{
	auto it = weaponDataMap_.find(weaponID);
	if (it == weaponDataMap_.end()) return nullptr;
	return &it->second;
}

/// -------------------------------------------------------------
///		指定した武器IDがコレクションに含まれているかを判定
/// -------------------------------------------------------------
bool WeaponMasterDataDatabase::ContainsID(int32_t weaponID) const
{
	return weaponDataMap_.contains(weaponID);
}

/// -------------------------------------------------------------
///	指定した武器IDに対応する FWeaponMasterData への const 参照を取得
/// -------------------------------------------------------------
const FWeaponMasterData& WeaponMasterDataDatabase::GetByID(int32_t weaponID) const
{
	const FWeaponMasterData* data = FindByID(weaponID);

	if (!data)
	{
		throw std::runtime_error("WeaponMasterDataDatabase: 指定された weaponID に対応するデータが存在しません: " + std::to_string(weaponID));
	}
	return *data;
}

/// -------------------------------------------------------------
///			昇順にソートされた ID のリストを返す
/// -------------------------------------------------------------
std::vector<int32_t> WeaponMasterDataDatabase::GetSortedIDList() const
{
	std::vector<int32_t> ids;
	ids.reserve(weaponDataMap_.size());
	for (const auto& [id, _] : weaponDataMap_) ids.push_back(id);
	std::sort(ids.begin(), ids.end());
	return ids;
}

/// -------------------------------------------------------------
///		エラーメッセージを設定し、失敗を示す false を返す
/// -------------------------------------------------------------
bool WeaponMasterDataDatabase::Fail(std::string* outError, std::string_view msg)
{
	if (outError) *outError = std::string(msg);
	return false;
}

/// -------------------------------------------------------------
///		新しい32ビットIDを生成して返すメンバー関数
/// -------------------------------------------------------------
int32_t WeaponMasterDataDatabase::GenerateNewID() const
{
	int32_t maxId = 0;
	for (const auto& [id, _] : weaponDataMap_) maxId = std::max(maxId, id);
	return maxId + 1;
}
