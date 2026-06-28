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

		if (!autoRegisterMainCamera_)
		{
			camera_ = nullptr;
			return; // MainCameraへ登録しない場合はCameraを生成しない
		}

		// CameraManagerのMainCameraを取得する
		camera_ = CameraManager::GetInstance()->GetMainCamera();

		SyncTransformToCamera();

		if (camera_)
		{
			camera_->Update(); // MainCameraのViewProjectionを初期計算する
		}
	}

	void CameraComponent::Update(float deltaTime)
	{
		// SceneComponent側でWorldTransformを更新する
		SceneComponent::Update(deltaTime);

		if (!autoRegisterMainCamera_ || !camera_)
		{
			return; // MainCameraとして使わない場合は同期しない
		}

		SyncTransformToCamera();
		camera_->Update(); // Update時点のCamera行列を更新する
	}

	void CameraComponent::PostPhysicsUpdate([[maybe_unused]] float deltaTime)
	{
		if (!autoRegisterMainCamera_ || !camera_)
		{
			return; // MainCameraとして使わない場合は同期しない
		}

		// Physics補正後の親TransformをCameraへ反映する
		SceneComponent::RefreshWorldTransform(); // 親子関係を考慮したWorldTransformを即座に再計算する
		SyncTransformToCamera(); // CameraのTransformを更新する
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

	void CameraComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson); // 親クラスの情報をJSONへ保存する

		outJson["Class"] = GetClassTypeName(); // CameraComponentの種類を保存する
		outJson["AutoRegisterMainCamera"] = autoRegisterMainCamera_; // MainCamera登録フラグを保存する"]
	}

	void CameraComponent::SetAutoRegisterMainCamera(bool autoRegister)
	{
		autoRegisterMainCamera_ = autoRegister;

		if (!autoRegisterMainCamera_)
		{
			camera_ = nullptr; // MainCameraへ登録しない場合はCameraを破棄する
			return;
		}

		camera_ = CameraManager::GetInstance()->GetMainCamera(); // MainCameraへ登録する場合はCameraを取得する
		SyncTransformToCamera(); // CameraのTransformを更新する

		if (camera_)
		{
			camera_->Update(); // 再登録時にCamera行列を即更新する
		}
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