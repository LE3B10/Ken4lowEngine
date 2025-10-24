#include "PostEffectPipelineBuilder.h"
#include "DirectXCommon.h"
#include <ShaderCompiler.h>

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
	auto vs = ShaderCompiler::CompileShader(L"Resources/Shaders/PostEffect/FullScreen.VS.hlsl", L"vs_6_0", dxCommon_->GetDXCCompilerManager());
	auto ps = ShaderCompiler::CompileShader(pixelShaderPath, L"ps_6_0", dxCommon_->GetDXCCompilerManager());

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
	HRESULT hr = S_FALSE;
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso));
	assert(SUCCEEDED(hr));
	return pso;
}


/// -------------------------------------------------------------
///				コンピュートパイプラインの生成
/// -------------------------------------------------------------
ComPtr<ID3D12RootSignature> PostEffectPipelineBuilder::CreateComputeRootSignature()
{
	// デスクリプタレンジ設定
	D3D12_DESCRIPTOR_RANGE srvRange0{};
	srvRange0.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange0.NumDescriptors = 1; // 1つのSRV
	srvRange0.BaseShaderRegister = 0; // t0
	srvRange0.RegisterSpace = 0; // スペース0
	srvRange0.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE srvRange1{};
	srvRange1.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange1.NumDescriptors = 1; // 1つのSRV
	srvRange1.BaseShaderRegister = 1; // t1
	srvRange1.RegisterSpace = 0; // スペース0
	srvRange1.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE uavRange{};
	uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange.NumDescriptors = 1; // 1つのUAV
	uavRange.BaseShaderRegister = 0; // u0
	uavRange.RegisterSpace = 0; // スペース0
	uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ルートパラメータ設定
	std::vector<D3D12_ROOT_PARAMETER> rootParams(4);

	// SRV (t0) 入力テクスチャ
	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[0].DescriptorTable.pDescriptorRanges = &srvRange0;
	rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// UAV (u0) 出力テクスチャ
	rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[1].DescriptorTable.pDescriptorRanges = &uavRange;
	rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// 定数バッファ (b0)
	rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[2].Descriptor.ShaderRegister = 0; // b0
	rootParams[2].Descriptor.RegisterSpace = 0; // スペース0
	rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// SRV (t1) 追加のテクスチャ（例: マスクや深度）
	rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[3].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[3].DescriptorTable.pDescriptorRanges = &srvRange1;
	rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// サンプラー（s0）
	D3D12_STATIC_SAMPLER_DESC samplerDesc[2]{};
	samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc[0].ShaderRegister = 0; // s0
	samplerDesc[0].RegisterSpace = 0;
	samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// s1: ポイント
	samplerDesc[1] = samplerDesc[0];
	samplerDesc[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc[1].ShaderRegister = 1;

	// ルートシグネチャの設定
	D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
	rootSigDesc.NumParameters = static_cast<UINT>(rootParams.size());
	rootSigDesc.pParameters = rootParams.data();
	rootSigDesc.NumStaticSamplers = 2; // サンプラーは1つ
	rootSigDesc.pStaticSamplers = samplerDesc;
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE; // コンピュートシェーダーでは特にフラグは不要

	ComPtr<ID3DBlob> sigBlob, errorBlog;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errorBlog);
	assert(SUCCEEDED(hr) && "RootSignature Serialize Failed");

	// ルートシグネチャの生成
	ComPtr<ID3D12RootSignature> computeRootSignature;
	hr = dxCommon_->GetDevice()->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&computeRootSignature));

	return computeRootSignature;
}


/// -------------------------------------------------------------
///				コンピュートパイプラインの生成
/// -------------------------------------------------------------
ComPtr<ID3D12PipelineState> PostEffectPipelineBuilder::CreateComputePipeline(const std::wstring& csPath, ID3D12RootSignature* rootSignature)
{
	// シェーダー
	auto cs = ShaderCompiler::CompileShader(csPath, L"cs_6_6", dxCommon_->GetDXCCompilerManager());
	assert(cs != nullptr);

	// 各種設定
	D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature;
	desc.CS = { cs->GetBufferPointer(), cs->GetBufferSize() };
	ComPtr<ID3D12PipelineState> pso;
	HRESULT hr = dxCommon_->GetDevice()->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pso));
	assert(SUCCEEDED(hr) && "CreateComputePipelineState Failed");
	// 成功したらパイプラインステートを返す
	if (SUCCEEDED(hr))
	{
		return pso;
	}
	// 失敗した場合は空のポインタを返す
	return ComPtr<ID3D12PipelineState>();
}


/// -------------------------------------------------------------
///					コピー用パイプラインの構築
/// -------------------------------------------------------------
void PostEffectPipelineBuilder::BuildCopyPipeline()
{
	// ① RootSig を生成（深度不要）
	copyRootSignature_ = CreateRootSignature();

	// ② FSQ 用ピクセルシェーダ（色をそのまま出力）
	const std::wstring kCopyPS = L"Resources/Shaders/PostEffect/NormalEffect.PS.hlsl";
	copyPipelineState_ = CreateGraphicsPipeline(kCopyPS, copyRootSignature_.Get(), false);
}
