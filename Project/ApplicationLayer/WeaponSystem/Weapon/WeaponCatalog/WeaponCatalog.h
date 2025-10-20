#pragma once
#include <unordered_map>
#include <string>
#include <functional>
#include "WeaponData.h"
#include "WeaponClass.h"


/// -------------------------------------------------------------
///				　		武器カタログクラス
/// -------------------------------------------------------------
class WeaponCatalog
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// 初期化処理
	/// </summary>
	/// <param name="weaponDir"> 分割Jsonフォルダ </param>
	/// <param name="weaponMonolith"> 旧モノリスJson（空なら無視） </param>
	void Initialize(const std::string& weaponDir, const std::string& weaponMonolith);

	// 保存・リロード処理
	void SaveAll() const;															 // 全武器データ保存
	void RequestReload(const std::string& forcusName);								 // 強制リロード要求
	void ApplyReloadIfNeeded(const std::function<void(const WeaponData&)> onForcus); // 強制リロード要求があれば実行

	// 在庫操作
	std::string MakeUniqueName(const std::string& baseName) const; // 一意な武器名を作成
	void AddWeapon(const std::string& newName, const WeaponData* base = nullptr);            // 武器データを追加
	void RemoveWeapon(const std::string& name);             // 武器データを削除

public: /// ---------- アクセサ ---------- ///

	// 武器名が存在するか
	bool Exists(const std::string& name) const { return weaponTable_.count(name) != 0; }

	// 武器データを探す
	const WeaponData* Find(const std::string& name) const { auto it = weaponTable_.find(name); return it == weaponTable_.end() ? nullptr : &it->second; }

	const std::string& DirectoryPath() const { return weaponDir_; } // ディレクトリパス取得
	const std::string& MonolithPath() const { return weaponMonolith_; } // モノリスパス取得

	// 全データ取得
	const std::unordered_map<std::string, WeaponData>& All() const { return weaponTable_; }

	// 全データ取得（編集用）
	std::unordered_map<std::string, WeaponData>& All() { return weaponTable_; }

	// 武器データ編集取得
	WeaponData* Edit(const std::string& name) { auto it = weaponTable_.find(name); return it == weaponTable_.end() ? nullptr : &it->second; }

private: /// ---------- メンバ変数 ---------- ///

	std::unordered_map<std::string, WeaponData> weaponTable_; // 武器データテーブル
	std::string weaponDir_;									// 分割Jsonフォルダパス
	std::string weaponMonolith_;								// 旧モノリスJsonパス
	bool reloadRequested_ = false;							// リロード要求フラグ
	std::string reloadForcusName_;							// リロード対象武器名
};

