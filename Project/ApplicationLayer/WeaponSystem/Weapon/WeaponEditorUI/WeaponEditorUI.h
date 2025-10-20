#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>
#include "WeaponData.h"
#include "WeaponClass.h"
#include "WeaponCatalog.h"
#include "Loadout.h"

/// ---------- 武器エディタ用フック群 ---------- ///
struct WeaponEditorHooks
{
	std::function<void()>                   SaveAll;                    // 保存
	std::function<void(const std::string&)> RequestReloadFocus;        // 再読込の予約（誰を再選択したいか）
	std::function<void()>                   RebuildLoadout;            // クラス変更時
	std::function<void(const WeaponData&)>  ApplyToRuntimeIfCurrent;   // 現在装備ならランタイムへ反映
	std::function<void(const std::string& newName, const std::string& baseNameOrEmpty)> RequestAdd; // 追加予約
	std::function<void(const std::string& name)> RequestDelete;        // 削除予約
};

/// -------------------------------------------------------------
///				　		　武器エディタUI
/// -------------------------------------------------------------
class WeaponEditorUI
{
public: /// ---------- メンバ関数 ---------- ///

	// ImGui描画処理
	void DrawImGui(WeaponCatalog& catalog, Loadout& loadout, const std::string& currentWeaponName, const WeaponEditorHooks& hooks);

private: /// ---------- メンバ関数 ---------- ///

	// 単一武器編集ウィンドウの描画
	void DrawOne(WeaponData& E, const std::string& currentWeaponName, const WeaponEditorHooks& hooks);

	// 追加・削除コントロールの描画
	void DrawAddDeleteControls(WeaponCatalog& catalog, const std::string& currentWeaponName, const WeaponEditorHooks& hooks);

private: /// ---------- メンバ変数 ---------- ///s

	// 武器ごとの「編集ウィンドウが開いているか」状態
	std::unordered_map<std::string, bool> weaponEditorOpen_;
};

