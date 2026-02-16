#ifdef USE_IMGUI
#include "WeaponDevPanel.h"
#include "WeaponMasterDataWriter.h"
#include <imgui.h>

namespace {
	const char* CategoryFolder(EWeaponCategory c) {
		switch (c) {
		case EWeaponCategory::Primary: return "primary";
		case EWeaponCategory::Backup:  return "backup";
		case EWeaponCategory::Melee:   return "melee";
		case EWeaponCategory::Special: return "special";
		case EWeaponCategory::Sniper:  return "sniper";
		case EWeaponCategory::Heavy:   return "heavy";
		default: return "unknown";
		}
	}
}

void WeaponDevPanel::Initialize(const std::filesystem::path& root,
	std::function<void(int32_t)> onApply)
{
	root_ = root;
	onApply_ = std::move(onApply);

	// hooks を1回だけセット
	hooks_.SaveAll = [&]()
		{
			std::string err;
			WeaponMasterDataWriter::SaveAllByCategory(db_, root_, &err);
		};

	hooks_.RequestReloadFocus = [](int32_t) {};
	hooks_.RebuildLoadout = []() {};

	hooks_.ApplyToRuntimeIfCurrent =
		[&](int32_t weaponID, const FWeaponMasterData&)
		{
			lastAppliedID_ = weaponID;
			if (onApply_) onApply_(weaponID); // ★ここでプレイヤーに装備させる
		};

	hooks_.RequestDelete =
		[&](int32_t weaponID)
		{
			std::string err;
			WeaponMasterDataWriter::DeleteFilesByWeaponID(root_, weaponID, &err);
			db_.RemoveByID(weaponID);
		};

	hooks_.RequestAdd = [](const std::string&, int32_t) {};

	initialized_ = true;
}

void WeaponDevPanel::EnsureLoadedOnce()
{
	// あなたのDBに LoadFromDirectory がある前提。名前が違うならここだけ合わせればOK。
	if (db_.Size() > 0) return;

	std::string err;
	db_.LoadFromDirectory(root_, &err); // ←DB関数名はプロジェクトに合わせて変更
}

void WeaponDevPanel::DrawImGui()
{
	EnsureLoadedOnce();

	ImGui::Begin("Weapon Master Editor");
	ImGui::Text("Root: %s", root_.string().c_str());
	ImGui::Text("Count: %zu", db_.Size());
	ImGui::Text("Last Applied ID: %d", lastAppliedID_);
	ImGui::Separator();

	editor_.DrawImGui(db_, hooks_);
	ImGui::End();
}

void WeaponDevPanel::DrawEquipOnlyImGui()
{
	EnsureLoadedOnce();

	ImGui::Begin("Weapon Equip");
	ImGui::Text("Root: %s", root_.string().c_str());
	ImGui::Text("Count: %zu", db_.Size());
	ImGui::Separator();

	// 一番簡単：IDを手入力してEquip
	static int equipID = 0;
	ImGui::InputInt("WeaponID", &equipID);
	if (ImGui::Button("Equip") && onApply_)
		onApply_(equipID);

	ImGui::End();
}
#endif