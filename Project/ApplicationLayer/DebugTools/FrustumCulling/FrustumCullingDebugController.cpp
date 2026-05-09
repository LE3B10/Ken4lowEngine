#define NOMINMAX
#include "FrustumCullingDebugController.h"

#include "Camera.h"
#include "CameraManager.h"
#include "Engine/Graphics/Culling/FrustumDebugRenderer.h"
#include "Object3DCommon.h"
#include "WinApp.h"

#ifdef _DEBUG
#include "DebugCamera.h"
#endif

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <algorithm>
#include <numbers>

namespace
{
	constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
	constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>;

	K4E::Vector3 GetDefaultMainCameraPosition()
	{
		return { 0.0f, 3.0f, -12.0f };
	}

	K4E::Vector3 GetDefaultMainCameraRotation()
	{
		return { 8.0f * kDegToRad, 0.0f, 0.0f };
	}
}

void FrustumCullingDebugController::Initialize(bool resetMainCamera)
{
	if (resetMainCamera)
	{
		ResetMainCamera();
	}
}

void FrustumCullingDebugController::Update(float deltaTime)
{
	(void)deltaTime;
}

void FrustumCullingDebugController::DrawDebug()
{
#ifdef _DEBUG
	if (!showFrustumWireframe_) { return; }

	const K4E::Matrix4x4 viewProjection = K4E::Object3DCommon::GetInstance()->GetCullingViewProjectionMatrix();
	K4E::FrustumDebugRenderer{}.Draw(viewProjection, frustumWireColor_);
#endif
}

void FrustumCullingDebugController::DrawImGui()
{
#ifdef USE_IMGUI
	K4E::Object3DCommon* object3DCommon = K4E::Object3DCommon::GetInstance();
	int cullingCameraIndex = static_cast<int>(object3DCommon->GetCullingCameraMode());
	const char* cullingCameraItems[] = { "ActiveCamera", "MainCamera", "DebugCamera" };

	bool frustumCullingEnabled = object3DCommon->IsFrustumCullingEnabled();
	float nearDistance = 0.0f;
	float farDistance = 0.0f;
	GetCullingCameraClipDistances(nearDistance, farDistance);

	ImGui::Begin("Frustum Culling Debug");
	if (ImGui::Checkbox("Frustum Culling Enabled", &frustumCullingEnabled))
	{
		object3DCommon->SetFrustumCullingEnabled(frustumCullingEnabled);
	}
	if (ImGui::Combo("Culling Camera", &cullingCameraIndex, cullingCameraItems, IM_ARRAYSIZE(cullingCameraItems)))
	{
		object3DCommon->SetCullingCameraMode(static_cast<K4E::Object3DCommon::CullingCameraMode>(cullingCameraIndex));
	}

	ImGui::Separator();
	ImGui::Text("ActiveCamera: %s", K4E::CameraManager::GetInstance()->IsUsingDebugCamera() ? "DebugCamera" : "MainCamera");
	ImGui::Text("MainCamera: %s", K4E::CameraManager::GetInstance()->GetMainCamera() ? "Available" : "None");
#ifdef _DEBUG
	ImGui::Text("DebugCamera: %s", K4E::CameraManager::GetInstance()->GetDebugCamera() ? "Available" : "None");
#else
	ImGui::Text("DebugCamera: Disabled in release builds");
#endif
	ImGui::Checkbox("Show Frustum Wireframe", &showFrustumWireframe_);
	ImGui::ColorEdit4("Frustum Wire Color", &frustumWireColor_.x);
	ImGui::InputFloat("Near", &nearDistance, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_ReadOnly);
	ImGui::InputFloat("Far", &farDistance, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_ReadOnly);

	ImGui::Separator();
	ImGui::Text("Total Objects: %d", object3DCommon->GetTotalObjectCount());
	ImGui::Text("Drawn Objects: %d", object3DCommon->GetDrawnObjectCount());
	ImGui::Text("Culled Objects: %d", object3DCommon->GetCulledObjectCount());
	ImGui::TextWrapped("Use DebugCamera outside the view and select MainCamera to inspect gameplay culling results.");

	DrawMainCameraImGui();
	ImGui::End();
#endif
}

void FrustumCullingDebugController::DrawMainCameraImGui()
{
#ifdef USE_IMGUI
	K4E::Camera* mainCamera = K4E::CameraManager::GetInstance()->GetMainCamera();
	if (!mainCamera)
	{
		ImGui::Separator();
		ImGui::Text("MainCamera is not set.");
		return;
	}

	if (ImGui::CollapsingHeader("MainCamera Settings", ImGuiTreeNodeFlags_DefaultOpen))
	{
		K4E::Vector3 position = mainCamera->GetTranslate();
		K4E::Vector3 rotation = mainCamera->GetRotate();
		float fovDeg = mainCamera->GetFovY() * kRadToDeg;
		float nearClip = mainCamera->GetNearClip();
		float farClip = mainCamera->GetFarClip();
		float aspectRatio = mainCamera->GetAspectRatio();
		bool changed = false;

		changed |= ImGui::DragFloat3("Position", &position.x, 0.05f);
		changed |= ImGui::DragFloat3("Rotation", &rotation.x, 0.005f);
		changed |= ImGui::SliderFloat("FOV", &fovDeg, 10.0f, 140.0f, "%.1f deg");
		changed |= ImGui::DragFloat("Near Clip", &nearClip, 0.001f, 0.001f, 10.0f, "%.3f");
		changed |= ImGui::DragFloat("Far Clip", &farClip, 0.5f, 1.0f, 1000.0f, "%.1f");
		changed |= ImGui::DragFloat("Aspect", &aspectRatio, 0.001f, 0.1f, 4.0f, "%.3f");

		if (changed)
		{
			farClip = std::max(farClip, 0.011f);
			nearClip = std::clamp(nearClip, 0.001f, farClip - 0.01f);
			farClip = std::max(farClip, nearClip + 0.01f);
			aspectRatio = std::max(aspectRatio, 0.1f);
			mainCamera->SetTranslate(position);
			mainCamera->SetRotate(rotation);
			mainCamera->SetFovY(fovDeg * kDegToRad);
			mainCamera->SetNearClip(nearClip);
			mainCamera->SetFarClip(farClip);
			mainCamera->SetAspectRatio(aspectRatio);
			mainCamera->Update();
		}

		if (ImGui::Button("Reset MainCamera"))
		{
			ResetMainCamera();
		}
#ifdef _DEBUG
		ImGui::SameLine();
		if (ImGui::Button("Copy Current DebugCamera to MainCamera"))
		{
			CopyDebugCameraToMainCamera();
		}
#endif
	}
#endif
}

void FrustumCullingDebugController::CopyDebugCameraToMainCamera()
{
#ifdef _DEBUG
	K4E::Camera* mainCamera = K4E::CameraManager::GetInstance()->GetMainCamera();
	K4E::DebugCamera* debugCamera = K4E::CameraManager::GetInstance()->GetDebugCamera();
	if (!mainCamera || !debugCamera) { return; }

	mainCamera->SetTranslate(debugCamera->GetTranslate());
	mainCamera->SetRotate(debugCamera->GetRotate());
	mainCamera->SetFovY(debugCamera->GetFovY());
	mainCamera->SetNearClip(debugCamera->GetNearClip());
	mainCamera->SetFarClip(debugCamera->GetFarClip());
	mainCamera->SetAspectRatio(debugCamera->GetAspectRatio());
	mainCamera->Update();
#endif
}

void FrustumCullingDebugController::ResetMainCamera()
{
	K4E::Camera* mainCamera = K4E::CameraManager::GetInstance()->GetMainCamera();
	if (!mainCamera) { return; }

	mainCamera->SetTranslate(GetDefaultMainCameraPosition());
	mainCamera->SetRotate(GetDefaultMainCameraRotation());
	mainCamera->SetFovY(60.0f * kDegToRad);
	mainCamera->SetNearClip(0.1f);
	mainCamera->SetFarClip(80.0f);
	mainCamera->SetAspectRatio(GetCurrentWindowAspectRatio());
	mainCamera->Update();
}

void FrustumCullingDebugController::GetCullingCameraClipDistances(float& nearDistance, float& farDistance) const
{
	nearDistance = 0.0f;
	farDistance = 0.0f;

	const K4E::Object3DCommon::CullingCameraMode mode = K4E::Object3DCommon::GetInstance()->GetCullingCameraMode();
	auto* cameraManager = K4E::CameraManager::GetInstance();

	auto setMainCameraClips = [&]() -> bool
		{
			if (K4E::Camera* mainCamera = cameraManager->GetMainCamera())
			{
				nearDistance = mainCamera->GetNearClip();
				farDistance = mainCamera->GetFarClip();
				return true;
			}
			return false;
		};

	auto setDebugCameraClips = [&]() -> bool
		{
#ifdef _DEBUG
			if (K4E::DebugCamera* debugCamera = cameraManager->GetDebugCamera())
			{
				nearDistance = debugCamera->GetNearClip();
				farDistance = debugCamera->GetFarClip();
				return true;
			}
#endif
			return false;
		};

	switch (mode)
	{
	case K4E::Object3DCommon::CullingCameraMode::MainCamera:
		if (setMainCameraClips()) { return; }
		break;
	case K4E::Object3DCommon::CullingCameraMode::DebugCamera:
		if (setDebugCameraClips()) { return; }
		break;
	case K4E::Object3DCommon::CullingCameraMode::ActiveCamera:
	default:
		if (cameraManager->IsUsingDebugCamera())
		{
			if (setDebugCameraClips()) { return; }
		}
		if (setMainCameraClips()) { return; }
		break;
	}

	setMainCameraClips();
}

float FrustumCullingDebugController::GetCurrentWindowAspectRatio() const
{
	const float height = static_cast<float>(std::max(K4E::WinApp::GetInstance()->GetClientHeight(), 1u));
	return static_cast<float>(K4E::WinApp::GetInstance()->GetClientWidth()) / height;
}
