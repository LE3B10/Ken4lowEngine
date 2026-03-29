#include "Object3DCommon.h"
#include "DirectXCommon.h"
#include "DebugCamera.h"
#include "Object3DShaderManifest.h"
#include <LogString.h>
#include <BlendStateFactory.h>
#include <ShaderCompiler.h>
#include <LightManager.h>

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///                 シングルトンインスタンス
	/// -------------------------------------------------------------
	Object3DCommon* Object3DCommon::GetInstance()
	{
		static Object3DCommon instance;
		return &instance;
	}

	/// -------------------------------------------------------------
	///                         初期化処理
	/// -------------------------------------------------------------
	void Object3DCommon::Initialize(DirectXCommon* dxCommon)
	{
		dxCommon_ = dxCommon;
		isDebugCamera_ = false;

		CreatePSO();
		CreateShadowPSO();

		LightManager::GetInstance()->Initialize(dxCommon_);

		graphicsPipelineState_->SetName(L"Object3DCommon_PSO");
		rootSignature_->SetName(L"Object3DCommon_RootSignature");

		if (shadowPipelineState_)
		{
			shadowPipelineState_->SetName(L"Object3DCommon_Shadow_PSO");
		}

		if (shadowRootSignature_)
		{
			shadowRootSignature_->SetName(L"Object3DCommon_Shadow_RootSignature");
		}
	}

	void Object3DCommon::Finalize()
	{
		LightManager::GetInstance()->Finalize();

		graphicsPipelineState_.Reset();
		rootSignature_.Reset();
		signatureBlob_.Reset();
		errorBlob_.Reset();

		shadowPipelineState_.Reset();
		shadowRootSignature_.Reset();
		shadowSignatureBlob_.Reset();
		shadowErrorBlob_.Reset();

		dxCommon_ = nullptr;
		defaultCamera_ = nullptr;

		isDebugCamera_ = false;
		inputLayoutDesc_ = {};
		viewProjectionMatrix_ = {};
		debugViewProjectionMatrix_ = {};
		activeCameraPosition_ = { 0,0,0 };
	}

	/// -------------------------------------------------------------
	///                         更新処理
	/// -------------------------------------------------------------
	void Object3DCommon::Update()
	{
		if (isDebugCamera_)
		{
#ifdef _DEBUG
			debugViewProjectionMatrix_ = DebugCamera::GetInstance()->GetViewProjectionMatrix();
			defaultCamera_->SetViewProjectionMatrix(debugViewProjectionMatrix_);
			activeCameraPosition_ = DebugCamera::GetInstance()->GetTranslate();
#endif // _DEBUG
		}
		else
		{
			viewProjectionMatrix_ = Matrix4x4::Multiply(
				defaultCamera_->GetViewMatrix(),
				defaultCamera_->GetProjectionMatrix());
			defaultCamera_->SetViewProjectionMatrix(viewProjectionMatrix_);
			activeCameraPosition_ = defaultCamera_->GetTranslate();
		}
	}

	void Object3DCommon::DrawImGui()
	{}

	/// -------------------------------------------------------------
	///                     共通描画処理設定
	/// -------------------------------------------------------------
	void Object3DCommon::SetRenderSetting()
	{
		auto commandList = dxCommon_->GetCommandManager()->GetCommandList();
		commandList->SetGraphicsRootSignature(rootSignature_.Get());
		commandList->SetPipelineState(graphicsPipelineState_.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		LightManager::GetInstance()->BindPunctualLights(5, 6);
	}

	void Object3DCommon::SetShadowMapRenderSetting()
	{
		auto commandList = dxCommon_->GetCommandManager()->GetCommandList();
		commandList->SetGraphicsRootSignature(shadowRootSignature_.Get());
		commandList->SetPipelineState(shadowPipelineState_.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	void Object3DCommon::CreateRootSignature()
	{
		D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
		descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		D3D12_STATIC_SAMPLER_DESC staticSamplers[2] = {};

		// s0 : 通常テクスチャ用
		staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		staticSamplers[0].MaxLOD = 0.0f;
		staticSamplers[0].ShaderRegister = 0;
		staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		// s1 : Shadow Map 用
		staticSamplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		staticSamplers[1].MaxLOD = 0.0f;
		staticSamplers[1].ShaderRegister = 1;
		staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		descriptionRootSignature.pStaticSamplers = staticSamplers;
		descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

		D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
		descriptorRange[0].BaseShaderRegister = 0;
		descriptorRange[0].NumDescriptors = 1;
		descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE cubeMapRange{};
		cubeMapRange.BaseShaderRegister = 1;
		cubeMapRange.NumDescriptors = 1;
		cubeMapRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		cubeMapRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE lightArrayRange{};
		lightArrayRange.BaseShaderRegister = 2;
		lightArrayRange.NumDescriptors = 1;
		lightArrayRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		lightArrayRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE dissolveMaskRange{};
		dissolveMaskRange.BaseShaderRegister = 3;
		dissolveMaskRange.NumDescriptors = 1;
		dissolveMaskRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		dissolveMaskRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE shadowMapRange{};
		shadowMapRange.BaseShaderRegister = 4;
		shadowMapRange.NumDescriptors = 1;
		shadowMapRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		shadowMapRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_ROOT_PARAMETER rootParameters[11] = {};

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

		rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[3].Descriptor.ShaderRegister = 1;

		rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[4].DescriptorTable.pDescriptorRanges = &cubeMapRange;
		rootParameters[4].DescriptorTable.NumDescriptorRanges = 1;

		rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[5].Descriptor.ShaderRegister = 2;

		rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[6].DescriptorTable.pDescriptorRanges = &lightArrayRange;
		rootParameters[6].DescriptorTable.NumDescriptorRanges = 1;

		rootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[7].Descriptor.ShaderRegister = 3;

		rootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[8].DescriptorTable.pDescriptorRanges = &dissolveMaskRange;
		rootParameters[8].DescriptorTable.NumDescriptorRanges = 1;

		rootParameters[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[9].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParameters[9].Descriptor.ShaderRegister = 4;

		rootParameters[10].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[10].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[10].DescriptorTable.pDescriptorRanges = &shadowMapRange;
		rootParameters[10].DescriptorTable.NumDescriptorRanges = 1;

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

	void Object3DCommon::CreatePSO()
	{
		HRESULT hr{};

		CreateRootSignature();

		D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
		inputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
		inputElementDescs[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
		inputElementDescs[2] = { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

		inputLayoutDesc_.pInputElementDescs = inputElementDescs;
		inputLayoutDesc_.NumElements = _countof(inputElementDescs);

		const D3D12_RENDER_TARGET_BLEND_DESC& blendDesc =
			BlendStateFactory::GetInstance()->GetBlendDesc(blendMode_);

		D3D12_RASTERIZER_DESC rasterizerDesc{};
		rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
		rasterizerDesc.FrontCounterClockwise = FALSE;

		const ShaderDescriptor& vsDesc =
			Object3DShaderManifest::Get(Object3DShaderId::Object3DVS);
		const ShaderDescriptor& psDesc =
			Object3DShaderManifest::Get(Object3DShaderId::Object3DPS);

		assert(vsDesc.stage == ShaderStage::Vertex);
		assert(psDesc.stage == ShaderStage::Pixel);
		assert(vsDesc.rootSignature == RootSignatureType::Object3D);
		assert(psDesc.rootSignature == RootSignatureType::Object3D);

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
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
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

	void Object3DCommon::CreateShadowRootSignature()
	{
		D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
		descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		D3D12_ROOT_PARAMETER rootParameters[1] = {};

		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParameters[0].Descriptor.ShaderRegister = 0;

		descriptionRootSignature.pParameters = rootParameters;
		descriptionRootSignature.NumParameters = _countof(rootParameters);
		descriptionRootSignature.pStaticSamplers = nullptr;
		descriptionRootSignature.NumStaticSamplers = 0;

		HRESULT hr = D3D12SerializeRootSignature(
			&descriptionRootSignature,
			D3D_ROOT_SIGNATURE_VERSION_1,
			&shadowSignatureBlob_,
			&shadowErrorBlob_);
		if (FAILED(hr))
		{
			if (shadowErrorBlob_)
			{
				Log(reinterpret_cast<char*>(shadowErrorBlob_->GetBufferPointer()));
			}
			assert(false);
		}

		hr = dxCommon_->GetDevice()->CreateRootSignature(
			0,
			shadowSignatureBlob_->GetBufferPointer(),
			shadowSignatureBlob_->GetBufferSize(),
			IID_PPV_ARGS(&shadowRootSignature_));
		assert(SUCCEEDED(hr));
	}

	void Object3DCommon::CreateShadowPSO()
	{
		HRESULT hr{};

		CreateShadowRootSignature();

		D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
		inputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
		inputElementDescs[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
		inputElementDescs[2] = { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

		D3D12_INPUT_LAYOUT_DESC shadowInputLayoutDesc{};
		shadowInputLayoutDesc.pInputElementDescs = inputElementDescs;
		shadowInputLayoutDesc.NumElements = _countof(inputElementDescs);

		D3D12_RASTERIZER_DESC rasterizerDesc{};
		rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
		rasterizerDesc.FrontCounterClockwise = FALSE;
		rasterizerDesc.DepthBias = 1000;
		rasterizerDesc.SlopeScaledDepthBias = 1.0f;
		rasterizerDesc.DepthBiasClamp = 0.0f;

		const ShaderDescriptor& shadowVsDesc =
			Object3DShaderManifest::Get(Object3DShaderId::ShadowMapVS);

		assert(shadowVsDesc.stage == ShaderStage::Vertex);
		assert(shadowVsDesc.rootSignature == RootSignatureType::ShadowMap);

		ComPtr<IDxcBlob> vertexShaderBlob = ShaderCompiler::CompileShader(
			shadowVsDesc,
			dxCommon_->GetDXCCompilerManager());
		assert(vertexShaderBlob != nullptr);

		D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
		depthStencilDesc.DepthEnable = TRUE;
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		depthStencilDesc.StencilEnable = FALSE;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.pRootSignature = shadowRootSignature_.Get();
		psoDesc.InputLayout = shadowInputLayoutDesc;
		psoDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
		psoDesc.PS = { nullptr, 0 };
		psoDesc.RasterizerState = rasterizerDesc;
		psoDesc.DepthStencilState = depthStencilDesc;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 0;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

		hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
			&psoDesc,
			IID_PPV_ARGS(&shadowPipelineState_));
		assert(SUCCEEDED(hr));
	}

} // namespace Ken4lowEngine