#include "CameraComponent.h"
#include "CameraManager.h"
#include <Camera.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	void CameraComponent::Initialize()
	{
		SceneComponent::Initialize();
		if (!autoRegisterMainCamera_)
		{
			camera_ = nullptr;
			return;
		}
		camera_ = CameraManager::GetInstance()->GetMainCamera();
		SyncTransformToCamera();
		if (camera_) camera_->Update();
	}

	void CameraComponent::Update(float deltaTime)
	{
		SceneComponent::Update(deltaTime);
		if (!autoRegisterMainCamera_ || !camera_) return;
		SyncTransformToCamera();
		camera_->Update();
	}

	void CameraComponent::UpdateEditor(float deltaTime)
	{
		SceneComponent::UpdateEditor(deltaTime);
		if (!autoRegisterMainCamera_ || !camera_) return;
		SyncTransformToCamera();
		camera_->Update(); // Edit中も位置だけは同期し、モデルのScaleをカメラ行列へ混ぜない。
	}

	void CameraComponent::PostPhysicsUpdate([[maybe_unused]] float deltaTime)
	{
		if (!autoRegisterMainCamera_ || !camera_) return;
		SyncTransformToCamera();
		camera_->Update();
	}

	void CameraComponent::DrawImGui()
	{
		SceneComponent::DrawImGui();
#ifdef USE_IMGUI
		ImGui::SeparatorText("Camera Component");
		bool autoRegister = autoRegisterMainCamera_;
		if (ImGui::Checkbox("Auto Register Main Camera", &autoRegister))
		{
			SetAutoRegisterMainCamera(autoRegister);
		}
		ImGui::Checkbox("Inherit Parent Rotation", &inheritParentRotation_);
		ImGui::TextDisabled("Camera scale is always fixed to 1,1,1.");
#endif // USE_IMGUI
	}

	void CameraComponent::Finalize()
	{
		camera_ = nullptr;
	}

	void CameraComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson);
		outJson["Class"] = GetClassTypeName();
		outJson["AutoRegisterMainCamera"] = autoRegisterMainCamera_;
		outJson["InheritParentRotation"] = inheritParentRotation_;
	}

	void CameraComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson);
		if (inJson.contains("AutoRegisterMainCamera") && inJson["AutoRegisterMainCamera"].is_boolean())
		{
			SetAutoRegisterMainCamera(inJson["AutoRegisterMainCamera"].get<bool>());
		}
		if (inJson.contains("InheritParentRotation") && inJson["InheritParentRotation"].is_boolean())
		{
			inheritParentRotation_ = inJson["InheritParentRotation"].get<bool>();
		}
	}

	void CameraComponent::SetAutoRegisterMainCamera(bool autoRegister)
	{
		autoRegisterMainCamera_ = autoRegister;
		if (!autoRegisterMainCamera_)
		{
			camera_ = nullptr;
			return;
		}
		camera_ = CameraManager::GetInstance()->GetMainCamera();
		SyncTransformToCamera();
		if (camera_) camera_->Update();
	}

	void CameraComponent::SyncTransformToCamera()
	{
		if (!camera_) return;
		camera_->SetTranslate(GetWorldPosition());
		camera_->SetRotate(inheritParentRotation_ ? GetWorldRotation() : GetLocalRotation());
		camera_->SetScale({ 1.0f, 1.0f, 1.0f }); // CameraのView行列へActorやModelのScaleを継承させない。
	}
} // namespace Ken4lowEngine
