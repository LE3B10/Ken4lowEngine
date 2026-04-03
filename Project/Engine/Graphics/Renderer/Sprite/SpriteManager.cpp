#include "SpriteManager.h"
#include "LogString.h"
#include "ShaderCompiler.h"
#include "SpriteShaderManifest.h"
#include <BlendStateFactory.h>
#include <SRVManager.h>
#include "DirectXCommon.h"

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///                     シングルトンインスタンス
	/// -------------------------------------------------------------
	SpriteManager* SpriteManager::GetInstance()
	{
		static SpriteManager instance;
		return &instance;
	}

	/// -------------------------------------------------------------
	///                         初期化処理
	/// -------------------------------------------------------------
	void SpriteManager::Initialize(DirectXCommon* dxCommon)
	{
		dxCommon_ = dxCommon;

		CreatePSO();

		graphicsPipelineState_Background_->SetName(L"SpriteManager::graphicsPipelineState_Background_");
		graphicsPipelineState_UI_->SetName(L"SpriteManager::graphicsPipelineState_UI_");
		rootSignature_->SetName(L"SpriteManager::rootSignature_");
	}

	void SpriteManager::Finalize()
	{
		graphicsPipelineState_Background_.Reset();
		graphicsPipelineState_UI_.Reset();
		rootSignature_.Reset();
		signatureBlob_.Reset();
		errorBlob_.Reset();

		dxCommon_ = nullptr;
	}

	/// -------------------------------------------------------------
	///                     背景用の共通描画設定
	/// -------------------------------------------------------------
	void SpriteManager::SetRenderSetting_Background()
	{
		auto commandList = dxCommon_->GetCommandManager()->GetCommandList();

		commandList->SetGraphicsRootSignature(rootSignature_.Get());
		commandList->SetPipelineState(graphicsPipelineState_Background_.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		SRVManager::GetInstance()->PreDraw();
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(3, 0);
	}

	/// -------------------------------------------------------------
	///                     UI用の共通描画設定
	/// -------------------------------------------------------------
	void SpriteManager::SetRenderSetting_UI()
	{
		auto commandList = dxCommon_->GetCommandManager()->GetCommandList();

		commandList->SetGraphicsRootSignature(rootSignature_.Get());
		commandList->SetPipelineState(graphicsPipelineState_UI_.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		SRVManager::GetInstance()->PreDraw();
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(3, 0);
	}

	/// -------------------------------------------------------------
	///                     ルートシグネチャの生成
	/// -------------------------------------------------------------
	void SpriteManager::CreateRootSignature()
	{
		D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
		descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
		staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
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

		D3D12_ROOT_PARAMETER rootParameters[4] = {};

		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[0].Descriptor.ShaderRegister = 0;

		rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[1].Descriptor.ShaderRegister = 1;
		rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParameters[2].Descriptor.ShaderRegister = 0;

		rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[3].DescriptorTable.pDescriptorRanges = descriptorRange;
		rootParameters[3].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

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
	///                         パイプラインの生成
	/// -------------------------------------------------------------
	void SpriteManager::CreatePSO()
	{
		HRESULT hr{};

		CreateRootSignature();

		D3D12_INPUT_ELEMENT_DESC inputElementDescs[2] = {};
		inputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
		inputElementDescs[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

		D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
		inputLayoutDesc.pInputElementDescs = inputElementDescs;
		inputLayoutDesc.NumElements = _countof(inputElementDescs);

		const D3D12_RENDER_TARGET_BLEND_DESC& blendDesc =
			BlendStateFactory::GetInstance()->GetBlendDesc(blendMode_);

		D3D12_RASTERIZER_DESC rasterizerDesc{};
		rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
		rasterizerDesc.FrontCounterClockwise = FALSE;

		// ShaderManifest から契約取得
		const ShaderDescriptor& vsDesc =
			SpriteShaderManifest::Get(SpriteShaderId::SpriteVS);
		const ShaderDescriptor& psDesc =
			SpriteShaderManifest::Get(SpriteShaderId::SpritePS);

		assert(vsDesc.stage == ShaderStage::Vertex);
		assert(psDesc.stage == ShaderStage::Pixel);
		assert(vsDesc.rootSignature == RootSignatureType::Sprite);
		assert(psDesc.rootSignature == RootSignatureType::Sprite);

		ComPtr<IDxcBlob> vertexShaderBlob = ShaderCompiler::CompileShader(
			vsDesc,
			dxCommon_->GetDXCCompilerManager());
		assert(vertexShaderBlob != nullptr);

		ComPtr<IDxcBlob> pixelShaderBlob = ShaderCompiler::CompileShader(
			psDesc,
			dxCommon_->GetDXCCompilerManager());
		assert(pixelShaderBlob != nullptr);

		// --- 背景用 ---
		{
			D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
			depthStencilDesc.DepthEnable = false;
			depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
			depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

			D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
			desc.pRootSignature = rootSignature_.Get();
			desc.InputLayout = inputLayoutDesc;
			desc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
			desc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
			desc.BlendState.RenderTarget[0] = blendDesc;
			desc.RasterizerState = rasterizerDesc;
			desc.NumRenderTargets = 1;
			desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			desc.SampleDesc.Count = 1;
			desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
			desc.DepthStencilState = depthStencilDesc;
			desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

			hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
				&desc,
				IID_PPV_ARGS(&graphicsPipelineState_Background_));
			assert(SUCCEEDED(hr));
		}

		// --- UI用 ---
		{
			D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
			depthStencilDesc.DepthEnable = false;
			depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
			depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

			D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
			desc.pRootSignature = rootSignature_.Get();
			desc.InputLayout = inputLayoutDesc;
			desc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
			desc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
			desc.BlendState.RenderTarget[0] = blendDesc;
			desc.RasterizerState = rasterizerDesc;
			desc.NumRenderTargets = 1;
			desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			desc.SampleDesc.Count = 1;
			desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
			desc.DepthStencilState = depthStencilDesc;
			desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

			hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
				&desc,
				IID_PPV_ARGS(&graphicsPipelineState_UI_));
			assert(SUCCEEDED(hr));
		}
	}

} // namespace Ken4lowEngine