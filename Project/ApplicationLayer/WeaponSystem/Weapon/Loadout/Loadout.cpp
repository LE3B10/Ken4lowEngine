#include "Loadout.h"
#include <json.hpp>
#include <fstream>

using nlohmann::json;

/// ---------- クラス名ラベル配列 ---------- ///
static const char* kClassLabels[] = { "Primary","Backup","Melee","Special","Sniper","Heavy" };

/// ---------- WeaponClass → 文字列変換 ---------- ///
static std::string ClassToString(WeaponClass c)
{
	size_t i = static_cast<size_t>(c); // 安全のため size_t に変換
	if (i < 6) return kClassLabels[i]; // 配列範囲内
	return "Unknown";				   // 範囲外
}

/// ---------- 文字列 → WeaponClass 変換 ---------- ///
static bool StringToClass(const std::string& s, WeaponClass& out)
{
	// 大文字小文字無視で比較
	for (int i = 0; i < 6; i++) { if (s == kClassLabels[i]) { out = static_cast<WeaponClass>(i); return true; } }
	return false;
}

/// -------------------------------------------------------------
/// 				装備情報を武器情報から再構築
/// -------------------------------------------------------------
void Loadout::Rebuild(const std::unordered_map<std::string, WeaponData>& weaponTable)
{
	// 新しい装備マップ
	std::unordered_map<WeaponClass, std::string> nextEquipMap;

	for (auto& [cls, name] : equippedByClass_) {
		auto it = weaponTable.find(name);
		if (it != weaponTable.end() && it->second.clazz == cls)
		{
			nextEquipMap[cls] = name;
		}
	}

	for (auto& [name, w] : weaponTable) {
		if (!nextEquipMap.count(w.clazz)) nextEquipMap[w.clazz] = name;
	}

	// 置き換え
	equippedByClass_.swap(nextEquipMap);
}

/// -------------------------------------------------------------
///		クラスに対して装備されている武器名を取得（無ければ空）
/// -------------------------------------------------------------
std::string Loadout::SelectNameByClass(WeaponClass className, const std::unordered_map<std::string, WeaponData>& weaponTable) const
{
	// クラス→名前マップに存在するか？
	if (auto it = equippedByClass_.find(className); it != equippedByClass_.end()) return it->second;

	// 存在しないなら武器テーブルを走査して探す
	for (auto& [name, w] : weaponTable) if (w.clazz == className) return name;

	// 見つからなければ空文字列を返す
	return {};
}

/// -------------------------------------------------------------
/// 				指定武器名を装備から外す
/// -------------------------------------------------------------
void Loadout::RemoveName(const std::string& weaponName)
{
	// クラス→名前マップを走査して、該当武器名を削除
	for (auto it = equippedByClass_.begin(); it != equippedByClass_.end(); )
	{
		// 該当武器名なら削除
		if (it->second == weaponName) it = equippedByClass_.erase(it);
		else ++it; // 次へ
	}
}

/// -------------------------------------------------------------
/// 				装備情報をファイルに保存
/// -------------------------------------------------------------
bool  Loadout::Save(const std::string& filePath) const
{
	json j = json::object();
	for (auto& [cls, name] : equippedByClass_) j[ClassToString(cls)] = name;
	std::ofstream ofs(filePath);
	if (!ofs) return false;
	ofs << j.dump(2);
	return true;
}

/// -------------------------------------------------------------
/// 				ファイルから装備情報を読み込み
/// -------------------------------------------------------------
bool  Loadout::Load(const std::string& filePath)
{
	std::ifstream ifs(filePath); // ファイルを開く
	if (!ifs) return false;		 // ファイルオープン失敗
	json j; ifs >> j;			 // JSONパース

	// 既存の装備マップをクリア
	equippedByClass_.clear();

	// JSONオブジェクトを走査してクラス→名前マップを再構築
	for (auto& [k, v] : j.items())
	{
		WeaponClass cls;

		// 文字列→クラス変換に成功したら登録
		if (StringToClass(k, cls)) equippedByClass_[cls] = v.get<std::string>();
	}

	// 成功
	return true;
}
