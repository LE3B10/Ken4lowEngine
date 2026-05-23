#include "JsonEditorWindow.h"
#include "JsonDataManager.h"
#include "ExampleJsonAsset.h"
#include "DataAssetPresets.h"
#include "Sprite.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <algorithm>
#include <filesystem>

namespace
{
	std::string GetDataAssetFolder(const std::string& basePath, const std::string& type)
	{
		std::string folder = basePath + "/";
		if (type == "LightPreset") { folder += "LightPresets/"; }
		else if (type == "PostEffectPreset") { folder += "PostEffectPresets/"; }
		else if (type == "Object3DPreset") { folder += "Object3DPresets/"; }
		else if (type == "SpritePreset") { folder += "SpritePresets/"; }
		else if (type == "ParticlePreset") { folder += "ParticlePresets/"; }
		return folder;
	}
}

namespace Ken4lowEngine
{
	JsonEditorWindow* JsonEditorWindow::GetInstance()
	{
		static JsonEditorWindow instance;
		return &instance;
	}

	void JsonEditorWindow::Initialize()
	{
		LoadAssetsFromDirectory(basePath_);
	}

	void JsonEditorWindow::LoadAssetsFromDirectory(const std::string& rootDirectory)
	{
		namespace fs = std::filesystem;
		if (!fs::exists(rootDirectory)) { return; }
		for (const auto& entry : fs::recursive_directory_iterator(rootDirectory))
		{
			if (!entry.is_regular_file() || entry.path().extension() != ".json") { continue; }
			JsonAssetEntry loaded{};
			if (!JsonDataManager::SafeLoad(entry.path().string(), loaded)) { continue; }
			if (loaded.id.empty()) { loaded.id = entry.path().stem().string(); }
			if (loaded.displayName.empty()) { loaded.displayName = loaded.id; }
			registry_.Register(loaded);
		}
	}

	void JsonEditorWindow::ApplySelectedSpritePresetToPreview()
	{
		if (selectedIndex_ < 0) { return; }
		auto& asset = const_cast<std::vector<JsonAssetEntry>&>(registry_.GetAssets())[selectedIndex_];
		if (asset.type != "SpritePreset") { return; }
		SpritePreset preset{};
		preset.FromJson(asset.data);
		if (preset.texturePath.empty()) { return; }
		if (!spritePreview_)
		{
			spritePreview_ = std::make_unique<Sprite>();
			spritePreview_->Initialize(preset.texturePath);
		}
		// SpritePresetをPreview用Spriteに適用して、JSON設定が実際の描画へ反映されることを確認できるようにする
		ApplySpritePreset(*spritePreview_, preset);
		lastPreviewPresetId_ = asset.id;
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
		std::string folder = GetDataAssetFolder(basePath_, entry.type);
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
			const std::string dstPath = GetDataAssetFolder(basePath_, src.type) + newId + ".json";
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

			if (asset.type == "SpritePreset")
			{
				SpritePreset preset{};
				preset.FromJson(asset.data);

				char texturePathBuffer[256]{};
				std::snprintf(texturePathBuffer, sizeof(texturePathBuffer), "%s", preset.texturePath.c_str());
				if (ImGui::InputText("Texture Path", texturePathBuffer, IM_ARRAYSIZE(texturePathBuffer))) { preset.texturePath = texturePathBuffer; asset.dirty = true; }
				if (ImGui::InputFloat2("Position", &preset.position.x)) { asset.dirty = true; }
				if (ImGui::InputFloat2("Size", &preset.size.x)) { asset.dirty = true; }
				if (ImGui::InputFloat("Rotation", &preset.rotation)) { asset.dirty = true; }
				if (ImGui::InputFloat2("Anchor", &preset.anchor.x)) { asset.dirty = true; }
				if (ImGui::ColorEdit4("Color", &preset.color.x)) { asset.dirty = true; }
				if (ImGui::Checkbox("Visible", &preset.visible)) { asset.dirty = true; }
				if (ImGui::InputInt("DrawOrder", &preset.drawOrder)) { asset.dirty = true; }
				if (ImGui::InputFloat2("Texture LeftTop(px)", &preset.textureLeftTop.x)) { asset.dirty = true; }
				if (ImGui::InputFloat2("Texture Size(px, 0=full)", &preset.textureSize.x)) { asset.dirty = true; }
				ImGui::TextDisabled("Not applied yet: layer / pivot / enableAlpha / drawOrder");
				if (ImGui::Button("Apply Selected SpritePreset to Test Sprite"))
				{
					ApplySelectedSpritePresetToPreview();
				}
				if (!lastPreviewPresetId_.empty())
				{
					ImGui::Text("Preview Applied: %s", lastPreviewPresetId_.c_str());
				}

				if (asset.dirty)
				{
					preset.ToJson(asset.data);
				}
			}
		}

		if (spritePreview_)
		{
			spritePreview_->Update();
			spritePreview_->Draw();
		}

		ImGui::End();
#endif
	}
}
