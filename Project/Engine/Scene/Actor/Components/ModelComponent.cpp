#include "ModelComponent.h"
#include "SceneComponent.h"
#include "Actor.h"

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

		object3D_ = std::make_unique<Object3D>();
		object3D_->Initialize(modelPath_);

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
		ImGui::Text("Model Path: %s", modelPath_.empty() ? "None" : modelPath_.c_str());
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
		modelPath_ = std::string(modelPath); // string_viewは保持せず、内部ではstd::stringとして所有する。
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