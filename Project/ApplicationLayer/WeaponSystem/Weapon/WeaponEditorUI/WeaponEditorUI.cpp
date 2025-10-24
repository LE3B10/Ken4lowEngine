#include "WeaponEditorUI.h"
#include "imgui.h"

// 武器クラスラベル
static const char* kClassLabels[] = { "Primary","Backup","Melee","Special","Sniper","Heavy" };

/// -------------------------------------------------------------
/// 			　　追加・削除コントロールの描画
/// -------------------------------------------------------------
void WeaponEditorUI::DrawImGui(WeaponCatalog& catalog, const std::string& currentWeaponName, const WeaponEditorHooks& hooks)
{
	auto& table = catalog.All();   // 参照を一度だけ取る

	// 管理パネル（Editors 見出しと開閉トグル）
	ImGui::Separator();
	ImGui::Text("Editors");
	if (ImGui::Button("Open all"))  for (auto& kv : table) weaponEditorOpen_[kv.first] = true;
	ImGui::SameLine();
	if (ImGui::Button("Close all")) for (auto& kv : table) weaponEditorOpen_[kv.first] = false;

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
		std::string title = "Weapon Editor: " + name + "###editor_" + name;
		if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::PushID(name.c_str());
			DrawOne(it->second, currentWeaponName, hooks);
			ImGui::PopID();
		}
		ImGui::End();
		weaponEditorOpen_[name] = open;
	}

	// 下部の便利ボタン＆Add/Delete
	if (ImGui::Button("Save folder")) { if (hooks.SaveAll) hooks.SaveAll(); }
	ImGui::SameLine();
	if (ImGui::Button("Reload folder")) {
		// フォーカス対象は“現在装備”や最初の1本でもOK。ここでは装備中を優先
		if (hooks.RequestReloadFocus) hooks.RequestReloadFocus(currentWeaponName);
	}
	ImGui::SameLine();
	if (ImGui::Button("Apply to runtime")) {
		if (!currentWeaponName.empty()) {
			auto it = table.find(currentWeaponName);
			if (it != table.end() && hooks.ApplyToRuntimeIfCurrent) hooks.ApplyToRuntimeIfCurrent(it->second);
		}
	}

	DrawAddDeleteControls(catalog, currentWeaponName, hooks);
}

/// -------------------------------------------------------------
/// 　　　　　	 単一武器編集ウィンドウの描画
/// -------------------------------------------------------------
void WeaponEditorUI::DrawOne(WeaponData& E, const std::string& currentWeaponName, const WeaponEditorHooks& hooks)
{
	// ---- 既存の “1本分” 編集UIを移植（カテゴリ変更→ランタイム反映→Loadout再構築） ----
	int clazz = static_cast<int>(E.clazz);
	if (ImGui::Combo("Category", &clazz, kClassLabels, IM_ARRAYSIZE(kClassLabels))) {
		E.clazz = static_cast<WeaponClass>(clazz);
		if (hooks.RebuildLoadout) hooks.RebuildLoadout();
		if (hooks.ApplyToRuntimeIfCurrent && E.name == currentWeaponName) hooks.ApplyToRuntimeIfCurrent(E);
	}

	if (ImGui::TreeNode("Basic"))
	{
		ImGui::DragFloat("muzzleSpeed", &E.muzzleSpeed, 1.0f, 10.0f, 2000.0f);
		ImGui::DragFloat("maxDistance", &E.maxDistance, 1.0f, 10.0f, 5000.0f);
		ImGui::DragFloat("rpm", &E.rpm, 1.0f, 1.0f, 2000.0f);
		ImGui::DragInt("magCapacity", &E.magCapacity, 1, 1, 200);
		ImGui::DragInt("startingReserve", &E.startingReserve, 1, 0, 1000);
		ImGui::DragFloat("reloadTime", &E.reloadTime, 0.01f, 0.1f, 10.0f);
		ImGui::DragInt("bulletsPerShot", &E.bulletsPerShot, 1, 1, 20);

		ImGui::Checkbox("autoReload", &E.autoReload);
		ImGui::DragInt("requestedMaxSegments", &E.requestedMaxSegments, 1, 10, 1000);
		ImGui::DragFloat("spreadDeg", &E.spreadDeg, 0.1f, 0.0f, 45.0f);
		int mode = E.pelletTracerMode;
		const char* modes[] = { "None", "One of pellets", "All pellets" };
		if (ImGui::Combo("pelletTracerMode", &mode, modes, IM_ARRAYSIZE(modes))) {
			E.pelletTracerMode = mode;
		}
		ImGui::DragInt("pelletTracerCount", &E.pelletTracerCount, 1, 1, 64);

		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Tracer"))
	{
		ImGui::Checkbox("enabled", &E.tracer.enabled);
		ImGui::DragFloat("tracerLength", &E.tracer.tracerLength, 0.01f, 0.01f, 50.0f);
		ImGui::DragFloat("tracerWidth", &E.tracer.tracerWidth, 0.001f, 0.001f, 1.0f);
		ImGui::DragFloat("minSegLength", &E.tracer.minSegLength, 0.001f, 0.001f, 1.0f);
		ImGui::DragFloat("startOffsetForward", &E.tracer.startOffsetForward, 0.01f, -10.0f, 10.0f);
		ImGui::ColorEdit4("color", &E.tracer.color.x);
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Muzzle"))
	{
		ImGui::Checkbox("enabled##mz", &E.muzzle.enabled);
		ImGui::DragFloat("life", &E.muzzle.life, 0.005f, 0.01f, 0.5f);
		ImGui::DragFloat("startLength", &E.muzzle.startLength, 0.01f, 0.01f, 1.0f);
		ImGui::DragFloat("endLength", &E.muzzle.endLength, 0.01f, 0.01f, 1.0f);
		ImGui::DragFloat("startWidth", &E.muzzle.startWidth, 0.005f, 0.01f, 0.5f);
		ImGui::DragFloat("endWidth", &E.muzzle.endWidth, 0.005f, 0.01f, 0.5f);
		ImGui::DragFloat("randomYawDeg", &E.muzzle.randomYawDeg, 0.1f, 0.0f, 45.0f);
		ImGui::ColorEdit4("color##mz", &E.muzzle.color.x);
		ImGui::DragFloat("offsetForward", &E.muzzle.offsetForward, 0.01f, -1.0f, 1.0f);
		ImGui::Checkbox("sparksEnabled", &E.muzzle.sparksEnabled);
		ImGui::DragInt("sparkCount", &E.muzzle.sparkCount, 1, 0, 200);
		ImGui::DragFloat("sparkLifeMin", &E.muzzle.sparkLifeMin, 0.005f, 0.01f, 1.0f);
		ImGui::DragFloat("sparkLifeMax", &E.muzzle.sparkLifeMax, 0.005f, 0.01f, 1.0f);
		ImGui::DragFloat("sparkSpeedMin", &E.muzzle.sparkSpeedMin, 0.1f, 0.1f, 100.0f);
		ImGui::DragFloat("sparkSpeedMax", &E.muzzle.sparkSpeedMax, 0.1f, 0.1f, 100.0f);
		ImGui::DragFloat("sparkConeDeg", &E.muzzle.sparkConeDeg, 0.1f, 0.0f, 90.0f);
		ImGui::DragFloat("sparkGravityY", &E.muzzle.sparkGravityY, 0.1f, -100.0f, 0.0f);
		ImGui::DragFloat("sparkWidth", &E.muzzle.sparkWidth, 0.001f, 0.001f, 0.1f);
		ImGui::DragFloat("sparkOffsetForward", &E.muzzle.sparkOffsetForward, 0.01f, -1.0f, 1.0f);
		ImGui::ColorEdit4("sparkColorStart", &E.muzzle.sparkColorStart.x);
		ImGui::ColorEdit4("sparkColorEnd", &E.muzzle.sparkColorEnd.x);
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Casing"))
	{
		ImGui::Checkbox("enabled##cs", &E.casing.enabled);
		ImGui::DragFloat3("offset R/U/B", &E.casing.offsetRight, 0.005f);
		ImGui::DragFloat("speedMin", &E.casing.speedMin, 0.1f, 0.0f, 20.0f);
		ImGui::DragFloat("speedMax", &E.casing.speedMax, 0.1f, 0.0f, 20.0f);
		ImGui::DragFloat("coneDeg", &E.casing.coneDeg, 0.1f, 0.0f, 90.0f);
		ImGui::DragFloat("gravityY", &E.casing.gravityY, 0.1f, -100.0f, 0.0f);
		ImGui::DragFloat("life", &E.casing.life, 0.05f, 0.1f, 10.0f);
		ImGui::DragFloat("drag", &E.casing.drag, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("upKick", &E.casing.upKick, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("upBias", &E.casing.upBias, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("spinMin", &E.casing.spinMin, 0.1f, 0.0f, 100.0f);
		ImGui::DragFloat("spinMax", &E.casing.spinMax, 0.1f, 0.0f, 100.0f);
		ImGui::ColorEdit4("color##cs", &E.casing.color.x);
		ImGui::DragFloat3("scale", &E.casing.scale.x, 0.001f);
		ImGui::TreePop();
	}

	// Apply ボタン（“このウィンドウの武器”で即反映したいとき）
	if (ImGui::Button("Apply to runtime")) {
		if (hooks.ApplyToRuntimeIfCurrent && E.name == currentWeaponName) hooks.ApplyToRuntimeIfCurrent(E);
	}
}

/// -------------------------------------------------------------
/// 　　　　　	  追加・削除コントロールの描画
/// -------------------------------------------------------------
void WeaponEditorUI::DrawAddDeleteControls(WeaponCatalog& catalog, const std::string& currentWeaponName, const WeaponEditorHooks& hooks)
{
	// Add
	if (ImGui::Button("Add Weapon")) {
		ImGui::OpenPopup("AddWeaponPopup");
	}
	if (ImGui::BeginPopupModal("AddWeaponPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		static char nameBuf[64] = "NewWeapon";
		static int sourceMode = 0; // 0=Duplicate Current, 1=Empty(Default), 2=Duplicate Selected
		static int selectedIndex = 0;

		auto& table = catalog.All();
		std::vector<std::string> names; names.reserve(table.size());
		for (auto& kv : table) names.push_back(kv.first);
		if (names.empty()) names.push_back("Pistol");

		ImGui::InputText("Name", nameBuf, IM_ARRAYSIZE(nameBuf));
		ImGui::RadioButton("Duplicate Current", &sourceMode, 0); ImGui::SameLine();
		ImGui::RadioButton("Empty(Default)", &sourceMode, 1); ImGui::SameLine();
		ImGui::RadioButton("Duplicate Selected", &sourceMode, 2);
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

		if (ImGui::Button("Create")) {
			std::string baseName;
			if (sourceMode == 0) baseName = currentWeaponName;        // 現在装備を複製
			else if (sourceMode == 2) baseName = names[selectedIndex]; // 選択複製
			if (hooks.RequestAdd) hooks.RequestAdd(nameBuf, baseName); // 依頼だけ（実処理はフレーム末）
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	ImGui::SameLine();

	// Delete
	if (ImGui::Button("Delete Weapon")) ImGui::OpenPopup("DeleteWeaponConfirm");
	if (ImGui::BeginPopupModal("DeleteWeaponConfirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Delete '%s' ?\nThis cannot be undone.", currentWeaponName.c_str());
		ImGui::Separator();
		if (ImGui::Button("Yes, delete", ImVec2(120, 0))) {
			if (hooks.RequestDelete) hooks.RequestDelete(currentWeaponName); // 依頼だけ
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}
