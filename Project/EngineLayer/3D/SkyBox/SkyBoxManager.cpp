#include "SkyBoxManager.h"
#include "DirectXCommon.h"
#include "LogString.h"
#include "ShaderCompiler.h"
#include "SkyBoxShaderManifest.h"
#include <BlendStateFactory.h>
#include <SRVManager.h>

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///                     シングルトンインスタンス
	/// -------------------------------------------------------------
	SkyBoxManager* SkyBoxManager::GetInstance()
	{
		static SkyBoxManager instance;
		return &instance;
	}

	/// -------------------------------------------------------------
	///                         初期化処理
	/// -------------------------------------------------------------
	void SkyBoxManager::Initialize(DirectXCommon* dxCommon)
	{
		dxCommon_ = dxCommon;

		CreateRootSignature();
		CreatePSO();

		graphicsPipelineState_->SetName(L"SkyBoxManager_PSO");
		rootSignature_->SetName(L"SkyBoxManager_RootSignature");
	}

	/// -------------------------------------------------------------
	///                         終了処理
	/// -------------------------------------------------------------
	void SkyBoxManager::Finalize()
	{
		if (!dxCommon_) { return; }

		graphicsPipelineState_.Reset();
		rootSignature_.Reset();
		signatureBlob_.Reset();
		errorBlob_.Reset();

		inputLayoutDesc_ = {};

		dxCommon_ = nullptr;
	}

	/// -------------------------------------------------------------
	///                     共通描画設定処理
	/// -------------------------------------------------------------
	void SkyBoxManager::SetRenderSetting()
	{
		auto commandList = dxCommon_->GetCommandManager()->GetCommandList();
		commandList->SetGraphicsRootSignature(rootSignature_.Get());
		commandList->SetPipelineState(graphicsPipelineState_.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		SRVManager::GetInstance()->PreDraw();
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(2, 0);
	}

	/// -------------------------------------------------------------
	///                 ルートシグネチャを生成する処理
	/// -------------------------------------------------------------
	void SkyBoxManager::CreateRootSignature()
	{
		D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
		descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
		staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
		staticSamplers[0].ShaderRegister = 0;
		staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		descriptionRootSignature.pStaticSamplers = staticSamplers;
		descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

		D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
		descriptorRange[0].BaseShaderRegister = 0;
		descriptorRange[0].NumDescriptors = SRVManager::GetInstance()->GetkMaxSRVCount();
		descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_ROOT_PARAMETER rootParameters[3] = {};

		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[0].Descriptor.ShaderRegister = 0;

		rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParameters[1].Descriptor.ShaderRegister = 0;

		rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
		rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

		descriptionRootSignature.pParameters = rootParameters;
		descriptionRootSignature.NumParameters = _countof(rootParameters);

		HRESULT hr = D3D12SerializeRootSignature(
			&descriptionRootSignature,
			D3D_ROOT_SIGNATURE_VERSION_1,
			&signatureBlob_,
			&errorBlob_);
		if (FAILED(hr))
		{
			Log(reinterpret_cast<char*>(errorBlob_->GetBufferPointer()));
			assert(false);
		}

		hr = dxCommon_->GetDevice()->CreateRootSignature(
			0,
			signatureBlob_->GetBufferPointer(),
			signatureBlob_->GetBufferSize(),
			IID_PPV_ARGS(&rootSignature_));
		assert(SUCCEEDED(hr));
	}

	/// -------------------------------------------------------------
	///                     PSOを生成する処理
	/// -------------------------------------------------------------
	void SkyBoxManager::CreatePSO()
	{
		HRESULT hr{};

		D3D12_INPUT_ELEMENT_DESC inputElementDescs[2] = {};
		inputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
		inputElementDescs[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

		inputLayoutDesc_.pInputElementDescs = inputElementDescs;
		inputLayoutDesc_.NumElements = _countof(inputElementDescs);

		const D3D12_RENDER_TARGET_BLEND_DESC blendDesc =
			BlendStateFactory::GetInstance()->GetBlendDesc(blendMode_);

		D3D12_RASTERIZER_DESC rasterizerDesc{};
		rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterizerDesc.CullMode = D3D12_CULL_MODE_FRONT;
		rasterizerDesc.FrontCounterClockwise = FALSE;

		const ShaderDescriptor& vsDesc =
			SkyBoxShaderManifest::Get(SkyBoxShaderId::SkyBoxVS);
		const ShaderDescriptor& psDesc =
			SkyBoxShaderManifest::Get(SkyBoxShaderId::SkyBoxPS);

		assert(vsDesc.stage == ShaderStage::Vertex);
		assert(psDesc.stage == ShaderStage::Pixel);

		ComPtr<IDxcBlob> vertexShaderBlob = ShaderCompiler::CompileShader(
			vsDesc,
			dxCommon_->GetDXCCompilerManager());
		assert(vertexShaderBlob != nullptr);

		ComPtr<IDxcBlob> pixelShaderBlob = ShaderCompiler::CompileShader(
			psDesc,
			dxCommon_->GetDXCCompilerManager());
		assert(pixelShaderBlob != nullptr);

		D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
		graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
		graphicsPipelineStateDesc.InputLayout = inputLayoutDesc_;
		graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
		graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
		graphicsPipelineStateDesc.BlendState.RenderTarget[0] = blendDesc;
		graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
		graphicsPipelineStateDesc.NumRenderTargets = 1;
		graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		graphicsPipelineStateDesc.SampleDesc.Count = 1;
		graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
		graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
		graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

		hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
			&graphicsPipelineStateDesc,
			IID_PPV_ARGS(&graphicsPipelineState_));
		assert(SUCCEEDED(hr));
	}

} // namespace Ken4lowEngine