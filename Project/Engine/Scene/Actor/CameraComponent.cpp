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
		// 親子関係を考慮したWorldTransformを初期計算する
		SceneComponent::Initialize();

		// CameraManagerのMainCameraを取得する
		camera_ = CameraManager::GetInstance()->GetMainCamera();

		SyncTransformToCamera();
	}

	void CameraComponent::Update(float deltaTime)
	{
		// SceneComponent側でWorldTransformを更新する
		SceneComponent::Update(deltaTime); 

		if (!camera_)
		{
			return; // Cameraが生成されていない場合は同期できない
		}

		SyncTransformToCamera();

		// MainCameraのViewProjectionを更新する
		camera_->Update();
	}

	void CameraComponent::DrawImGui()
	{
		SceneComponent::DrawImGui(); // CameraComponentのLocal / World Transformを表示する

#ifdef USE_IMGUI
		ImGui::SeparatorText("Camera Component");
		ImGui::Checkbox("Auto Register Main Camera", &autoRegisterMainCamera_);
#endif // USE_IMGUI
	}

	void CameraComponent::Finalize()
	{
		camera_ = nullptr; // CameraComponentが所有しているCameraを破棄する
	}

	void CameraComponent::SyncTransformToCamera()
	{
		if (!camera_)
		{
			return; // Cameraが無い場合はTransformを同期しない
		}

		camera_->SetTranslate(GetWorldPosition());
		camera_->SetRotate(GetWorldRotation());
		camera_->SetScale(GetWorldScale());
	}
}