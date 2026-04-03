#pragma once
#include "PipelineCommon.h"
#include <d3d12.h>
#include <wrl.h>

namespace Ken4lowEngine
{

	class PipelineFactory
	{
	public:
		PipelineFactory() = default;
		explicit PipelineFactory(ID3D12Device* device);

		void Initialize(ID3D12Device* device);

		PipelineBundle CreateGraphicsPipeline(
			const GraphicsPipelineDesc& desc,
			const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc) const;

	private:
		Microsoft::WRL::ComPtr<ID3DBlob> SerializeRootSignature(
			const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc) const;

	private:
		ID3D12Device* device_ = nullptr;
	};

}