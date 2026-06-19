#include "InstancedObject3DRenderer.h"
#include "Object3DCommon.h"
#include "DirectXCommon.h"
#include "ResourceManager.h"
#include "SRVManager.h"
#include "TextureManager.h"
#include "Model.h"
#include "ModelManager.h"
#include "CameraManager.h"

#include <algorithm>
#include <stdexcept>

namespace Ken4lowEngine
{
	InstancedObject3DRenderer::~InstancedObject3DRenderer()
	{
		Finalize();
	}

	void InstancedObject3DRenderer::Initialize(const std::string& modelPath, size_t maxInstanceCount)
	{
		if (maxInstanceCount == 0 || maxInstanceCount > UINT32_MAX)
		{
			throw std::invalid_argument("InstancedObject3DRenderer maxInstanceCount is invalid");
		}

		Finalize();
		dxCommon_ = DirectXCommon::GetInstance();
		model_ = ModelManager::GetInstance()->LoadModel(modelPath);
		if (!model_) { throw std::runtime_error("InstancedObject3DRenderer failed to load model"); }

		maxInstanceCount_ = maxInstanceCount;
		instanceResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(InstanceData) * maxInstanceCount_);
		instanceResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedInstances_));
		instanceResource_->SetName(L"InstancedObject3DRenderer.InstanceBuffer");

		auto* srvManager = SRVManager::GetInstance();
		instanceSrvIndex_ = srvManager->Allocate();
		srvManager->CreateSRVForStructureBuffer(instanceSrvIndex_, instanceResource_.Get(), static_cast<UINT>(maxInstanceCount_), sizeof(InstanceData));

		perViewResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(PerViewData));
		perViewResource_->Map(0, nullptr, reinterpret_cast<void**>(&perViewData_));
		cameraResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(CameraForGPU));
		cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));
		dissolveResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(DissolveSetting));
		dissolveResource_->Map(0, nullptr, reinterpret_cast<void**>(&dissolveData_));
		shadowParameterResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(ShadowParameterForGPU));
		shadowParameterResource_->Map(0, nullptr, reinterpret_cast<void**>(&shadowParameterData_));

		material_.Initialize();
		materialSRVs_ = model_->GetMaterialSRVs();
		materialUsePointSampling_ = model_->GetMaterialPointSamplingFlags();

		TextureManager::GetInstance()->LoadTexture("SkyBox/skybox.dds");
		environmentMapHandle_ = TextureManager::GetInstance()->GetSrvHandleGPU("SkyBox/skybox.dds");
		TextureManager::GetInstance()->LoadTexture("Effects/Masks/noise.dds");
		dissolveMaskHandle_ = TextureManager::GetInstance()->GetSrvHandleGPU("Effects/Masks/noise.dds");

		dissolveData_->threshold = 1.0f;
		dissolveData_->edgeThickness = 0.0f;
		dissolveData_->padding[0] = dissolveData_->padding[1] = 0.0f;
		dissolveData_->edgeColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		shadowParameterData_->lightViewProjection = Matrix4x4::MakeIdentity();
		shadowParameterData_->shadowBias = 0.0f;
		shadowParameterData_->normalBias = 0.0f;
		shadowParameterData_->shadowStrength = 0.0f;
		shadowParameterData_->shadowMode = 0;
		shadowParameterData_->shadowDebugMode = 0;
		shadowParameterData_->padding[0] = 0.0f;
		initialized_ = true;
	}

	void InstancedObject3DRenderer::Finalize()
	{
		if (instanceSrvIndex_ != UINT32_MAX)
		{
			SRVManager::GetInstance()->Free(instanceSrvIndex_);
			instanceSrvIndex_ = UINT32_MAX;
		}
		instanceResource_.Reset();
		perViewResource_.Reset();
		cameraResource_.Reset();
		dissolveResource_.Reset();
		shadowParameterResource_.Reset();
		mappedInstances_ = nullptr;
		perViewData_ = nullptr;
		cameraData_ = nullptr;
		dissolveData_ = nullptr;
		shadowParameterData_ = nullptr;
		model_.reset();
		materialSRVs_.clear();
		materialUsePointSampling_.clear();
		maxInstanceCount_ = 0;
		instanceCount_ = 0;
		dxCommon_ = nullptr;
		initialized_ = false;
	}

	bool InstancedObject3DRenderer::SetInstances(const std::vector<InstanceData>& instances)
	{
		if (!initialized_ || instances.size() > maxInstanceCount_) { return false; }
		std::copy(instances.begin(), instances.end(), mappedInstances_);
		instanceCount_ = instances.size();
		return true;
	}

	bool InstancedObject3DRenderer::SetWorldMatrices(const std::vector<Matrix4x4>& worldMatrices, const Vector4& color)
	{
		if (!initialized_ || worldMatrices.size() > maxInstanceCount_) { return false; }
		for (size_t i = 0; i < worldMatrices.size(); ++i)
		{
			mappedInstances_[i].world = worldMatrices[i];
			mappedInstances_[i].worldInverseTranspose = Matrix4x4::Transpose(Matrix4x4::Inverse(worldMatrices[i]));
			mappedInstances_[i].color = color;
		}
		instanceCount_ = worldMatrices.size();
		return true;
	}

	void InstancedObject3DRenderer::Draw()
	{
		if (!initialized_ || !model_ || instanceCount_ == 0) { return; }

		perViewData_->viewProjection = CameraManager::GetInstance()->GetActiveViewProjectionMatrix();
		const Vector3 cameraPosition = CameraManager::GetInstance()->GetActiveCameraPosition();
		cameraData_->x = cameraPosition.x;
		cameraData_->y = cameraPosition.y;
		cameraData_->z = cameraPosition.z;
		cameraData_->padding = 0.0f;
		material_.Update();

		auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		SRVManager::GetInstance()->PreDraw();
		Object3DCommon::GetInstance()->SetInstancedRenderSetting();
		material_.SetPipeline(0);
		commandList->SetGraphicsRootConstantBufferView(1, perViewResource_->GetGPUVirtualAddress());
		commandList->SetGraphicsRootConstantBufferView(3, cameraResource_->GetGPUVirtualAddress());
		TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 4, environmentMapHandle_);
		commandList->SetGraphicsRootConstantBufferView(7, dissolveResource_->GetGPUVirtualAddress());
		TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 8, dissolveMaskHandle_);
		commandList->SetGraphicsRootConstantBufferView(9, shadowParameterResource_->GetGPUVirtualAddress());
		commandList->SetGraphicsRootDescriptorTable(10, dxCommon_->GetShadowMapSrvHandleGPU());
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(12, instanceSrvIndex_);

		// 同一Modelの各サブメッシュを、全インスタンス分まとめて少数のDrawIndexedInstancedで描画する。
		auto& meshes = model_->GetMeshes();
		for (size_t i = 0; i < meshes.size(); ++i)
		{
			if (i < materialSRVs_.size())
			{
				TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 2, materialSRVs_[i]);
				material_.SetUsePointSampling(i < materialUsePointSampling_.size() ? materialUsePointSampling_[i] : false);
				material_.Update();
			}
			meshes[i].DrawInstanced(static_cast<UINT>(instanceCount_));
		}
	}
}
