#include "PipelineFactory.h"
#include <cassert>
#include <stdexcept>

namespace Ken4lowEngine
{

	PipelineFactory::PipelineFactory(ID3D12Device* device)
		: device_(device) {}

	void PipelineFactory::Initialize(ID3D12Device* device)
	{
		device_ = device;
	}

	Microsoft::WRL::ComPtr<ID3DBlob> PipelineFactory::SerializeRootSignature(const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc) const
	{
		assert(device_);

		Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

		HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);

		if (FAILED(hr))
		{
			if (errorBlob)
			{
				OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
			}
			throw std::runtime_error("Failed to serialize root signature.");
		}

		return signatureBlob;
	}

	PipelineBundle PipelineFactory::CreateGraphicsPipeline(const GraphicsPipelineDesc& desc, const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc) const
	{
		assert(device_);

		PipelineBundle out{};

		auto signatureBlob = SerializeRootSignature(rootSignatureDesc);

		HRESULT hr = device_->CreateRootSignature(
			0,
			signatureBlob->GetBufferPointer(),
			signatureBlob->GetBufferSize(),
			IID_PPV_ARGS(&out.rootSignature));
		if (FAILED(hr)) {
			throw std::runtime_error("Failed to create root signature.");
		}

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.pRootSignature = out.rootSignature.Get();

		if (desc.shaders.vertexShader.blob)
		{
			psoDesc.VS = {
				desc.shaders.vertexShader.blob->GetBufferPointer(),
				desc.shaders.vertexShader.blob->GetBufferSize()
			};
		}
		if (desc.shaders.pixelShader.blob)
		{
			psoDesc.PS = {
				desc.shaders.pixelShader.blob->GetBufferPointer(),
				desc.shaders.pixelShader.blob->GetBufferSize()
			};
		}
		if (desc.shaders.geometryShader.blob)
		{
			psoDesc.GS = {
				desc.shaders.geometryShader.blob->GetBufferPointer(),
				desc.shaders.geometryShader.blob->GetBufferSize()
			};
		}
		if (desc.shaders.hullShader.blob)
		{
			psoDesc.HS = {
				desc.shaders.hullShader.blob->GetBufferPointer(),
				desc.shaders.hullShader.blob->GetBufferSize()
			};
		}
		if (desc.shaders.domainShader.blob)
		{
			psoDesc.DS = {
				desc.shaders.domainShader.blob->GetBufferPointer(),
				desc.shaders.domainShader.blob->GetBufferSize()
			};
		}

		psoDesc.BlendState = desc.blendState;
		psoDesc.RasterizerState = desc.rasterizerState;
		psoDesc.DepthStencilState = desc.depthStencilState;
		psoDesc.InputLayout = desc.inputLayout;
		psoDesc.PrimitiveTopologyType = desc.primitiveTopologyType;
		psoDesc.NumRenderTargets = desc.numRenderTargets;
		psoDesc.SampleMask = desc.sampleMask;
		psoDesc.SampleDesc.Count = desc.sampleCount;
		psoDesc.DSVFormat = desc.dsvFormat;

		for (UINT i = 0; i < desc.numRenderTargets; ++i)
		{
			psoDesc.RTVFormats[i] = desc.rtvFormats[i];
		}

		hr = device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&out.pipelineState));
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create graphics pipeline state.");
		}

		return out;
	}

}