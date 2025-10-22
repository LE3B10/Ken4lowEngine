#pragma once
#include "WeaponClass.h"
#include "WeaponData.h"

#include <unordered_map>
#include <string>

/// -------------------------------------------------------------
/// 				　		装備セットアップ
/// -------------------------------------------------------------
class Loadout
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// 装備情報を武器情報から再構築
	/// </summary>
	/// <param name="weaponTable"></param>
	void Rebuild(const std::unordered_map<std::string, WeaponData>& weaponTable);

	/// <summary>
	/// クラスに対して装備されている武器名を取得（無ければ空）
	/// </summary>
	/// <param name="className"> 武器のクラス名(プライマリ, バックアップ...) </param>
	/// <param name="weaponTable"> 武器情報 </param>
	/// <returns></returns>
	std::string SelectNameByClass(WeaponClass className, const std::unordered_map<std::string, WeaponData>& weaponTable) const;

	/// <summary>
	/// 指定武器名を装備から外す
	/// </summary>
	void RemoveName(const std::string& weaponName);

	/// <summary>
	/// 装備情報をファイルに保存
	/// </summary>
	/// <param name="filePath"> ファイル名 </param>
	/// <returns></returns>
	bool Save(const std::string& filePath) const;

	/// <summary>
	/// ファイルから装備情報を読み込み
	/// </summary>
	/// <param name="filePath"> ファイル名 </param>
	/// <returns></returns>
	bool Load(const std::string& filePath);

public: /// ---------- アクセサー関数 ---------- ///

	/// <summary>
	/// クラスに対して装備されている武器名を設定
	/// </summary>
	/// <param name="className"> 武器のクラス名(プライマリ, バックアップ...) </param>
	/// <param name="weaponName"> 武器の名前 </param>
	void SetWeaponForClass(WeaponClass className, const std::string& weaponName) { equippedByClass_[className] = weaponName; }

	/// <summary>
	/// 装備マップを取得
	/// </summary>
	const std::unordered_map<WeaponClass, std::string>& GetEquipMap() const { return equippedByClass_; }

private: /// ---------- メンバ変数 ---------- ///

	std::unordered_map<WeaponClass, std::string> equippedByClass_; // クラスごとの装備武器名マップ
};

