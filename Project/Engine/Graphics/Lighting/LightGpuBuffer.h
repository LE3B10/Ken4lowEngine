#pragma once

#include "DX12Include.h"
#include "LightManager.h"

#include <cstdint>
#include <vector>

namespace Ken4lowEngine
{
	class DirectXCommon;

	/// <summary>
	/// LightManagerが所有するライト状態をHLSLへ渡すためのGPUバッファ管理クラスです。<br/>
	/// ライトデータの最終所有やImGui/Preset処理は行わず、既存のCBV/SRVレイアウトとバインド順を保ったまま
	/// DirectX12リソース生成・更新・バインド責務だけをLightManagerから分離します。
	/// </summary>
	class LightGpuBuffer
	{
	public:
		/// <summary>
		/// ライト数CB、LightingSettings CB、PunctualLight StructuredBuffer/SRVを初期化します。<br/>
		/// HLSL側のレイアウト維持のため、LightManager内の既存構造体サイズをそのまま利用します。
		/// </summary>
		void Initialize(DirectXCommon* dxCommon, const LightManager::LightingSettingsGPU& initialLightingSettings);

		/// <summary>
		/// 確保済みSRVスロットとMap済みCBリソースを解放します。<br/>
		/// LightManager::Finalizeから呼ばれ、GPUリソース寿命をこのクラス内に閉じます。
		/// </summary>
		void Finalize();

		/// <summary>
		/// CPU側ライト配列から有効ライトだけを抽出し、StructuredBufferとライト数CBへ転送します。<br/>
		/// 転送順序は従来通り、グローバル/Legacyライトの後にLightComponent由来のライトを連結します。
		/// </summary>
		void UpdatePunctualLights(
			const std::vector<LightManager::PunctualLightGPU>& punctualLights,
			const std::vector<LightManager::PunctualLightGPU>& lightComponentLights);

		/// <summary>
		/// PunctualLight用のライト数CBVとStructuredBuffer SRVを既存root indexへバインドします。<br/>
		/// SetGraphicsRootConstantBufferView / SRVManager::SetGraphicsRootDescriptorTable の順序は従来通りです。
		/// </summary>
		void BindPunctualLights(uint32_t rootIndexCB_b2, uint32_t rootIndexSRV_t2);

		/// <summary>
		/// LightingSettingsをCBへ反映し、既存root indexへバインドします。<br/>
		/// Ambient/Exposure/FogなどのHLSL定数バッファレイアウトはLightManager::LightingSettingsGPUのまま維持します。
		/// </summary>
		void BindLightingSettings(uint32_t rootIndexCB_b5, const LightManager::LightingSettingsGPU& lightingSettings);

	private:
		DirectXCommon* dxCommon_ = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> punctualBuffer_;
		uint32_t punctualBufferBytes_ = 0;
		uint32_t punctualSRVIndex_ = UINT32_MAX;
		bool punctualSRVAllocated_ = false;
		Microsoft::WRL::ComPtr<ID3D12Resource> lightInfoResource_;
		LightManager::LightInfo* lightInfoData_ = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> lightingSettingsResource_;
		LightManager::LightingSettingsGPU* lightingSettingsData_ = nullptr;
	};
}
