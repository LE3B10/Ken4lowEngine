#pragma once
#include "PipelineCommon.h"
#include <d3d12.h>
#include <wrl.h>

namespace Ken4lowEngine
{

	class PipelineFactory
	{
	public: /// ---------- メンバ関数 ---------- ///

		PipelineFactory() = default;
		explicit PipelineFactory(ID3D12Device* device);

		void Initialize(ID3D12Device* device);

		void Finalize();

		PipelineBundle CreateGraphicsPipeline(const GraphicsPipelineDesc& desc, const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc) const;

	private: /// ---------- 内部関数 ---------- ///

		Microsoft::WRL::ComPtr<ID3DBlob> SerializeRootSignature(const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc) const;

	private: /// ---------- メンバ変数 ---------- ///

		Microsoft::WRL::ComPtr<ID3D12Device> device_;
	};

}