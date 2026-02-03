#include "WeaponMasterDataEditor.h"

#ifdef USE_IMGUI
#include <imgui.h>

#endif // USE_IMGUI

namespace
{
	static const char* kCategoryNames[] = { "プライマリ","バックアップ","近接","特殊","スナイパー","ヘビー" };
	static const char* kRarityNames[] = { "コモン", "レア", "エピック", "レジェンダりー", "ミシカル" };
	static const char* kAmmoNames[] = { "デフォルト", "エネルギー", "爆薬", "なし" };

	// 文字列プリセット
	static const char* kCurrencyPresets[] = { "ゴールド", "ジェム", "コイン" };

	/// ---------- 特殊一覧 ---------- ///
	static const EWeaponAttribute kAttrList[] = {
		EWeaponAttribute::Poison,
		EWeaponAttribute::Burning,
		EWeaponAttribute::AreaDamage,
		EWeaponAttribute::Bouncing,
		EWeaponAttribute::LifeSteal,
		EWeaponAttribute::WallBreak,
		EWeaponAttribute::FixedDelay,
	};
	static const char* kAttrNames[] = { "毒", "発火", "エリアダメージ", "反射", "吸血", "壁貫通", "固定ディレイ" };

	static std::vector<int32_t> BuildSortedIDs(const WeaponMasterDataDatabase& database)
	{
		std::vector<int32_t> ids;
		ids.reserve(database.GetAll().size());
		for (const auto& kv : database.GetAll())
		{
			ids.push_back(kv.first);
		}
		std::sort(ids.begin(), ids.end());
		return ids;
	}
}

/// -------------------------------------------------------------
///				　		武器マスターデータエディタ
/// -------------------------------------------------------------
void WeaponMasterDataEditor::DrawImGui(WeaponMasterDataDatabase& database, WeaponEditorHooks& hooks)
{
#ifdef USE_IMGUI
	ImGui::Begin("武器マスターデータエディタ");

	DrawToolbar(database, hooks);

	ImGui::Columns(2, nullptr, true);
	DrawList(database);
	ImGui::NextColumn();
	DrawInspector(database, hooks);
	ImGui::Columns(1);

	ImGui::End();

	ProcessPending(database, hooks); // Endの後にやるのが安全
#endif // USE_IMGUI
}

void WeaponMasterDataEditor::DrawToolbar(WeaponMasterDataDatabase& database, WeaponEditorHooks& hooks)
{
#ifdef USE_IMGUI
	ImGui::Checkbox("Auto Apply", &autoApply_);
	ImGui::SameLine();

	if (ImGui::Button("Save All")) { pending_.doSaveAll = true; }
	ImGui::SameLine();

	if (ImGui::Button("Reload"))
	{
		const int32_t keepId = selectedID_;
		std::string err;

		const std::filesystem::path dir = "Resources/JSON/weapons";
		if (!database.LoadFromDirectory(dir, &err))
		{
			lastError_ = err;
		}
		else
		{
			lastError_.clear();

			if (keepId != 0 && database.ContainsID(keepId))
				selectedID_ = keepId;
			else
				selectedID_ = database.GetAll().empty() ? 0 : database.GetAll().begin()->first;

			if (hooks.RebuildLoadout) hooks.RebuildLoadout();
		}
	}

	ImGui::SameLine();
	ImGui::Checkbox("K4E::Group", &groupByCategory_);

	ImGui::SameLine();
	const char* viewLabel = (viewCategory_ < 0) ? "All" : kCategoryNames[viewCategory_];
	if (ImGui::BeginCombo("View##Category", viewLabel))
	{
		bool selAll = (viewCategory_ < 0);
		if (ImGui::Selectable("All", selAll)) viewCategory_ = -1;
		if (selAll) ImGui::SetItemDefaultFocus();

		for (int i = 0; i < (int)IM_ARRAYSIZE(kCategoryNames); ++i)
		{
			bool sel = (viewCategory_ == i);
			if (ImGui::Selectable(kCategoryNames[i], sel)) viewCategory_ = i;
			if (sel) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	ImGui::TextUnformatted(dirty_ ? "*dirty" : " ");

	// 件数表示（全体 / 表示中）
	auto ids = BuildSortedIDs(database);
	const int totalCount = (int)ids.size();
	int viewCount = 0;
	for (int32_t id : ids)
	{
		const FWeaponMasterData* d = database.FindByID(id);
		if (!d) continue;
		if (viewCategory_ >= 0 && (int)d->coreData.category != viewCategory_) continue;
		++viewCount;
	}
	ImGui::Text("Weapons : %d  (view:%d)   Selected : %d", totalCount, viewCount, selectedID_);

	if (!lastError_.empty())
	{
		ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s", lastError_.c_str());
	}
#endif // USE_IMGUI

}

void WeaponMasterDataEditor::DrawList(WeaponMasterDataDatabase& database)
{
#ifdef USE_IMGUI
	if (ImGui::Button("新規作成"))
	{
		std::snprintf(newNameBuf_, sizeof(newNameBuf_), "NewWeapon");
		ImGui::OpenPopup("武器を作成");
	}
	ImGui::SameLine();

	if (ImGui::Button("複製") && selectedID_ != 0)
	{
		const int32_t newId = database.Duplicate(selectedID_);
		if (newId != 0) { selectedID_ = newId; dirty_ = true; }
	}
	ImGui::SameLine();

	if (ImGui::Button("削除") && selectedID_ != 0)
	{
		pending_.deleteID = selectedID_;
	}

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	// Popup本体（OpenPopupした名前と完全一致させる）
	if (ImGui::BeginPopupModal("武器を作成", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("武器名を入力してください");
		ImGui::InputText("##NewWeaponName", newNameBuf_, sizeof(newNameBuf_)); // Flags無し

		ImGui::Separator();

		if (ImGui::Button("作成"))
		{
			const std::string name = newNameBuf_;
			const int32_t id = database.CreateNewWithName(name); // これが無ければ CreateNewID→名前代入でも可

			selectedID_ = id;
			dirty_ = true;

			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("キャンセル"))
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	ImGui::Separator();

	auto ids = BuildSortedIDs(database);

	auto passFilter = [&](const FWeaponMasterData& d) -> bool
		{
			if (viewCategory_ < 0) return true;
			return (int)d.coreData.category == viewCategory_;
		};

	ImGui::BeginChild("武器リスト", ImVec2(0, 0), true);

	if (groupByCategory_)
	{
		for (int cat = 0; cat < (int)IM_ARRAYSIZE(kCategoryNames); ++cat)
		{
			if (viewCategory_ >= 0 && cat != viewCategory_) continue;

			if (ImGui::CollapsingHeader(kCategoryNames[cat], ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (int32_t id : ids)
				{
					const FWeaponMasterData* data = database.FindByID(id);
					if (!data) continue;
					if ((int)data->coreData.category != cat) continue;
					if (!passFilter(*data)) continue;

					const bool selected = (id == selectedID_);
					std::string label = std::to_string(id) + " : " + data->coreData.weaponName;
					if (ImGui::Selectable(label.c_str(), selected)) selectedID_ = id;
				}
			}
		}
	}
	else
	{
		for (int32_t id : ids)
		{
			const FWeaponMasterData* data = database.FindByID(id);
			if (!data) continue;
			if (!passFilter(*data)) continue;

			const bool selected = (id == selectedID_);
			std::string label =
				std::to_string(id) + " : [" + kCategoryNames[(int)data->coreData.category] + "] " + data->coreData.weaponName;

			if (ImGui::Selectable(label.c_str(), selected)) selectedID_ = id;
		}
	}

	ImGui::EndChild();
#endif // USE_IMGUI

}

void WeaponMasterDataEditor::DrawInspector(WeaponMasterDataDatabase& database, WeaponEditorHooks& hooks)
{
#ifdef USE_IMGUI
	if (selectedID_ == 0)
	{
		ImGui::TextDisabled("左のリストから武器を選択してください");
		return;
	}

	FWeaponMasterData* data = database.FindMutableByID(selectedID_);
	if (!data)
	{
		ImGui::TextDisabled("見つかりませんでした");
		return;
	}

	// 適応ボタン
	if (ImGui::Button("ランタイム適用"))
	{
		if (hooks.ApplyToRuntimeIfCurrent) hooks.ApplyToRuntimeIfCurrent(selectedID_, *data);
	}

	ImGui::Separator();

	/// ---------- 武器の基本データ ---------- ///
	if (ImGui::CollapsingHeader("基本設定", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("武器ID: %d", data->coreData.weaponID);
		ImGui::Text("武器名: %s", data->coreData.weaponName.c_str());

		ImGui::SameLine();
		if (ImGui::Button("Rename"))
		{
			std::snprintf(renameBuf_, sizeof(renameBuf_), "%s", data->coreData.weaponName.c_str());
			ImGui::OpenPopup("Rename Weapon");
		}

		if (ImGui::BeginPopupModal("Rename Weapon", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("新しい武器名:");
			ImGui::InputText("##RenameWeaponName", renameBuf_, sizeof(renameBuf_));

			ImGui::Separator();
			if (ImGui::Button("OK"))
			{
				data->coreData.weaponName = renameBuf_;
				MarkEditedAndMaybeApply(hooks, selectedID_, *data);
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		int cat = (int)data->coreData.category;
		if (ImGui::Combo("カテゴリー", &cat, kCategoryNames, IM_ARRAYSIZE(kCategoryNames)))
		{
			data->coreData.category = (EWeaponCategory)cat;
			dirty_ = true;
			if (hooks.RebuildLoadout) hooks.RebuildLoadout();
			MarkEditedAndMaybeApply(hooks, selectedID_, *data);
		}

		int rar = (int)data->coreData.rarity;
		if (ImGui::Combo("レアリティ", &rar, kRarityNames, IM_ARRAYSIZE(kRarityNames)))
		{
			data->coreData.rarity = (EWeaponRarity)rar;
			MarkEditedAndMaybeApply(hooks, selectedID_, *data);
		}
	}

	/// ---------- 経済データ ---------- ///
	if (ImGui::CollapsingHeader("経済設定", ImGuiTreeNodeFlags_DefaultOpen))
	{
		int curIdx = -1;
		for (int i = 0; i < (int)IM_ARRAYSIZE(kCurrencyPresets); ++i)
		{
			if (data->economyData.currencyType == kCurrencyPresets[i]) { curIdx = i; break; }
		}
		const char* curPreview = (curIdx >= 0) ? kCurrencyPresets[curIdx] :
			(data->economyData.currencyType.empty() ? "空" : data->economyData.currencyType.c_str());

		if (ImGui::BeginCombo("通貨の種類", curPreview))
		{
			for (int i = 0; i < (int)IM_ARRAYSIZE(kCurrencyPresets); ++i)
			{
				bool sel = (i == curIdx);
				if (ImGui::Selectable(kCurrencyPresets[i], sel))
				{
					data->economyData.currencyType = kCurrencyPresets[i];
					MarkEditedAndMaybeApply(hooks, selectedID_, *data);
				}
				if (sel) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear##通貨"))
		{
			data->economyData.currencyType.clear();
			MarkEditedAndMaybeApply(hooks, selectedID_, *data);
		}

		if (ImGui::DragInt("購入価格", &data->economyData.purchasePrice, 1.0f, 0, 1000000)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
		if (ImGui::DragInt("解放に必要なレベル", &data->economyData.minLevelToUnlock, 1.0f, 0, 999)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
		if (ImGui::Checkbox("期間限定", &data->economyData.bIsLimitedTime)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
		if (ImGui::SliderFloat("割引率", &data->economyData.discountRate, 0.0f, 1.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);

		if (ImGui::TreeNode("アップグレード費用"))
		{
			for (int i = 0; i < (int)data->economyData.upgradeCosts.size(); ++i) // ★修正: 条件が壊れてた
			{
				ImGui::PushID(i);
				if (ImGui::DragInt("費用", &data->economyData.upgradeCosts[i], 1.0f, 0, 1000000))
					MarkEditedAndMaybeApply(hooks, selectedID_, *data);

				ImGui::SameLine();
				if (ImGui::Button("X"))
				{
					data->economyData.upgradeCosts.erase(data->economyData.upgradeCosts.begin() + i);
					MarkEditedAndMaybeApply(hooks, selectedID_, *data);
					ImGui::PopID();
					break;
				}
				ImGui::PopID();
			}
			if (ImGui::Button("+ 追加"))
			{
				data->economyData.upgradeCosts.push_back(0);
				MarkEditedAndMaybeApply(hooks, selectedID_, *data);
			}
			ImGui::TreePop();
		}
	}

	/// ---------- 射撃性能データ ---------- ///
	if (ImGui::CollapsingHeader("ステータス設定", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::DragFloat("ダメージ", &data->stats.damage, 0.1f, 0.0f, 100000.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
		if (ImGui::DragFloat("発射速度(RPM)", &data->stats.fireRate, 0.1f, 0.0f, 3000.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
		if (ImGui::SliderFloat("機動力", &data->stats.mobility, 0.0f, 1.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
		if (ImGui::DragInt("弾薬容量", &data->stats.capacity, 1.0f, 0, 9999)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);

		int ammo = (int)data->stats.ammoType;
		if (ImGui::Combo("弾薬タイプ", &ammo, kAmmoNames, IM_ARRAYSIZE(kAmmoNames)))
		{
			data->stats.ammoType = (EAmmoType)ammo;
			MarkEditedAndMaybeApply(hooks, selectedID_, *data);
		}

		if (ImGui::DragFloat("リロード時間(秒)", &data->stats.reloadTime, 0.01f, 0.0f, 60.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
		if (ImGui::DragInt("消費弾薬数/発", &data->stats.ammoPerShot, 1.0f, 0, 100)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
		if (ImGui::DragInt("予備弾薬上限", &data->stats.maxReserveAmmo, 1.0f, 0, 99999)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);

		if (ImGui::SliderFloat("クリティカル率", &data->stats.criticalChance, 0.0f, 1.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
		if (ImGui::DragFloat("ヘッド倍率", &data->stats.headshotMultiplier, 0.01f, 0.0f, 60.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
	}

	/// ---------- 操作・反動データ ---------- ///
	if (ImGui::CollapsingHeader("操作・反動設定", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::SliderFloat("精度", &data->handling.accuracy, 0.0f, 1.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);

		if (ImGui::DragFloat("拡散増加量", &data->handling.spreadIncrease, 0.01f, 0.0f, 10.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
		if (ImGui::DragFloat("垂直反動", &data->handling.verticalRecoil, 0.01f, 0.0f, 100.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
		if (ImGui::DragFloat("水平反動", &data->handling.horizontalRecoil, 0.01f, 0.0f, 100.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
		if (ImGui::DragFloat("反動回復", &data->handling.recoilRecovery, 0.01f, 0.0f, 100.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);

		if (ImGui::DragFloat("ADS視野角", &data->handling.adsZoomFov, 0.1f, 1.0f, 179.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
		if (ImGui::DragFloat("ADS移行速度", &data->handling.adsTransitionSpeed, 0.01f, 0.0f, 100.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
		if (ImGui::DragFloat("固定ディレイ", &data->handling.fixedDelayTime, 0.01f, 0.0f, 10.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
		if (ImGui::DragFloat("スコープ倍率", &data->handling.zoomLevel, 0.01f, 0.0f, 50.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
	}

	/// ---------- アセット・サウンドデータ ---------- ///
	if (ImGui::CollapsingHeader("アセット", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("モデルパス : %s", data->assetData.modelPath.empty() ? "空" : data->assetData.modelPath.c_str());
		ImGui::SameLine();
		if (ImGui::Button("Clear##モデル")) { data->assetData.modelPath.clear(); MarkEditedAndMaybeApply(hooks, selectedID_, *data); }

		ImGui::Text("アイコンパス : %s", data->assetData.iconPath.empty() ? "空" : data->assetData.iconPath.c_str());
		ImGui::SameLine();
		if (ImGui::Button("Clear##アイコン")) { data->assetData.iconPath.clear(); MarkEditedAndMaybeApply(hooks, selectedID_, *data); }
	}

	if (ImGui::CollapsingHeader("サウンド", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto ClearLine = [&](const char* label, std::string& s, const char* btnId)
			{
				ImGui::Text("%s : %s", label, s.empty() ? "空" : s.c_str());
				ImGui::SameLine();
				if (ImGui::Button(btnId)) { s.clear(); MarkEditedAndMaybeApply(hooks, selectedID_, *data); }
			};

		ClearLine("発砲音", data->soundData.fireSoundPath, "Clear##発砲音");
		ClearLine("リロード音", data->soundData.reloadSoundPath, "Clear##リロード音");
		ClearLine("空砲音", data->soundData.emptySoundPath, "Clear##空砲音");
		ClearLine("装備音", data->soundData.equipSoundPath, "Clear##装備音");
		ClearLine("ヒット音", data->soundData.impactSoundPath, "Clear##ヒット音");
	}

	/// ---------- オプションデータ ---------- ///
	if (ImGui::CollapsingHeader("オプション", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bool hasProjectile = data->projectileData.has_value();
		if (ImGui::Checkbox("弾道・判定詳細", &hasProjectile))
		{
			data->projectileData = hasProjectile ? std::make_optional<FWeaponProjectileData>() : std::nullopt;
			MarkEditedAndMaybeApply(hooks, selectedID_, *data);
		}
		if (data->projectileData)
		{
			ImGui::Indent();
			if (ImGui::DragFloat("最大射程", &data->projectileData->maxRange, 0.1f, 0.0f, 10000.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
			if (ImGui::Checkbox("弾道武器有効", &data->projectileData->bIsProjectile)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
			if (ImGui::DragFloat("弾速", &data->projectileData->projectileSpeed, 0.1f, 0.0f, 100000.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
			if (ImGui::DragInt("貫通回数", &data->projectileData->pierceCount, 1.0f, 0, 1000)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
			if (ImGui::DragInt("反射回数", &data->projectileData->ricochetCount, 1.0f, 0, 1000)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
			if (ImGui::DragFloat("爆風半径", &data->projectileData->splashRadius, 0.1f, 0.0f, 10000.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
			if (ImGui::Checkbox("自己ダメ有効", &data->projectileData->bCanDamageSelf)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
			ImGui::Unindent();
		}

		bool hasBurst = data->burstSettings.has_value();
		if (ImGui::Checkbox("バースト設定", &hasBurst))
		{
			data->burstSettings = hasBurst ? std::make_optional<FBurstSettings>() : std::nullopt;
			MarkEditedAndMaybeApply(hooks, selectedID_, *data);
		}
		if (data->burstSettings)
		{
			ImGui::Indent();
			if (ImGui::DragInt("射撃回数", &data->burstSettings->count, 1.0f, 0, 100)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
			if (ImGui::DragFloat("射撃間隔", &data->burstSettings->interval, 0.01f, 0.0f, 10.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
			ImGui::Unindent();
		}

		bool hasCharge = data->chargeSettings.has_value();
		if (ImGui::Checkbox("チャージ設定", &hasCharge))
		{
			data->chargeSettings = hasCharge ? std::make_optional<FChargeSettings>() : std::nullopt;
			MarkEditedAndMaybeApply(hooks, selectedID_, *data);
		}
		if (data->chargeSettings)
		{
			ImGui::Indent();
			if (ImGui::DragFloat("最大チャージ(銃器)", &data->chargeSettings->maxChargeTime, 0.01f, 0.0f, 30.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
			ImGui::Unindent();
		}

		// 近接：Checkboxでoptional生成/破棄 → optionalがあるときだけUI表示
		bool hasMelee = data->meleeData.has_value();
		if (ImGui::Checkbox("近接武器設定", &hasMelee))
		{
			data->meleeData = hasMelee ? std::make_optional<FMeleeWeaponData>() : std::nullopt;
			MarkEditedAndMaybeApply(hooks, selectedID_, *data);
		}
		if (data->meleeData)
		{
			ImGui::Indent();
			if (ImGui::DragFloat("攻撃範囲", &data->meleeData->attackRange, 0.01f, 0.0f, 1000.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
			if (ImGui::DragFloat("攻撃角度", &data->meleeData->attackArc, 0.1f, 0.0f, 360.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
			if (ImGui::DragFloat("垂直角度", &data->meleeData->verticalAngle, 0.1f, -180.0f, 180.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);

			if (ImGui::DragFloat("発生", &data->meleeData->startupDelay, 0.01f, 0.0f, 10.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
			if (ImGui::DragFloat("持続", &data->meleeData->activeFrames, 0.01f, 0.0f, 10.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
			if (ImGui::DragFloat("硬直", &data->meleeData->recoveryDelay, 0.01f, 0.0f, 10.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);

			if (ImGui::DragInt("最大コンボ数", &data->meleeData->maxComboCount, 1.0f, 0, 20)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
			if (ImGui::DragFloat("コンボ猶予", &data->meleeData->comboWindow, 0.01f, 0.0f, 10.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);

			if (ImGui::DragFloat("前進速度", &data->meleeData->dashImpulse, 0.01f, 0.0f, 100.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
			if (ImGui::Checkbox("チャージ攻撃", &data->meleeData->bCanCharge)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
			if (ImGui::DragFloat("最大チャージ(近接)", &data->meleeData->maxChargeTime, 0.01f, 0.0f, 30.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
			if (ImGui::DragFloat("チャージ倍率", &data->meleeData->chargeDamageMultiplier, 0.01f, 0.0f, 100.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);

			if (ImGui::Checkbox("防御", &data->meleeData->bCanBlock)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
			if (ImGui::SliderFloat("防御軽減率", &data->meleeData->blockDamageReduction, 0.0f, 1.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);

			if (ImGui::DragFloat("ヒット停止", &data->meleeData->hitStopDuration, 0.001f, 0.0f, 1.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
			if (ImGui::DragFloat("ノックバック", &data->meleeData->knockbackForce, 0.01f, 0.0f, 10000.0f)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);
			ImGui::Unindent();
		}
	}

	/// ---------- 特殊効果・オートデータ ---------- ///
	if (ImGui::CollapsingHeader("特殊効果・オート設定", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::Checkbox("オート有効化", &data->bIsAutomatic)) MarkEditedAndMaybeApply(hooks, selectedID_, *data);

		ImGui::Separator();
		ImGui::TextUnformatted("特殊能力:");

		for (int i = 0; i < (int)IM_ARRAYSIZE(kAttrList); ++i)
		{
			const bool has = HasAttributes(data->attributes, kAttrList[i]);
			bool b = has;
			if (ImGui::Checkbox(kAttrNames[i], &b))
			{
				ToggleAttributes(data->attributes, kAttrList[i]);
				MarkEditedAndMaybeApply(hooks, selectedID_, *data);
			}
		}
	}
#endif // USE_IMGUI
}

void WeaponMasterDataEditor::ProcessPending(WeaponMasterDataDatabase& database, WeaponEditorHooks& hooks)
{
#ifdef USE_IMGUI
	if (pending_.doSaveAll)
	{
		pending_.doSaveAll = false;
		dirty_ = false;
		if (hooks.SaveAll) hooks.SaveAll();
	}

	if (pending_.deleteID.has_value())
	{
		const int32_t del = *pending_.deleteID;
		pending_.deleteID.reset();

		if (selectedID_ == del) selectedID_ = 0;

		// 将来ファイル削除が絡むなら hooks.RequestDelete を使う
		if (hooks.RequestDelete) hooks.RequestDelete(del);
		else database.RemoveByID(del);

		dirty_ = true;
	}
#endif // USE_IMGUI

}

void WeaponMasterDataEditor::MarkEditedAndMaybeApply(WeaponEditorHooks& hooks, int32_t weaponID, const FWeaponMasterData& data)
{
	dirty_ = true;
#ifdef USE_IMGUI
	if (autoApply_ && hooks.ApplyToRuntimeIfCurrent)
	{
		hooks.ApplyToRuntimeIfCurrent(weaponID, data);
	}
#endif // USE_IMGUI

}

bool WeaponMasterDataEditor::HasAttributes(const std::vector<EWeaponAttribute>& v, EWeaponAttribute attr)
{
	for (auto& x : v) if (x == attr) return true;
	return false;
}

bool WeaponMasterDataEditor::ToggleAttributes(std::vector<EWeaponAttribute>& v, EWeaponAttribute attr)
{
	for (size_t i = 0; i < v.size(); ++i)
	{
		if (v[i] == attr)
		{
			v.erase(v.begin() + i);
			return true;
		}
	}
	v.push_back(attr);
	return true;
}
