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
		cullingCameraMode_ = CullingCameraMode::MainCamera;

		pipelineSet_.Initialize(dxCommon_->GetPipelineFactory(), dxCommon_->GetDXCCompilerManager(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_D24_UNORM_S8_UINT);
		instancedPipelineSet_.Initialize(dxCommon_->GetPipelineFactory(), dxCommon_->GetDXCCompilerManager(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_D24_UNORM_S8_UINT);

		LightManager::GetInstance()->Initialize(dxCommon_);
	}

	void Object3DCommon::Finalize()
	{
		LightManager::GetInstance()->Finalize();
		pipelineSet_.Finalize();
		instancedPipelineSet_.Finalize();
		cullingCameraMode_ = CullingCameraMode::MainCamera;
		dxCommon_ = nullptr;
	}

	void Object3DCommon::BeginObject3DPass()
	{
		frustumCullingSystem_.ResetStatistics();
		frustumCullingSystem_.BuildFrustum(GetCullingViewProjectionMatrix());
	}

	void Object3DCommon::DrawImGui()
	{
		// Frustum Culling の確認 UI は ApplicationLayer の Controller に集約する。
	}

	Matrix4x4 Object3DCommon::GetCullingViewProjectionMatrix() const
	{
		auto* cameraManager = CameraManager::GetInstance();
		switch (cullingCameraMode_)
		{
		case CullingCameraMode::MainCamera:
			if (Camera* mainCamera = cameraManager->GetMainCamera())
			{
				return mainCamera->GetViewProjectionMatrix();
			}
			break;
		case CullingCameraMode::DebugCamera:
#ifdef _DEBUG
			if (DebugCamera* debugCamera = cameraManager->GetDebugCamera())
			{
				return debugCamera->GetViewProjectionMatrix();
			}
#endif
			break;
		case CullingCameraMode::ActiveCamera:
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
		LightManager::GetInstance()->BindLightingSettings(11);
		LightManager::GetInstance()->BindExtendedShadowResources(16, 17, 18);
	}

	void Object3DCommon::SetInstancedRenderSetting()
	{
		auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		const PipelineBundle& pipeline = instancedPipelineSet_.GetDefault();

		commandList->SetGraphicsRootSignature(pipeline.rootSignature.Get());
		commandList->SetPipelineState(pipeline.pipelineState.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		LightManager::GetInstance()->BindPunctualLights(5, 6);
		LightManager::GetInstance()->BindLightingSettings(11);
		LightManager::GetInstance()->BindExtendedShadowResources(17, 18, 19);
	}

	bool Object3DCommon::ShouldDrawObject(const BoundingSphere& worldBounds, bool objectCullingEnabled, bool hasBounds, bool isStageObject)
	{
		const auto unit = isStageObject ? FrustumCullingSystem::CullingUnit::StageObject : FrustumCullingSystem::CullingUnit::Object;
		return frustumCullingSystem_.IsVisible(worldBounds, !objectCullingEnabled, hasBounds, unit);
	}

	bool Object3DCommon::ShouldDrawMesh(const BoundingSphere& worldBounds, bool objectCullingEnabled, bool hasBounds)
	{
		return frustumCullingSystem_.IsVisible(worldBounds, !objectCullingEnabled, hasBounds, FrustumCullingSystem::CullingUnit::Mesh);
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
