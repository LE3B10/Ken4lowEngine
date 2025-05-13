#include "PostEffectPipelineBuilder.h"
#include "DirectXCommon.h"
#include <ShaderManager.h>

#include <cassert>

using namespace Microsoft::WRL;


/// -------------------------------------------------------------
///						　初期化処理
/// -------------------------------------------------------------
void PostEffectPipelineBuilder::Initialize(DirectXCommon* dxCommon)
{
	dxCommon_ = dxCommon;
}


/// -------------------------------------------------------------
///					ルートシグネチャの生成
/// -------------------------------------------------------------
ComPtr<ID3D12RootSignature> PostEffectPipelineBuilder::CreateRootSignature()
{
	// ---- サンプラー設定 ----
	D3D12_STATIC_SAMPLER_DESC samplers[2] = {};

	// s0: バイリニア
	samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	samplers[0].ShaderRegister = 0;
	samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// s1: ポイント
	samplers[1] = samplers[0];
	samplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	samplers[1].ShaderRegister = 1;

	// ---- ディスクリプタレンジ設定 ----
	std::vector<D3D12_DESCRIPTOR_RANGE> descriptorRanges;

	for (int i = 0; i <= 2; ++i) { // t0～t2 に対応（gTexture, gMask, gDepth）
		D3D12_DESCRIPTOR_RANGE range{};
		range.BaseShaderRegister = i;
		range.NumDescriptors = 1;
		range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		descriptorRanges.push_back(range);
	}

	// ---- ルートパラメータ設定 ----
	std::vector<D3D12_ROOT_PARAMETER> rootParams;

	// t0 (gTexture)
	D3D12_ROOT_PARAMETER texParam{};
	texParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	texParam.DescriptorTable.pDescriptorRanges = &descriptorRanges[0];
	texParam.DescriptorTable.NumDescriptorRanges = 1;
	texParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParams.push_back(texParam);

	// b0, b1 (定数バッファ)
	for (int i = 0; i < 2; ++i) {
		D3D12_ROOT_PARAMETER cbv{};
		cbv.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		cbv.Descriptor.ShaderRegister = i;
		cbv.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParams.push_back(cbv);
	}

	// t1 (gMask)
	D3D12_ROOT_PARAMETER maskParam{};
	maskParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	maskParam.DescriptorTable.pDescriptorRanges = &descriptorRanges[1];
	maskParam.DescriptorTable.NumDescriptorRanges = 1;
	maskParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParams.push_back(maskParam);

	// t2 (gDepth)
	D3D12_ROOT_PARAMETER depthParam{};
	depthParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	depthParam.DescriptorTable.pDescriptorRanges = &descriptorRanges[2];
	depthParam.DescriptorTable.NumDescriptorRanges = 1;
	depthParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParams.push_back(depthParam);

	// ---- ルートシグネチャ作成 ----
	D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSigDesc.pParameters = rootParams.data();
	rootSigDesc.NumParameters = static_cast<UINT>(rootParams.size());
	rootSigDesc.pStaticSamplers = samplers;
	rootSigDesc.NumStaticSamplers = _countof(samplers);

	ComPtr<ID3DBlob> sigBlob;
	ComPtr<ID3DBlob> errBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
	assert(SUCCEEDED(hr) && "RootSignature Serialize Failed");

	ComPtr<ID3D12RootSignature> rootSig;
	hr = dxCommon_->GetDevice()->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&rootSig));
	assert(SUCCEEDED(hr) && "CreateRootSignature Failed");

	return rootSig;
}


/// -------------------------------------------------------------
///				グラフィックスパイプラインの生成
/// -------------------------------------------------------------
ComPtr<ID3D12PipelineState> PostEffectPipelineBuilder::CreateGraphicsPipeline(const std::wstring& pixelShaderPath, ID3D12RootSignature* rootSignature, bool enableDepth)
{
	// シェーダー
	auto vs = ShaderManager::CompileShader(L"Resources/Shaders/PostEffect/FullScreen.VS.hlsl", L"vs_6_0", dxCommon_->GetDXCCompilerManager());
	auto ps = ShaderManager::CompileShader(pixelShaderPath, L"ps_6_0", dxCommon_->GetDXCCompilerManager());

	// 各種設定
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature;
	desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
	desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	desc.SampleDesc.Count = 1;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// ラスタライザとブレンド
	desc.RasterizerState = { D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_NONE, FALSE };
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// 深度
	if (enableDepth)
	{
		D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		depthStencilDesc.StencilEnable = false;

		desc.DepthStencilState = depthStencilDesc;
		desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	}
	else
	{
		desc.DepthStencilState = {}; // 明示的に初期化
		desc.DepthStencilState.DepthEnable = false;
		desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS; // 無難な比較
		desc.DepthStencilState.StencilEnable = false;
		desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	}

	ComPtr<ID3D12PipelineState> pso;
	HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso));
	assert(SUCCEEDED(hr));
	return pso;
}
