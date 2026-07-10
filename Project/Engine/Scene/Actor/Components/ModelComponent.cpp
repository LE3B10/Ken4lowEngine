#include "ModelComponent.h"
#include "SceneComponent.h"
#include "Actor.h"
#include "AssetPathSelector.h"
#include "MaterialRepository.h"

#include <algorithm>
#include <exception>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	void ModelComponent::Initialize()
	{
		// SceneComponent側のWorldTransform計算を先に行う
		SceneComponent::Initialize();

		if (modelPath_.empty())
		{
			return; // モデルパス未設定の場合は、描画対象を生成しない。
		}

		try
		{
			object3D_ = std::make_unique<Object3D>();
			object3D_->Initialize(modelPath_);
		} catch (const std::exception&)
		{
			object3D_.reset();
			return;
		} catch (...)
		{
			object3D_.reset();
			return;
		}

		SyncTransformToObject3D();
		ApplyMaterialBinding(); // Material未指定なら既存Object3Dの初期Materialをそのまま維持する。
	}

	void ModelComponent::Update(float deltaTime)
	{
		// 親子関係を考慮したWorldTransformを更新する。
		SceneComponent::Update(deltaTime);

		if (!object3D_)
		{
			return; // モデル未生成の場合は更新しない。
		}

		RefreshSharedMaterialBinding(); // MaterialPreset編集による共有Asset差し替えを描画前に反映する。
		SyncTransformToObject3D();
		object3D_->Update(); // Object3D側の行列更新やGPU転送を行う。
	}

	void ModelComponent::PostPhysicsUpdate([[maybe_unused]] float deltaTime)
	{
		if (!object3D_)
		{
			return; // モデル未生成の場合は更新しない。
		}

		RefreshSharedMaterialBinding(); // Physics後更新だけが走る場合も共有Materialの変更を取りこぼさない。
		SyncTransformToObject3D();
		object3D_->Update(); // Object3D側の行列更新やGPU転送を行う。
	}

	void ModelComponent::Draw()
	{
		if (!visible_ || !object3D_)
		{
			return; // 描画対象がない場合は描画しない。
		}

		object3D_->Draw(); // 通常描画パスでモデルを描画する。
	}

	void ModelComponent::DrawShadow()
	{
		if (!visible_ || !object3D_)
		{
			return; // 影描画対象がない場合は描画しない。
		}

		object3D_->DrawShadow(); // ShadowMap用の描画パスでモデルを描画する。
	}

	void ModelComponent::DrawImGui()
	{
		// Local / World TransformをImGuiに表示する
		SceneComponent::DrawImGui();

#ifdef USE_IMGUI
		ImGui::SeparatorText("Model Component");
		ImGui::Text("現在のモデル: %s", modelPath_.empty() ? "未選択" : modelPath_.c_str());

		std::string selectedModelPath = modelPath_;
		if (AssetPathSelector::DrawAssetSelector("一覧から選択##ModelComponentModelPath", selectedModelPath, AssetType::Model))
		{
			SetModelPath(selectedModelPath);
		}

		ComponentPropertyUtility::DrawImGui(CreateProperties(false));
		DrawMaterialBindingImGui();

		ImGui::Text("Object3D: %s", object3D_ ? "Created" : "Not Created");
#endif // USE_IMGUI
	}

	void ModelComponent::Finalize()
	{
		object3D_.reset(); // Component破棄時にObject3Dも破棄する。
	}

	void ModelComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson); // SceneComponent共通情報をJSONへ保存する

		outJson["Class"] = GetClassTypeName(); // ModelComponentとして保存する
		ComponentPropertyUtility::ToJson(const_cast<ModelComponent*>(this)->CreateProperties(), outJson);
		if (materialBinding_.HasBinding())
		{
			outJson["Material"] = materialBinding_.ToJson(); // Material未指定の旧Actor JSONには新しい項目を増やさない。
		}
	}

	void ModelComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson); // SceneComponent共通情報をJSONから復元する

		ComponentPropertyUtility::FromJson(CreateProperties(), inJson);
		const auto materialIt = inJson.find("Material");
		if (materialIt != inJson.end() && materialIt->is_object())
		{
			materialBinding_.FromJson(*materialIt); // 新形式だけを任意読込し、旧JSONはモデル既定Materialへフォールバックする。
		}
		else
		{
			materialBinding_ = MaterialBinding{};
		}
		ApplyMaterialBinding(); // 初期化済みComponentへ直接FromJsonした場合もMaterial状態を同期する。
	}

	void ModelComponent::SetModelPath(std::string_view modelPath)
	{
		const std::string newModelPath(modelPath);
		if (modelPath_ == newModelPath)
		{
			return; // 同じモデルパスなら再生成しない
		}

		modelPath_ = newModelPath; // string_viewは保持せず、内部ではstd::stringとして所有する。
		const bool hadObject = object3D_ != nullptr;
		object3D_.reset();

		if (!hadObject || modelPath_.empty())
		{
			return;
		}

		try
		{
			object3D_ = std::make_unique<Object3D>();
			object3D_->Initialize(modelPath_);
		} catch (const std::exception&)
		{
			object3D_.reset();
			return;
		} catch (...)
		{
			object3D_.reset();
			return;
		}

		object3D_->SetCamera(camera_);
		SyncTransformToObject3D();
		ApplyMaterialBinding(); // モデル再生成後もComponentのMaterial選択を復元する。
	}

	void ModelComponent::SetCamera(Camera* camera)
	{
		camera_ = camera; // Cameraは外部管理なので、ModelComponentでは所有しない。

		if (object3D_)
		{
			object3D_->SetCamera(camera_);
		}
	}

	void ModelComponent::SetMaterialAssetId(std::string_view assetId)
	{
		materialBinding_.SetAssetId(assetId);
		ApplyMaterialBinding(); // Editorやゲームコードからの変更を生成済みObject3Dへ即時反映する。
	}

	void ModelComponent::SetMaterialOverrideEnabled(bool enabled)
	{
		materialBinding_.SetUseOverride(enabled);
		ApplyMaterialBinding(); // Override切り替え時に共有Assetまたはモデル既定へ安全に戻す。
	}

	void ModelComponent::ApplyMaterialBinding()
	{
		if (!object3D_)
		{
			return;
		}
		materialRepositoryRevision_ = MaterialRepository::GetInstance()->GetRevision(); // 今回反映したRepository世代を記録する。

		if (!materialBinding_.HasBinding())
		{
			object3D_->ResetMaterialBinding();
			materialBindingStatus_ = "モデル既定Materialを使用中";
			return;
		}

		MaterialDesc resolvedDesc{};
		if (!materialBinding_.Resolve(resolvedDesc))
		{
			object3D_->ResetMaterialBinding();
			materialBindingStatus_ = "MaterialAssetが見つからないためモデル既定へフォールバック";
			return;
		}

		object3D_->ApplyMaterialDesc(resolvedDesc);
		materialBindingStatus_ = materialBinding_.IsUsingOverride()
			? "Component固有Material Overrideを使用中"
			: "共有MaterialAssetを使用中: " + materialBinding_.GetAssetId();
	}

	void ModelComponent::RefreshSharedMaterialBinding()
	{
		if (materialBinding_.GetAssetId().empty() || materialBinding_.IsUsingOverride())
		{
			return; // モデル既定とComponent固有OverrideはRepository更新の影響を受けない。
		}

		const uint64_t currentRevision = MaterialRepository::GetInstance()->GetRevision();
		if (currentRevision != materialRepositoryRevision_)
		{
			ApplyMaterialBinding(); // MaterialPresetの保存前編集も次の描画フレームへ反映する。
		}
	}

	void ModelComponent::DrawMaterialBindingImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("マテリアル");

		std::vector<std::string> materialIds = MaterialRepository::GetInstance()->GetRegisteredIds();
		std::sort(materialIds.begin(), materialIds.end());
		const std::string preview = materialBinding_.GetAssetId().empty()
			? "モデル既定"
			: materialBinding_.GetAssetId();

		if (ImGui::BeginCombo("共有MaterialAsset", preview.c_str()))
		{
			const bool useModelDefault = materialBinding_.GetAssetId().empty();
			if (ImGui::Selectable("モデル既定", useModelDefault))
			{
				SetMaterialAssetId("");
			}
			if (useModelDefault)
			{
				ImGui::SetItemDefaultFocus();
			}

			for (const std::string& materialId : materialIds)
			{
				const bool selected = materialBinding_.GetAssetId() == materialId;
				if (ImGui::Selectable(materialId.c_str(), selected))
				{
					SetMaterialAssetId(materialId);
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		bool useOverride = materialBinding_.IsUsingOverride();
		if (ImGui::Checkbox("Component固有Materialで上書き", &useOverride))
		{
			SetMaterialOverrideEnabled(useOverride);
		}

		bool changed = false;
		if (materialBinding_.IsUsingOverride())
		{
			MaterialDesc& desc = materialBinding_.GetMutableOverrideDesc();
			changed |= ImGui::Checkbox("PBRを使用", &desc.preferPbrWorkflow);

			if (desc.preferPbrWorkflow)
			{
				changed |= ImGui::ColorEdit4("ベースカラー", &desc.pbr.baseColorFactor.x);
				changed |= ImGui::DragFloat("メタリック", &desc.pbr.metallicFactor, 0.01f, 0.0f, 1.0f);
				changed |= ImGui::DragFloat("粗さ", &desc.pbr.roughnessFactor, 0.01f, 0.04f, 1.0f);
				changed |= ImGui::DragFloat("法線の強さ", &desc.pbr.normalScale, 0.01f, 0.0f, 2.0f);
				changed |= ImGui::DragFloat("AOの強さ", &desc.pbr.occlusionStrength, 0.01f, 0.0f, 1.0f);
				ImGui::TextDisabled("Phase 1ではPBRテクスチャはJSON/Asset保持のみです");
			}
			else
			{
				changed |= ImGui::ColorEdit4("色", &desc.legacy.color.x);
				changed |= ImGui::DragFloat("光沢度", &desc.legacy.shininess, 1.0f, 1.0f, 256.0f);
				changed |= ImGui::DragFloat("反射率", &desc.legacy.reflection, 0.01f, 0.0f, 1.0f);
				changed |= ImGui::DragFloat("粗さ", &desc.legacy.roughness, 0.01f, 0.0f, 1.0f);
				changed |= ImGui::Checkbox("ポイントサンプリング", &desc.legacy.usePointSampling);
			}

			if (changed)
			{
				ApplyMaterialBinding(); // ImGuiで変更した値を同フレームの描画へ反映する。
			}
		}

		ImGui::TextDisabled("状態: %s", materialBindingStatus_.c_str());
#endif // USE_IMGUI
	}

	void ModelComponent::SyncTransformToObject3D()
	{
		if (!object3D_)
		{
			return; // 描画対象がない場合は同期しない
		}

		object3D_->SetTranslate(GetWorldPosition());
		object3D_->SetRotate(GetWorldRotation());
		object3D_->SetScale(GetWorldScale());
	}

	std::vector<ComponentProperty> ModelComponent::CreateProperties(bool includeModelPath)
	{
		std::vector<ComponentProperty> properties = {
			{ "Visible", "表示", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return visible_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) { SetVisible(*typedValue); } } }
		};

		if (includeModelPath)
		{
			properties.insert(properties.begin(),
				{ "ModelPath", "モデルパス", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return modelPath_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<std::string>(&value)) { SetModelPath(*typedValue); } } });
		}

		return properties;
	}
}
