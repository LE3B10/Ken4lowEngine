#include "ModelComponent.h"
#include "SceneComponent.h"
#include "Actor.h"
#include "AssetPathSelector.h"

#include <array>
#include <cstdio>
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
		}
		catch (const std::exception&)
		{
			object3D_.reset();
			return;
		}
		catch (...)
		{
			object3D_.reset();
			return;
		}

		SyncTransformToObject3D();
	}

	void ModelComponent::Update(float deltaTime)
	{
		// 親子関係を考慮したWorldTransformを更新する。
		SceneComponent::Update(deltaTime);

		if (!object3D_)
		{
			return; // モデル未生成の場合は更新しない。
		}

		SyncTransformToObject3D();
		object3D_->Update(); // Object3D側の行列更新やGPU転送を行う。
	}

	void ModelComponent::PostPhysicsUpdate([[maybe_unused]] float deltaTime)
	{
		if (!object3D_)
		{
			return; // モデル未生成の場合は更新しない。
		}

		SyncTransformToObject3D();
		object3D_->Update(); // Object3D側の行列更新やGPU転送を行う。
	}

	void ModelComponent::Draw()
	{
		if (!object3D_)
		{
			return; // 描画対象がない場合は描画しない。
		}

		object3D_->Draw(); // 通常描画パスでモデルを描画する。
	}

	void ModelComponent::DrawShadow()
	{
		if (!object3D_)
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
		std::array<char, 256> modelPathBuffer{};
		std::snprintf(modelPathBuffer.data(), modelPathBuffer.size(), "%s", modelPath_.c_str());
		if (ImGui::InputText("モデルパス", modelPathBuffer.data(), modelPathBuffer.size()))
		{
			SetModelPath(modelPathBuffer.data());
		}

		std::string selectedModelPath = modelPath_;
		if (AssetPathSelector::DrawAssetSelector("一覧から選択##ModelComponentModelPath", selectedModelPath, AssetType::Model))
		{
			SetModelPath(selectedModelPath);
		}

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
		outJson["ModelPath"] = modelPath_;     // モデルパスをJSONへ保存する
	}

	void ModelComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson); // SceneComponent共通情報をJSONから復元する

		if (inJson.contains("ModelPath") && inJson["ModelPath"].is_string())
		{
			modelPath_ = inJson["ModelPath"].get<std::string>(); // モデルパスをJSONから復元する
		}
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
		}
		catch (const std::exception&)
		{
			object3D_.reset();
			return;
		}
		catch (...)
		{
			object3D_.reset();
			return;
		}

		object3D_->SetCamera(camera_);
		SyncTransformToObject3D();
	}

	void ModelComponent::SetCamera(Camera* camera)
	{
		camera_ = camera; // Cameraは外部管理なので、ModelComponentでは所有しない。

		if (object3D_)
		{
			object3D_->SetCamera(camera_);
		}
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
}
