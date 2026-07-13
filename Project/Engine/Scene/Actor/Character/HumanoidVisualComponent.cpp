#include "HumanoidVisualComponent.h"

#include "ComponentProperty.h"
#include "MaterialRepository.h"

#include <algorithm>
#include <exception>
#include <unordered_map>
#include <utility>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	void HumanoidVisualComponent::Initialize()
	{
		SceneComponent::Initialize();

		HumanoidDefinition loadedDefinition;
		std::string loadError;
		if (!definitionPath_.empty() && loadedDefinition.LoadFromFile(definitionPath_, &loadError))
		{
			definition_ = std::move(loadedDefinition); // 外部定義が存在する場合はActor JSON内の控えより新しいAssetを優先する。
		}
		else if (definition_.GetParts().empty())
		{
			definition_ = HumanoidDefinition::CreateDefault();
		}

		if (!BuildBodyHierarchy(&loadError))
		{
			statusMessage_ = loadError;
		}
	}

	void HumanoidVisualComponent::Update(float deltaTime)
	{
		SceneComponent::Update(deltaTime);
		RefreshSharedMaterialBinding();
		UpdateHierarchy();
	}

	void HumanoidVisualComponent::UpdateEditor(float deltaTime)
	{
		SceneComponent::UpdateEditor(deltaTime);
		RefreshSharedMaterialBinding();
		UpdateHierarchy(); // PIE停止中もGizmoと部位階層の表示位置を一致させる。
	}

	void HumanoidVisualComponent::PostPhysicsUpdate([[maybe_unused]] float deltaTime)
	{
		RefreshWorldTransform();
		RefreshSharedMaterialBinding();
		UpdateHierarchy();
	}

	void HumanoidVisualComponent::Draw()
	{
		for (const BodyPart& part : parts_)
		{
			if (part.visible && part.object) part.object->Draw();
		}
	}

	void HumanoidVisualComponent::DrawShadow()
	{
		if (!IsCastShadowEnabled()) return;
		for (const BodyPart& part : parts_)
		{
			if (part.visible && part.object) part.object->DrawShadow();
		}
	}

	void HumanoidVisualComponent::DrawEditorObjectId(uint32_t objectId)
	{
		if (objectId == 0) return;
		for (const BodyPart& part : parts_)
		{
			if (part.visible && part.object) part.object->DrawEditorObjectId(objectId);
		}
	}

	void HumanoidVisualComponent::DrawImGui()
	{
		SceneComponent::DrawImGui();
#ifdef USE_IMGUI
		ImGui::SeparatorText("人型表示");
		ComponentPropertyUtility::DrawImGui(CreateProperties());
		if (ImGui::Button("人型定義を再読み込み"))
		{
			std::string error;
			if (!LoadDefinitionFromFile(definitionPath_, &error)) statusMessage_ = error;
		}
		ImGui::SameLine();
		if (ImGui::Button("人型定義を保存"))
		{
			std::string error;
			if (!SaveDefinitionToFile(definitionPath_, &error)) statusMessage_ = error;
			else statusMessage_ = "人型定義を保存しました: " + definitionPath_;
		}

		DrawMaterialBindingImGui();
		ImGui::SeparatorText("部位表示");
		for (BodyPart& part : parts_)
		{
			bool visible = part.visible;
			if (ImGui::Checkbox(part.id.c_str(), &visible)) SetPartVisible(part.id, visible);
		}
		ImGui::TextWrapped("状態: %s", statusMessage_.c_str());
#endif
	}

	void HumanoidVisualComponent::Finalize()
	{
		parts_.clear(); // Object3Dの所有権をComponentからまとめて解放する。
	}

	void HumanoidVisualComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson);
		ComponentPropertyUtility::ToJson(const_cast<HumanoidVisualComponent*>(this)->CreateProperties(), outJson);
		outJson["Definition"] = definition_.ToJson(); // 外部Assetが見つからない環境でも復元できる控えをActor JSONへ残す。
		if (materialBinding_.HasBinding()) outJson["Material"] = materialBinding_.ToJson();
	}

	void HumanoidVisualComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson);
		ComponentPropertyUtility::FromJson(CreateProperties(), inJson);

		HumanoidDefinition inlineDefinition;
		std::string definitionError;
		const auto definitionIt = inJson.find("Definition");
		const bool hasInlineDefinition = definitionIt != inJson.end() && inlineDefinition.FromJson(*definitionIt, &definitionError);

		HumanoidDefinition externalDefinition;
		const bool hasExternalDefinition = !definitionPath_.empty() && externalDefinition.LoadFromFile(definitionPath_, &definitionError);
		if (hasExternalDefinition) definition_ = std::move(externalDefinition);
		else if (hasInlineDefinition) definition_ = std::move(inlineDefinition);
		else if (definition_.GetParts().empty()) definition_ = HumanoidDefinition::CreateDefault();

		const auto materialIt = inJson.find("Material");
		if (materialIt != inJson.end() && materialIt->is_object()) materialBinding_.FromJson(*materialIt);
		else materialBinding_ = MaterialBinding{};

		if (IsInitialized())
		{
			if (!BuildBodyHierarchy(&definitionError)) statusMessage_ = definitionError;
		}
	}

	bool HumanoidVisualComponent::SetDefinition(const HumanoidDefinition& definition, std::string* outError)
	{
		if (!definition.Validate(outError)) return false;
		definition_ = definition;
		if (!IsInitialized())
		{
			if (outError) outError->clear();
			return true;
		}
		return BuildBodyHierarchy(outError);
	}

	bool HumanoidVisualComponent::LoadDefinitionFromFile(std::string_view definitionPath, std::string* outError)
	{
		if (definitionPath.empty())
		{
			if (outError) *outError = "人型定義ファイルパスが空です。";
			return false;
		}

		HumanoidDefinition loadedDefinition;
		if (!loadedDefinition.LoadFromFile(definitionPath, outError)) return false;
		definitionPath_ = std::string(definitionPath);
		definition_ = std::move(loadedDefinition);
		if (IsInitialized() && !BuildBodyHierarchy(outError)) return false;
		statusMessage_ = "人型定義を読み込みました: " + definitionPath_;
		return true;
	}

	bool HumanoidVisualComponent::SaveDefinitionToFile(std::string_view definitionPath, std::string* outError) const
	{
		if (definitionPath.empty())
		{
			if (outError) *outError = "人型定義ファイルパスが空です。";
			return false;
		}
		return definition_.SaveToFile(definitionPath, outError);
	}

	HumanoidVisualComponent::BodyPart* HumanoidVisualComponent::FindPart(std::string_view partId)
	{
		const auto partIt = std::find_if(parts_.begin(), parts_.end(), [partId](const BodyPart& part) { return part.id == partId; });
		return partIt != parts_.end() ? &(*partIt) : nullptr;
	}

	const HumanoidVisualComponent::BodyPart* HumanoidVisualComponent::FindPart(std::string_view partId) const
	{
		const auto partIt = std::find_if(parts_.begin(), parts_.end(), [partId](const BodyPart& part) { return part.id == partId; });
		return partIt != parts_.end() ? &(*partIt) : nullptr;
	}

	bool HumanoidVisualComponent::SetPartVisible(std::string_view partId, bool visible)
	{
		BodyPart* part = FindPart(partId);
		HumanoidPartDefinition* partDefinition = definition_.FindPart(partId);
		if (!part || !partDefinition) return false;
		part->visible = visible;
		partDefinition->visible = visible; // 実行時の切り替えを次回Actor JSON保存にも反映する。
		return true;
	}

	void HumanoidVisualComponent::SetAllPartsVisible(bool visible)
	{
		for (BodyPart& part : parts_)
		{
			part.visible = visible;
			if (HumanoidPartDefinition* partDefinition = definition_.FindPart(part.id)) partDefinition->visible = visible;
		}
	}

	void HumanoidVisualComponent::SetSkinTexturePath(std::string_view texturePath)
	{
		skinTexturePath_ = std::string(texturePath);
		ApplyAppearanceToAllParts();
	}

	void HumanoidVisualComponent::ApplySkinToAllParts()
	{
		ApplyAppearanceToAllParts();
	}

	void HumanoidVisualComponent::ApplySkinToAllParts(std::string_view texturePath)
	{
		SetSkinTexturePath(texturePath);
	}

	void HumanoidVisualComponent::SetMaterialAssetId(std::string_view assetId)
	{
		materialBinding_.SetAssetId(assetId);
		ApplyAppearanceToAllParts();
	}

	void HumanoidVisualComponent::SetMaterialOverrideEnabled(bool enabled)
	{
		materialBinding_.SetUseOverride(enabled);
		ApplyAppearanceToAllParts();
	}

	void HumanoidVisualComponent::ApplyMaterialToAllParts()
	{
		ApplyAppearanceToAllParts();
	}

	bool HumanoidVisualComponent::BuildBodyHierarchy(std::string* outError)
	{
		if (!definition_.Validate(outError)) return false;
		parts_.clear();
		parts_.reserve(definition_.GetParts().size()); // 親Transformへのポインタが構築中に無効化されない容量を先に確保する。

		std::unordered_map<std::string, size_t> partIndices;
		std::vector<bool> built(definition_.GetParts().size(), false);
		while (parts_.size() < definition_.GetParts().size())
		{
			bool builtAnyPart = false;
			for (size_t definitionIndex = 0; definitionIndex < definition_.GetParts().size(); ++definitionIndex)
			{
				if (built[definitionIndex]) continue;
				const HumanoidPartDefinition& partDefinition = definition_.GetParts()[definitionIndex];
				const auto parentIt = partIndices.find(partDefinition.parentId);
				if (!partDefinition.parentId.empty() && parentIt == partIndices.end()) continue;

				BodyPart part{};
				part.id = partDefinition.id;
				part.parentId = partDefinition.parentId;
				part.transform.translate_ = partDefinition.localPosition;
				part.transform.rotate_ = partDefinition.localRotation;
				part.transform.scale_ = partDefinition.localScale;
				part.transform.parent_ = partDefinition.parentId.empty()
					? &visualRootTransform_
					: &parts_[parentIt->second].transform;
				part.visible = partDefinition.visible;
				try
				{
					part.object = std::make_unique<Object3D>();
					part.object->Initialize(partDefinition.modelPath);
				}
				catch (const std::exception&)
				{
					part.object.reset(); // 1部位のモデル生成失敗でEditor全体を停止させず、定義修正を可能にする。
				}

				partIndices.emplace(part.id, parts_.size());
				parts_.push_back(std::move(part));
				built[definitionIndex] = true;
				builtAnyPart = true;
			}

			if (!builtAnyPart)
			{
				if (outError) *outError = "人型部位の親子順序を解決できませんでした。";
				parts_.clear();
				return false;
			}
		}

		ApplyAppearanceToAllParts();
		UpdateHierarchy();
		statusMessage_ = "人型部位を構築しました: " + std::to_string(parts_.size()) + " parts";
		if (outError) outError->clear();
		return true;
	}

	void HumanoidVisualComponent::UpdateHierarchy()
	{
		visualRootTransform_.translate_ = GetWorldPosition();
		visualRootTransform_.rotate_ = GetWorldRotation();
		visualRootTransform_.scale_ = GetWorldScale();
		visualRootTransform_.parent_ = nullptr;
		visualRootTransform_.Update();

		for (BodyPart& part : parts_)
		{
			part.transform.Update(); // 構築時に親が先となる順序へ並べているため1回の走査で階層全体を更新できる。
			if (part.object) part.object->UpdateWithWorldMatrix(part.transform.worldMatrix_);
		}
	}

	void HumanoidVisualComponent::ApplyAppearanceToAllParts()
	{
		materialRepositoryRevision_ = MaterialRepository::GetInstance()->GetRevision();
		MaterialDesc resolvedMaterial{};
		const bool hasBinding = materialBinding_.HasBinding();
		const bool resolved = hasBinding && materialBinding_.Resolve(resolvedMaterial);

		for (BodyPart& part : parts_)
		{
			if (!part.object) continue;
			if (resolved) part.object->ApplyMaterialDesc(resolvedMaterial);
			else part.object->ResetMaterialBinding();
			if (!skinTexturePath_.empty()) part.object->SetTextureForAll(skinTexturePath_); // Skin指定は共有MaterialのBaseColor Textureより後に適用する。
		}

		if (!hasBinding) materialBindingStatus_ = "モデル既定Materialを使用中";
		else if (!resolved) materialBindingStatus_ = "MaterialAssetが見つからないためモデル既定へフォールバック";
		else materialBindingStatus_ = materialBinding_.IsUsingOverride()
			? "Component固有Material Overrideを使用中"
			: "共有MaterialAssetを使用中: " + materialBinding_.GetAssetId();
	}

	void HumanoidVisualComponent::RefreshSharedMaterialBinding()
	{
		if (materialBinding_.GetAssetId().empty() || materialBinding_.IsUsingOverride()) return;
		const uint64_t currentRevision = MaterialRepository::GetInstance()->GetRevision();
		if (currentRevision != materialRepositoryRevision_) ApplyAppearanceToAllParts();
	}

	void HumanoidVisualComponent::DrawMaterialBindingImGui()
	{
#ifdef USE_IMGUI
		if (Ken4lowEngine::DrawMaterialBindingImGui(materialBinding_, "HumanoidVisualComponent")) ApplyAppearanceToAllParts();
		ImGui::TextDisabled("状態: %s", materialBindingStatus_.c_str());
#endif
	}

	std::vector<ComponentProperty> HumanoidVisualComponent::CreateProperties()
	{
		return {
			{ "DefinitionPath", "人型定義パス", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return definitionPath_; }, [this](const ComponentPropertyValue& value) { if (const std::string* path = std::get_if<std::string>(&value)) definitionPath_ = *path; } },
			{ "SkinTexturePath", "スキンテクスチャ", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return skinTexturePath_; }, [this](const ComponentPropertyValue& value) { if (const std::string* path = std::get_if<std::string>(&value)) SetSkinTexturePath(*path); } }
		};
	}
} // namespace Ken4lowEngine
