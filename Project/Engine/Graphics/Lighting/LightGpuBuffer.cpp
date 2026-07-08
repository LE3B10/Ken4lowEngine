#include "LightGpuBuffer.h"

#include "DirectXCommon.h"
#include <ResourceManager.h>
#include <SRVManager.h>

#include <cstring>

namespace Ken4lowEngine
{
	void LightGpuBuffer::Initialize(DirectXCommon* dxCommon, const LightManager::LightingSettingsGPU& initialLightingSettings)
	{
		dxCommon_ = dxCommon;

		if (!lightInfoResource_)
		{
			// b2へ渡すライト数CBは既存LightInfoレイアウトのまま生成する。
			lightInfoResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(LightManager::LightInfo));
			lightInfoResource_->Map(0, nullptr, reinterpret_cast<void**>(&lightInfoData_));
			lightInfoData_->lightCount = 0;
			lightInfoResource_->SetName(L"LightInfoConstantBuffer");
		}

		if (!lightingSettingsResource_)
		{
			// b5へ渡すライティング調整CBは既存LightingSettingsGPUレイアウトのまま生成する。
			lightingSettingsResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(LightManager::LightingSettingsGPU));
			lightingSettingsResource_->Map(0, nullptr, reinterpret_cast<void**>(&lightingSettingsData_));
			*lightingSettingsData_ = initialLightingSettings;
			lightingSettingsResource_->SetName(L"LightingSettingsConstantBuffer");
		}

		if (!punctualSRVAllocated_)
		{
			punctualSRVIndex_ = SRVManager::GetInstance()->Allocate();
			punctualSRVAllocated_ = true;
		}

		if (!punctualBuffer_)
		{
			const uint32_t stride = sizeof(LightManager::PunctualLightGPU);
			const uint32_t minElems = 1;
			const uint32_t minBytes = stride * minElems;

			punctualBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), minBytes);
			punctualBufferBytes_ = minBytes;
			punctualBuffer_->SetName(L"PunctualLightBuffer");

			// StructuredBufferのNumElementsは0にできないため、従来通り最低1要素でSRVを作る。
			SRVManager::GetInstance()->CreateSRVForStructureBuffer(punctualSRVIndex_, punctualBuffer_.Get(), minElems, stride);
		}
	}

	void LightGpuBuffer::Finalize()
	{
		if (punctualSRVAllocated_ && punctualSRVIndex_ != UINT32_MAX)
		{
			// SRVスロットの寿命をGPUバッファ管理側に閉じ、二重Freeを防ぐため状態も戻す。
			SRVManager::GetInstance()->Free(punctualSRVIndex_);
			punctualSRVIndex_ = UINT32_MAX;
			punctualSRVAllocated_ = false;
		}

		if (lightInfoResource_)
		{
			lightInfoResource_->Unmap(0, nullptr);
			lightInfoData_ = nullptr;
		}

		if (lightingSettingsResource_)
		{
			lightingSettingsResource_->Unmap(0, nullptr);
			lightingSettingsData_ = nullptr;
		}

		punctualBuffer_.Reset();
		punctualBufferBytes_ = 0;
		lightInfoResource_.Reset();
		lightingSettingsResource_.Reset();
		dxCommon_ = nullptr;
	}

	void LightGpuBuffer::UpdatePunctualLights(
		const std::vector<LightManager::PunctualLightGPU>& punctualLights,
		const std::vector<LightManager::PunctualLightGPU>& lightComponentLights)
	{
		std::vector<LightManager::PunctualLightGPU> sourceLights;
		sourceLights.reserve(punctualLights.size() + lightComponentLights.size());
		sourceLights.insert(sourceLights.end(), punctualLights.begin(), punctualLights.end());
		sourceLights.insert(sourceLights.end(), lightComponentLights.begin(), lightComponentLights.end());

		std::vector<LightManager::PunctualLightGPU> gpuLights;
		gpuLights.reserve(sourceLights.size());
		for (const auto& L : sourceLights)
		{
			if (L.lightType == 0 || L.enabled == 0u) { continue; }
			LightManager::PunctualLightGPU C = L;
			if (C.lightType == 1 || C.lightType == 3 || C.lightType == 4 || C.lightType == 5)
			{
				// HLSLへ渡す方向ライト系のdirection正規化は、既存のGPU転送直前処理をそのまま維持する。
				C.direction = Vector3::Normalize(C.direction);
			}
			gpuLights.push_back(C);
		}

		const uint32_t stride = sizeof(LightManager::PunctualLightGPU);
		const uint32_t elemCount = static_cast<uint32_t>(gpuLights.size());
		const uint32_t safeCount = (elemCount == 0) ? 1u : elemCount;
		const uint32_t bytes = stride * safeCount;

		if (!punctualBuffer_ || punctualBufferBytes_ < bytes)
		{
			// 既存挙動と同じく不足時だけ拡張し、ライト順序とデータ数は変更しない。
			punctualBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), bytes);
			punctualBufferBytes_ = bytes;
			punctualBuffer_->SetName(L"PunctualLightBuffer");
		}

		if (elemCount > 0)
		{
			void* mapped = nullptr;
			punctualBuffer_->Map(0, nullptr, &mapped);
			std::memcpy(mapped, gpuLights.data(), elemCount * stride);
			punctualBuffer_->Unmap(0, nullptr);
		}

		// SRVのNumElementsは従来通り有効ライト数に追従し、0本の場合だけ1に丸める。
		SRVManager::GetInstance()->CreateSRVForStructureBuffer(punctualSRVIndex_, punctualBuffer_.Get(), safeCount, stride);

		if (lightInfoData_)
		{
			lightInfoData_->lightCount = elemCount;
		}
	}

	void LightGpuBuffer::BindPunctualLights(uint32_t rootIndexCB_b2, uint32_t rootIndexSRV_t2)
	{
		auto commandList = dxCommon_->GetCommandManager()->GetCommandList();

		// Descriptor heap設定からCBV/SRV設定までの順序は既存LightManager実装と同じにする。
		SRVManager::GetInstance()->PreDraw();
		commandList->SetGraphicsRootConstantBufferView(rootIndexCB_b2, lightInfoResource_->GetGPUVirtualAddress());
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(rootIndexSRV_t2, punctualSRVIndex_);
	}

	void LightGpuBuffer::BindLightingSettings(uint32_t rootIndexCB_b5, const LightManager::LightingSettingsGPU& lightingSettings)
	{
		auto commandList = dxCommon_->GetCommandManager()->GetCommandList();

		if (lightingSettingsData_)
		{
			// HLSLのLightingSettingsへ最新のAmbient/Exposure/Contrast/Fog/Shadingを送る。
			*lightingSettingsData_ = lightingSettings;
		}

		commandList->SetGraphicsRootConstantBufferView(rootIndexCB_b5, lightingSettingsResource_->GetGPUVirtualAddress());
	}
}
