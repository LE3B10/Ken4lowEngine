#pragma once
#include <json.hpp>
#include <fstream>
#include <string>
#include <unordered_map>

#include "WeaponData.h"
#include <Vector3.h>

// 実行時に変化する状態
struct WeaponState
{
	int   ammoInMag = 0;
	int   reserveAmmo = 0;
	bool  reloading = false;
	float reloadTimer = 0.0f;
	float cooldown = 0.0f;  // 射撃インターバル管理
};

/// --------------------------------------------
///					武器クラス
/// --------------------------------------------
class Weapon
{
public: /// ---------- メンバ関数 ---------- ///

	// 最低限データ参照と上体を持つ
	explicit Weapon(const WeaponData& data);

	// 武器データの読み込み
	static std::unordered_map<std::string, WeaponData> LoadWeapon(const std::string& filePath);

	// 武器データの保存
	static bool SaveWeapons(const std::string& filePath, const std::unordered_map<std::string, WeaponData>& weaponTable);

	// 武器データの読み込み（短縮版）
	static std::unordered_map<std::string, WeaponData> LoadFromPath(const std::string& filePath);

	// 武器データの保存（短縮版）
	static bool SaveToPath(const std::string& filePath, const std::unordered_map<std::string, WeaponData>& weaponTable);

	// 単一武器データの保存
	static bool SaveOne(const std::string& filePath, const WeaponData& weaponData);

	// 移行ヘルパ（巨大->分離）
	static bool MigrateMonolithToDir(const std::string& monolithJson, const std::string& outDir);

public: /// ---------- アクセサ ---------- ///

	// データを取得
	const WeaponData& Data() const { return weaponData_; }

private: /// ---------- メンバ変数 ---------- ///

	WeaponData weaponData_;		// 武器データ
	WeaponState weaponState_;	// 武器状態
	float fireInterval_ = 0.0f; // 発射間隔管理
};

