#include "JsonEditorWindow.h"
#include "JsonDataManager.h"
#include "ExampleJsonAsset.h"
#include "DataAssetPresets.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <algorithm>

namespace Ken4lowEngine
{
	JsonEditorWindow* JsonEditorWindow::GetInstance()
	{
		static JsonEditorWindow instance;
		return &instance;
	}

	void JsonEditorWindow::Initialize()
	{
		JsonAssetEntry example;
		example.type = "ExampleType";
		example.id = "example_asset";
		example.displayName = "Example Asset";
		example.path = std::string(basePath_) + "/example_asset.json";
		ExampleJsonAsset exampleData;
		exampleData.ToJson(example.data);
		JsonDataManager::SafeLoad(example.path, example);
		registry_.Register(example);
	}

	void JsonEditorWindow::Update(float deltaTime)
	{
		TryAutoSave(deltaTime);
	}

	void JsonEditorWindow::TryAutoSave(float deltaTime)
	{
		if (!autoSaveEnabled_)
		{
			return;
		}
		autoSaveElapsedSec_ += deltaTime;
		if (autoSaveElapsedSec_ < autoSaveIntervalSec_)
		{
			return;
		}
		autoSaveElapsedSec_ = 0.0f;
		for (auto& asset : const_cast<std::vector<JsonAssetEntry>&>(registry_.GetAssets()))
		{
			if (!asset.dirty) { continue; }
			if (JsonDataManager::SafeSave(asset)) { asset.dirty = false; }
		}
	}

	void JsonEditorWindow::CreateNewAsset()
	{
		JsonAssetEntry entry;
		const char* kTypes[] = { "ExampleType", "LightPreset", "PostEffectPreset", "Object3DPreset", "SpritePreset", "ParticlePreset", "ModelPreset" };
		entry.type = kTypes[newTypeIndex_];
		entry.id = registry_.MakeUniqueId(newId_);
		entry.displayName = newDisplayName_;
		std::string folder = std::string(basePath_) + "/";
		if (entry.type == "LightPreset") { folder += "LightPresets/"; }
		else if (entry.type == "PostEffectPreset") { folder += "PostEffectPresets/"; }
		else if (entry.type == "Object3DPreset") { folder += "Object3DPresets/"; }
		else if (entry.type == "SpritePreset") { folder += "SpritePresets/"; }
		else if (entry.type == "ParticlePreset") { folder += "ParticlePresets/"; }
		entry.path = folder + entry.id + ".json";
				if (entry.type == "LightPreset") { LightPreset preset; preset.ToJson(entry.data); }
		else if (entry.type == "PostEffectPreset") { PostEffectPreset preset; preset.ToJson(entry.data); }
		else if (entry.type == "Object3DPreset") { Object3DPreset preset; preset.ToJson(entry.data); }
		else if (entry.type == "SpritePreset") { SpritePreset preset; preset.ToJson(entry.data); }
		else if (entry.type == "ParticlePreset") { ParticlePreset preset; preset.ToJson(entry.data); }
		else { entry.data = nlohmann::json::object(); }
		entry.dirty = true;
		registry_.Register(entry);
	}

	void JsonEditorWindow::Draw(bool* pOpen)
	{
#ifdef USE_IMGUI
		if (!pOpen || !(*pOpen)) return;
		if (!ImGui::Begin("Json Asset Manager", pOpen))
		{
			ImGui::End();
			return;
		}

		ImGui::Checkbox("AutoSave", &autoSaveEnabled_);
		ImGui::SliderFloat("AutoSave Interval(sec)", &autoSaveIntervalSec_, 0.5f, 30.0f, "%.1f");
		ImGui::InputText("Asset Type Filter", typeFilter_, IM_ARRAYSIZE(typeFilter_));

		if (ImGui::Button("New")) { CreateNewAsset(); }
		ImGui::SameLine();
		if (ImGui::Button("Save") && selectedIndex_ >= 0)
		{
			auto& asset = const_cast<std::vector<JsonAssetEntry>&>(registry_.GetAssets())[selectedIndex_];
			if (JsonDataManager::SafeSave(asset)) { asset.dirty = false; }
		}
		ImGui::SameLine();
		if (ImGui::Button("Reload") && selectedIndex_ >= 0)
		{
			auto& asset = const_cast<std::vector<JsonAssetEntry>&>(registry_.GetAssets())[selectedIndex_];
			JsonDataManager::SafeLoad(asset.path, asset);
		}
		ImGui::SameLine();
		if (ImGui::Button("Duplicate") && selectedIndex_ >= 0)
		{
			auto& src = const_cast<std::vector<JsonAssetEntry>&>(registry_.GetAssets())[selectedIndex_];
			JsonAssetEntry dup;
			const std::string newId = registry_.MakeUniqueId(src.id + "_copy");
			const std::string dstPath = std::string(basePath_) + "/" + newId + ".json";
			if (JsonDataManager::Duplicate(src, dstPath, newId, dup)) { registry_.Register(dup); }
		}
		ImGui::SameLine();
		if (ImGui::Button("Delete") && selectedIndex_ >= 0)
		{
			auto& src = const_cast<std::vector<JsonAssetEntry>&>(registry_.GetAssets())[selectedIndex_];
			if (ImGui::IsKeyDown(ImGuiKey_LeftShift) && JsonDataManager::Delete(src.path))
			{
				registry_.RemoveById(src.id);
				selectedIndex_ = -1;
			}
		}

		const char* kTypes[] = { "ExampleType", "LightPreset", "PostEffectPreset", "Object3DPreset", "SpritePreset", "ParticlePreset", "ModelPreset" };
		ImGui::Combo("New Type", &newTypeIndex_, kTypes, IM_ARRAYSIZE(kTypes));
		ImGui::InputText("New Id", newId_, IM_ARRAYSIZE(newId_));
		ImGui::InputText("New DisplayName", newDisplayName_, IM_ARRAYSIZE(newDisplayName_));

		auto indices = registry_.CollectFilteredIndices(typeFilter_);
		ImGui::SeparatorText("Asset List");
		for (size_t idx : indices)
		{
			auto& a = registry_.GetAssets()[idx];
			if (ImGui::Selectable((a.id + "##" + std::to_string(idx)).c_str(), selectedIndex_ == static_cast<int>(idx)))
			{
				selectedIndex_ = static_cast<int>(idx);
			}
		}

		if (selectedIndex_ >= 0)
		{
			auto& asset = const_cast<std::vector<JsonAssetEntry>&>(registry_.GetAssets())[selectedIndex_];
			ImGui::SeparatorText("Selected Asset Detail");
			ImGui::Text("Type: %s", asset.type.c_str());
			ImGui::Text("Dirty: %s", asset.dirty ? "true" : "false");
			ImGui::TextWrapped("File Path: %s", asset.path.c_str());
			char displayNameBuffer[128]{};
			std::snprintf(displayNameBuffer, sizeof(displayNameBuffer), "%s", asset.displayName.c_str());
			if (ImGui::InputText("Display Name", displayNameBuffer, IM_ARRAYSIZE(displayNameBuffer)))
			{
				asset.displayName = displayNameBuffer;
				asset.dirty = true;
			}
		}

		ImGui::End();
#endif
	}
}
