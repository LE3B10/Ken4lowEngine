#include "WeaponCatalog.h"
#include "Weapon.h"

#include <filesystem>
namespace fs = std::filesystem;

/// -------------------------------------------------------------
///				　		初期化処理
/// -------------------------------------------------------------
void WeaponCatalog::Initialize(const std::string& weaponDir, const std::string& weaponMonolith)
{
	// パス保存
	weaponDir_ = weaponDir;
	weaponMonolith_ = weaponMonolith;

	// ディレクトリ作成
	std::error_code ec;
	fs::create_directories(weaponDir_, ec); // ディレクトリ作成（存在してもOK）

	// 分割版を読み込み
	weaponTable_ = Weapon::LoadFromPath(weaponDir_);

	// 分割版が空ならモノリス版を読み込む
	if (weaponTable_.empty() && !weaponMonolith_.empty())
	{
		weaponTable_ = Weapon::LoadWeapon(weaponMonolith_);
	}
}

/// -------------------------------------------------------------
///				　		全武器データ保存
/// -------------------------------------------------------------
void WeaponCatalog::SaveAll() const
{
	Weapon::SaveToPath(weaponDir_, weaponTable_);
}

/// -------------------------------------------------------------
///				　		強制リロード要求
/// -------------------------------------------------------------
void WeaponCatalog::RequestReload(const std::string& forcusName)
{
	reloadRequested_ = true;
	reloadForcusName_ = forcusName;
}

/// -------------------------------------------------------------
///				　強制リロード要求があれば実行
/// -------------------------------------------------------------
void WeaponCatalog::ApplyReloadIfNeeded(const std::function<void(const WeaponData&)> onForcus)
{
	// リロード要求がなければ無視
	if (!reloadRequested_) return;

	// 再読み込み実行
	weaponTable_ = Weapon::LoadFromPath(weaponDir_);

	// フォーカス武器があればコールバック実行
	if (auto it = weaponTable_.find(reloadForcusName_); it != weaponTable_.end())
		if (onForcus) onForcus(it->second);

	// フラグクリア
	reloadRequested_ = false;
}

/// -------------------------------------------------------------
///				　一意な武器名を作成
/// -------------------------------------------------------------
std::string WeaponCatalog::MakeUniqueName(const std::string& baseName) const
{
	// 一意な名前を作成
	std::string uniqueName = baseName;

	int i = 0;

	// 既存と被らなくなるまでインクリメント
	while (weaponTable_.count(uniqueName) != 0)	uniqueName = baseName + "_" + std::to_string(++i);

	// 完成
	return uniqueName;
}

/// -------------------------------------------------------------
///				　		　 武器データを追加
/// -------------------------------------------------------------
void WeaponCatalog::AddWeapon(const std::string& newName, const WeaponData* base)
{
	// ベースを決める（null ならデフォルトを使う）
	WeaponData weaponData = base ? *base : WeaponData{};

	// 一意な名前を付与してテーブルへ追加
	weaponData.name = MakeUniqueName(newName);

	// テーブルへ追加
	weaponTable_[weaponData.name] = weaponData;

	// JSON保存（ファイル一括書き戻し）
	SaveAll();
}

/// -------------------------------------------------------------
///				　		武器データを削除
/// -------------------------------------------------------------
void WeaponCatalog::RemoveWeapon(const std::string& name)
{
	// テーブルから削除
	weaponTable_.erase(name);

	// JSON保存（ファイル一括書き戻し）
	SaveAll();
}
