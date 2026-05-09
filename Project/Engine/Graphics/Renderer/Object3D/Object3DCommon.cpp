#include "Object3DCommon.h"
#include "DirectXCommon.h"
#include "CameraManager.h"
#include "DebugCamera.h"

#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	Object3DCommon* Object3DCommon::GetInstance()
	{
		static Object3DCommon instance;
		return &instance;
	}

	void Object3DCommon::Initialize(DirectXCommon* dxCommon)
	{
		dxCommon_ = dxCommon;
		cullingCameraMode_ = CullingCameraMode::Active;

		pipelineSet_.Initialize(dxCommon_->GetPipelineFactory(), dxCommon_->GetDXCCompilerManager(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_D24_UNORM_S8_UINT);

		LightManager::GetInstance()->Initialize(dxCommon_);
	}

	void Object3DCommon::Finalize()
	{
		LightManager::GetInstance()->Finalize();
		pipelineSet_.Finalize();
		cullingCameraMode_ = CullingCameraMode::Active;
		dxCommon_ = nullptr;
	}

	void Object3DCommon::BeginObject3DPass()
	{
		drawnObjectCount_ = 0;
		culledObjectCount_ = 0;
		totalObjectCount_ = 0;
		activeFrustum_.BuildFromViewProjection(GetCullingViewProjectionMatrix());
	}

	void Object3DCommon::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::Begin("Object3D Frustum Culling");
		ImGui::Checkbox("Frustum Culling 有効", &frustumCullingEnabled_);
		ImGui::Separator();
		ImGui::Text("現在の描画対象数: %d", drawnObjectCount_);
		ImGui::Text("カリングされた数: %d", culledObjectCount_);
		ImGui::Text("総オブジェクト数: %d", totalObjectCount_);
		ImGui::End();
#endif
	}

	Matrix4x4 Object3DCommon::GetCullingViewProjectionMatrix() const
	{
		auto* cameraManager = CameraManager::GetInstance();
		switch (cullingCameraMode_)
		{
		case CullingCameraMode::Main:
			if (Camera* mainCamera = cameraManager->GetMainCamera())
			{
				return mainCamera->GetViewProjectionMatrix();
			}
			break;
		case CullingCameraMode::Debug:
#ifdef _DEBUG
			if (DebugCamera* debugCamera = cameraManager->GetDebugCamera())
			{
				return debugCamera->GetViewProjectionMatrix();
			}
#endif
			break;
		case CullingCameraMode::Active:
		default:
			return cameraManager->GetActiveViewProjectionMatrix();
		}

		return cameraManager->GetActiveViewProjectionMatrix();
	}

	void Object3DCommon::SetRenderSetting()
	{
		auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		const PipelineBundle& pipeline = pipelineSet_.GetDefault();

		commandList->SetGraphicsRootSignature(pipeline.rootSignature.Get());
		commandList->SetPipelineState(pipeline.pipelineState.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		LightManager::GetInstance()->BindPunctualLights(5, 6);
	}

	bool Object3DCommon::ShouldDrawObject(const BoundingSphere& worldBounds, bool objectCullingEnabled)
	{
		++totalObjectCount_;

		if (!frustumCullingEnabled_ || !objectCullingEnabled || activeFrustum_.Intersects(worldBounds))
		{
			++drawnObjectCount_;
			return true;
		}

		++culledObjectCount_;
		return false;
	}

	void Object3DCommon::SetShadowMapRenderSetting()
	{
		auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		const PipelineBundle& pipeline = pipelineSet_.GetShadow();

		commandList->SetGraphicsRootSignature(pipeline.rootSignature.Get());
		commandList->SetPipelineState(pipeline.pipelineState.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}
}