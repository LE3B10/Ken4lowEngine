#include "WeaponEditorUI.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif // USE_IMGUI
#include <WeaponData.h>
#include <WeaponCatalog.h>
#include <WeaponClass.h>
#include <string>
#include <vector>

// 武器クラスラベル
static const char* kClassLabels[] = { "プライマリ","バックアップ","近接","特殊","スナイパー","ヘビー" };

/// -------------------------------------------------------------
/// 			　　追加・削除コントロールの描画
/// -------------------------------------------------------------
void WeaponEditorUI::DrawImGui(WeaponCatalog& catalog, const std::string& currentWeaponName, const WeaponEditorHooks& hooks)
{
#ifdef USE_IMGUI
	ImGui::Begin("武器編集");
	auto& table = catalog.All();   // 参照を一度だけ取る

	// 管理パネル（Editors 見出しと開閉トグル）
	ImGui::Separator();
	ImGui::Text("Editors");
	if (ImGui::Button("全て開く"))  for (auto& kv : table) weaponEditorOpen_[kv.first] = true;
	ImGui::SameLine();
	if (ImGui::Button("全て閉じる")) for (auto& kv : table) weaponEditorOpen_[kv.first] = false;

	// 一覧（チェックでトグル）
	for (auto& [name, _] : table) {
		bool o = weaponEditorOpen_[name];
		if (ImGui::Checkbox(name.c_str(), &o)) weaponEditorOpen_[name] = o;
	}

	// 安全のため “開いている名前だけ” スナップショット → これを回す
	std::vector<std::string> names;
	for (auto& kv : table) if (weaponEditorOpen_[kv.first]) names.push_back(kv.first);

	// 開いている武器ごとに独立ウィンドウを描画
	for (auto& name : names) {
		auto it = table.find(name);
		if (it == table.end()) continue; // そのフレームで消えた場合に備える

		bool open = weaponEditorOpen_[name];
		std::string title = "武器編集: " + name + "###editor_" + name;
		if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::PushID(name.c_str());
			DrawOne(it->second, currentWeaponName, hooks);
			ImGui::PopID();
		}
		ImGui::End();
		weaponEditorOpen_[name] = open;
	}

	// 下部の便利ボタン＆Add/Delete
	if (ImGui::Button("フォルダ保存")) { if (hooks.SaveAll) hooks.SaveAll(); }
	ImGui::SameLine();
	if (ImGui::Button("フォルダ読込")) {
		// フォーカス対象は“現在装備”や最初の1本でもOK。ここでは装備中を優先
		if (hooks.RequestReloadFocus) hooks.RequestReloadFocus(currentWeaponName);
	}
	ImGui::SameLine();
	if (ImGui::Button("ランタイム適用")) {
		if (!currentWeaponName.empty()) {
			auto it = table.find(currentWeaponName);
			if (it != table.end() && hooks.ApplyToRuntimeIfCurrent) hooks.ApplyToRuntimeIfCurrent(it->second);
		}
	}
	DrawAddDeleteControls(catalog, currentWeaponName, hooks);

	ImGui::End();
#endif // USE_IMGUI
}

/// -------------------------------------------------------------
/// 　　　　　	 単一武器編集ウィンドウの描画
/// -------------------------------------------------------------
void WeaponEditorUI::DrawOne(WeaponData& E, const std::string& currentWeaponName, const WeaponEditorHooks& hooks)
{
#ifdef USE_IMGUI
	// ---- 既存の “1本分” 編集UIを移植（カテゴリ変更→ランタイム反映→Loadout再構築） ----
	int classes = static_cast<int>(E.weapon_class);
	if (ImGui::Combo("カテゴリー", &classes, kClassLabels, IM_ARRAYSIZE(kClassLabels))) {
		E.weapon_class = static_cast<WeaponClass>(classes);
		if (hooks.RebuildLoadout) hooks.RebuildLoadout();
		if (hooks.ApplyToRuntimeIfCurrent && E.name == currentWeaponName) hooks.ApplyToRuntimeIfCurrent(E);
	}

	if (ImGui::TreeNode("武器設定"))
	{
		ImGui::DragFloat("銃口初速", &E.muzzleSpeed, 1.0f, 10.0f, 2000.0f);
		ImGui::DragFloat("最大距離", &E.maxDistance, 1.0f, 10.0f, 5000.0f);
		ImGui::DragFloat("連射速度", &E.rpm, 1.0f, 1.0f, 2000.0f);
		ImGui::DragFloat("一発あたりのダメージ量", &E.damage, 0.1f, 0.1f, 1000.0f);
		ImGui::DragInt("弾倉の装弾数", &E.magCapacity, 1, 1, 200);
		ImGui::DragInt("初期予備弾数", &E.startingReserve, 1, 0, 1000);
		ImGui::DragFloat("リロード時間", &E.reloadTime, 0.01f, 0.1f, 10.0f);
		ImGui::DragInt("発射する弾の数", &E.bulletsPerShot, 1, 1, 20);

		ImGui::Checkbox("オートリロード", &E.autoReload);
		ImGui::DragInt("弾道セグメント最大数", &E.requestedMaxSegments, 1, 10, 1000);
		ImGui::DragFloat("発射時の拡がり角", &E.spreadDeg, 0.1f, 0.0f, 45.0f);
		int mode = E.pelletTracerMode;
		const char* modes[] = { "なし", "1発ずつ", "全弾" };
		if (ImGui::Combo("散弾時のトレーサ動作モード", &mode, modes, IM_ARRAYSIZE(modes))) {
			E.pelletTracerMode = mode;
		}
		ImGui::DragInt("散弾時にトレーサを出す発数", &E.pelletTracerCount, 1, 1, 64);

		ImGui::TreePop();
	}
	if (ImGui::TreeNode("弾道の設定"))
	{
		ImGui::Checkbox("弾道の有効", &E.tracer.enabled);
		ImGui::DragFloat("見た目の長さ", &E.tracer.tracerLength, 0.01f, 0.01f, 50.0f);
		ImGui::DragFloat("幅", &E.tracer.tracerWidth, 0.001f, 0.001f, 1.0f);
		ImGui::DragFloat("セグメント間引き閾値", &E.tracer.minSegLength, 0.001f, 0.001f, 1.0f);
		ImGui::DragFloat("弾/トレーサの開始点を銃口から前後にオフセット", &E.tracer.startOffsetForward, 0.01f, -10.0f, 10.0f);
		ImGui::ColorEdit4("弾道の色", &E.tracer.color.x);
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("マズルフラッシュ設定"))
	{
		ImGui::Checkbox("マズルフラッシュ有効", &E.muzzle.enabled);
		ImGui::DragFloat("寿命", &E.muzzle.life, 0.005f, 0.01f, 0.5f);
		ImGui::DragFloat("初期の長さ", &E.muzzle.startLength, 0.01f, 0.01f, 1.0f);
		ImGui::DragFloat("終了時の長さ", &E.muzzle.endLength, 0.01f, 0.01f, 1.0f);
		ImGui::DragFloat("初期の太さ", &E.muzzle.startWidth, 0.005f, 0.01f, 0.5f);
		ImGui::DragFloat("終了時の太さ", &E.muzzle.endWidth, 0.005f, 0.01f, 0.5f);
		ImGui::DragFloat("発射ごとのランダム広がり", &E.muzzle.randomYawDeg, 0.1f, 0.0f, 45.0f);
		ImGui::ColorEdit4("マズルの色", &E.muzzle.color.x);
		ImGui::DragFloat("フラッシュ根元を前後にオフセット", &E.muzzle.offsetForward, 0.01f, -1.0f, 1.0f);
		ImGui::Checkbox("火花を有効", &E.muzzle.sparksEnabled);
		ImGui::DragInt("1発で何本", &E.muzzle.sparkCount, 1, 0, 200);
		ImGui::DragFloat("火花の最小寿命", &E.muzzle.sparkLifeMin, 0.005f, 0.01f, 1.0f);
		ImGui::DragFloat("火花の最大寿命", &E.muzzle.sparkLifeMax, 0.005f, 0.01f, 1.0f);
		ImGui::DragFloat("火花の最小速度", &E.muzzle.sparkSpeedMin, 0.1f, 0.1f, 100.0f);
		ImGui::DragFloat("火花の最大速度", &E.muzzle.sparkSpeedMax, 0.1f, 0.1f, 100.0f);
		ImGui::DragFloat("前方への拡がり角", &E.muzzle.sparkConeDeg, 0.1f, 0.0f, 90.0f);
		ImGui::DragFloat("火花用重力", &E.muzzle.sparkGravityY, 0.1f, -100.0f, 0.0f);
		ImGui::DragFloat("火花の太さ", &E.muzzle.sparkWidth, 0.001f, 0.001f, 0.1f);
		ImGui::DragFloat("火花の開始位置", &E.muzzle.sparkOffsetForward, 0.01f, -1.0f, 1.0f);
		ImGui::ColorEdit4("火花の始まりの色", &E.muzzle.sparkColorStart.x);
		ImGui::ColorEdit4("火花の終わりの色", &E.muzzle.sparkColorEnd.x);
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("薬莢の設定"))
	{
		ImGui::Checkbox("薬莢を有効", &E.casing.enabled);
		ImGui::DragFloat3("銃口基準のローカルオフセット（右 / 上 / 後）", &E.casing.offsetRight, 0.005f);
		ImGui::DragFloat("薬莢の初速最小", &E.casing.speedMin, 0.1f, 0.0f, 20.0f);
		ImGui::DragFloat("薬莢の初速最大", &E.casing.speedMax, 0.1f, 0.0f, 20.0f);
		ImGui::DragFloat("右方向を中心にした円錐拡がり", &E.casing.coneDeg, 0.1f, 0.0f, 90.0f);
		ImGui::DragFloat("自然落下", &E.casing.gravityY, 0.1f, -100.0f, 0.0f);
		ImGui::DragFloat("寿命", &E.casing.life, 0.05f, 0.1f, 10.0f);
		ImGui::DragFloat("空気抵抗", &E.casing.drag, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("真上方向への瞬間的なキック", &E.casing.upKick, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("方向ベクトルを上向きに寄せるブレンド", &E.casing.upBias, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("回転速度最小", &E.casing.spinMin, 0.1f, 0.0f, 100.0f);
		ImGui::DragFloat("回転速度最大", &E.casing.spinMax, 0.1f, 0.0f, 100.0f);
		ImGui::ColorEdit4("真鍮の色", &E.casing.color.x);
		ImGui::DragFloat3("大きさ", &E.casing.scale.x, 0.001f);
		ImGui::TreePop();
	}

	// Apply ボタン（“このウィンドウの武器”で即反映したいとき）
	if (ImGui::Button("ランタイムを適用")) {
		if (hooks.ApplyToRuntimeIfCurrent && E.name == currentWeaponName) hooks.ApplyToRuntimeIfCurrent(E);
	}
#endif // USE_IMGUI
}

/// -------------------------------------------------------------
/// 　　　　　	  追加・削除コントロールの描画
/// -------------------------------------------------------------
void WeaponEditorUI::DrawAddDeleteControls(WeaponCatalog& catalog, const std::string& currentWeaponName, const WeaponEditorHooks& hooks)
{
#ifdef USE_IMGUI
	// Add
	if (ImGui::Button("武器を追加")) {
		ImGui::OpenPopup("武器ポップアップを追加");
	}
	if (ImGui::BeginPopupModal("武器ポップアップを追加", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		static char nameBuf[64] = "新しい武器";
		static int sourceMode = 0; // 0=Duplicate Current, 1=Empty(Default), 2=Duplicate Selected
		static int selectedIndex = 0;

		auto& table = catalog.All();
		std::vector<std::string> names; names.reserve(table.size());
		for (auto& kv : table) names.push_back(kv.first);
		if (names.empty()) names.push_back("ピストル");

		ImGui::InputText("名前", nameBuf, IM_ARRAYSIZE(nameBuf));
		ImGui::RadioButton("複製", &sourceMode, 0); ImGui::SameLine();
		ImGui::RadioButton("空", &sourceMode, 1); ImGui::SameLine();
		ImGui::RadioButton("選択して複製", &sourceMode, 2);
		if (sourceMode == 2) {
			if (ImGui::BeginCombo("From", names[selectedIndex].c_str())) {
				for (int i = 0; i < (int)names.size(); ++i) {
					bool sel = (i == selectedIndex);
					if (ImGui::Selectable(names[i].c_str(), sel)) selectedIndex = i;
					if (sel) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}

		if (ImGui::Button("作成")) {
			std::string baseName;
			if (sourceMode == 0) baseName = currentWeaponName;        // 現在装備を複製
			else if (sourceMode == 2) baseName = names[selectedIndex]; // 選択複製
			if (hooks.RequestAdd) hooks.RequestAdd(nameBuf, baseName); // 依頼だけ（実処理はフレーム末）
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("キャンセル")) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	ImGui::SameLine();

	// Delete
	if (ImGui::Button("武器を削除")) ImGui::OpenPopup("武器削除の確認");
	if (ImGui::BeginPopupModal("武器削除の確認", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Delete '%s' ?\nThis cannot be undone.", currentWeaponName.c_str());
		ImGui::Separator();
		if (ImGui::Button("はい, 削除", ImVec2(120, 0))) {
			if (hooks.RequestDelete) hooks.RequestDelete(currentWeaponName); // 依頼だけ
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("キャンセル", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
#endif // USE_IMGUI
}
