#pragma once
#include <string>
#include <functional>
#include <optional>
#include "WeaponMasterDataDatabase.h"
#include "WeaponMasterData.h"
#include <vector>

/// ---------- 武器エディタ用フック群 ---------- ///
struct WeaponEditorHooks
{
	// 保存
	std::function<void()> SaveAll;

	// 再読込の予約（誰を再選択したいか）
	std::function<void(int32_t weaponID)> RequestReloadFocus;

	// クラス変更時
	std::function<void()> RebuildLoadout;

	// 現在装備ならランタイムへ反映
	std::function<void(int32_t weaponID, const FWeaponMasterData&)> ApplyToRuntimeIfCurrent;

	// 追加予約
	std::function<void(const std::string& newDisplayName, int32_t baseWeaponIDOrZero)> RequestAdd;

	// 削除予約
	std::function<void(int32_t weaponID)> RequestDelete;
};


/// -------------------------------------------------------------
///				　		武器マスターデータエディタ
/// -------------------------------------------------------------
class WeaponMasterDataEditor
{
	// 予約（イテレータ破壊防止）
	struct Pending
	{
		bool doSaveAll = false;
		std::optional<int32_t> deleteID;
	}pending_{};

public: /// ---------- メンバ関数 ---------- ///

	// ImGui描画処理
	void DrawImGui(WeaponMasterDataDatabase& database, WeaponEditorHooks& hooks);

private: /// ---------- メンバ関数 ---------- ///

	void DrawToolbar(WeaponMasterDataDatabase& database, WeaponEditorHooks& hooks);

	void DrawList(WeaponMasterDataDatabase& database);

	void DrawInspector(WeaponMasterDataDatabase& database, WeaponEditorHooks& hooks);

	void ProcessPending(WeaponMasterDataDatabase& database, WeaponEditorHooks& hooks);

	void MarkEditedAndMaybeApply(WeaponEditorHooks& hooks, int32_t weaponID, const FWeaponMasterData& data);

	static bool HasAttributes(const std::vector<EWeaponAttribute>& v, EWeaponAttribute attr);

	static bool ToggleAttributes(std::vector<EWeaponAttribute>& v, EWeaponAttribute attr);

private: /// ---------- メンバ変数 ---------- ///

	// UIの状態
	int32_t selectedID_ = 0; // 選択中の武器ID
	bool autoApply_ = true;   // 編集即ランタイム反映フラグ
	bool dirty_ = false;     // 変更されたかどうか

	// --- New時の名前入力（InputText用バッファ）---
	char newNameBuf_[64] = "NewWeapon";

	// Renameもしたい場合に使う（今回は付けておくと便利）
	char renameBuf_[64] = "";

	// 最後のエラーメッセージ
	std::string lastError_;

	// 表示フィルター
	int viewCategory_ = -1; // -1=全て、0～=カテゴリ絞り込み
	bool groupByCategory_ = false;
};

