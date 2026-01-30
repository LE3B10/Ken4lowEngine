#include "GpuParticlePipeline.h"
#include <DirectXCommon.h>
#include <LogString.h>
#include <SRVManager.h>
#include "BlendStateFactory.h"
#include "ShaderCompiler.h"


/// -------------------------------------------------------------
///				　　　	初期化処理
/// -------------------------------------------------------------
void GpuParticlePipeline::Initialize()
{
	// 引数でDirectXCommonのポインタを受け取ってメンバ変数に記録する
	dxCommon_ = DirectXCommon::GetInstance();

	// ルートシグネチャの生成
	CreateRootSignature();

	// PSOを生成
	CreatePSO();

	// コンピュートシェーダー用のルートシグネチャの生成
	CreateComputeRootSignature();

	// コンピュートシェーダー用のパイプラインステートオブジェクトの生成
	CreateComputePSO();

	// エミット用コンピュートシェーダー用のパイプラインステートオブジェクトの生成
	CreateEmitComputePSO();

	// パーティクル更新用コンピュートシェーダーのコンパイル
	CreateUpdateComputePSO();

	// オブジェクト名の設定
	rootSignature_->SetName(L"GpuParticlePipeline_RootSignature");
	pipelineState_->SetName(L"GpuParticlePipeline_Gfx_PSO");
	computePipelineState_->SetName(L"GpuParticlePipeline_Compute_PSO");
	computeRootSignature_->SetName(L"GpuParticlePipeline_Compute_RootSignature");
	emitComputePipelineState_->SetName(L"GpuParticlePipeline_Emit_Compute_PSO");
	updateComputePipelineState_->SetName(L"GpuParticlePipeline_Update_Compute_PSO");
}

/// -------------------------------------------------------------
///			　パイプラインシグネイチャーの生成処理
/// -------------------------------------------------------------
void GpuParticlePipeline::CreatePSO()
{
	HRESULT hr{};

	// 入力レイアウトの設定
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElementDescs[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,		0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElementDescs[2] = { "NORMAL"  , 0, DXGI_FORMAT_R32G32B32_FLOAT,	0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

	// InputLayoutの設定
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	//BlendStateの設定
	const D3D12_RENDER_TARGET_BLEND_DESC blendDesc = BlendStateFactory::GetInstance()->GetBlendDesc(blendMode_); // アルファブレンドを使用

	//RasterizerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;	 // 裏面（時計回り）を表示しない
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID; // 三角形の中を塗りつぶす

	//Shaderをコンパイルする
	ComPtr <IDxcBlob> vertexShaderBlob = ShaderCompiler::CompileShader(L"Resources/Shaders/GpuParticle/GpuParticle.VS.hlsl", L"vs_6_0", dxCommon_->GetDXCCompilerManager());
	assert(vertexShaderBlob != nullptr);

	//Pixelをコンパイルする
	ComPtr <IDxcBlob> pixelShaderBlob = ShaderCompiler::CompileShader(L"Resources/Shaders/GpuParticle/GpuParticle.PS.hlsl", L"ps_6_0", dxCommon_->GetDXCCompilerManager());
	assert(pixelShaderBlob != nullptr);

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	//Depthの機能を無効化する
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // Depthを描くのをやめる
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // 比較関数はLessEqual。つまり、近ければ描画される

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};												// パイプラインステートディスクリプタの初期化
	graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();											// RootSignature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;													// InputLayout
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),vertexShaderBlob->GetBufferSize() };	// VertexDhader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),pixelShaderBlob->GetBufferSize() };	// PixelShader
	graphicsPipelineStateDesc.BlendState.RenderTarget[0] = blendDesc;											// BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;													// RasterizerState

	//レンダーターゲットの設定
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	//利用するトポロジー（形態）のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	// サンプルマスクとサンプル記述子の設定
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// DepthStencilステートの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// パイプラインステートオブジェクトの生成
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(hr));
}

/// -------------------------------------------------------------
///					ルートシグネチャーの生成処理
/// -------------------------------------------------------------
void GpuParticlePipeline::CreateRootSignature()
{
	HRESULT hr{};

	//RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// DescriptorRangeの設定 : Particle用SRVテーブル
	D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[1] = {};
	descriptorRangeForInstancing[0].BaseShaderRegister = 0; // 0から始まる
	descriptorRangeForInstancing[0].NumDescriptors = 1;		// 数は1つ
	descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ルートパラメータの設定
	D3D12_ROOT_PARAMETER rootParameters[4] = {};

	// b0: シミュレーション定数用CBV
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;								// CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;								// 全てのシェーダで使う
	rootParameters[0].Descriptor.ShaderRegister = 0;												// レジスタ番号０とバインド

	/// ---------- VertexShader用SRVテーブルの設定 ---------- ///

	// t0～tn : Particle用SRVテーブル
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;					// DescriptorTableを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;							// VertexShaderで使う
	rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing;				// Tableの中身の配列を指定
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing); // Tableで利用する

	/// ---------- PixelShader用SRVテーブルの設定 ---------- ///

	// b0 : マテリアル用
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;								// CBVを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;								// PixelShaderで使う
	rootParameters[2].Descriptor.ShaderRegister = 1;												// レジスタ番号１とバインド

	// t0～tn : パーティクル用テクスチャSRVテーブル
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;					// DescriptorTableを使う
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;								// PixelShaderで使う
	rootParameters[3].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing;				// Tableの中身の配列を指定
	rootParameters[3].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing); // Tableで利用する

	descriptionRootSignature.pParameters = rootParameters;											//ルートパラメータ配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters);								//配列の長さ

	// スタティックサンプラーの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;										//バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;									//0～1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;									//比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;													//ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0;															//レジスタ番号０を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;								//PixelShaderで使う
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	ComPtr <ID3DBlob> signatureBlob = nullptr;
	ComPtr <ID3DBlob> errorBlob = nullptr;

	// ルートシグネチャのシリアライズ
	hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr))
	{
		Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	// ルートシグネチャの生成
	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));
}

/// -------------------------------------------------------------
///	　コンピュートシェーダー用のルートシグネチャーの生成処理
/// -------------------------------------------------------------
void GpuParticlePipeline::CreateComputeRootSignature()
{
	// CBV(b0): シミュレーション定数（Δt, emit数 等）
	D3D12_ROOT_PARAMETER params[4] = {};

	// CBVの設定
	params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[0].Descriptor.ShaderRegister = 0;
	params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// UAVテーブル(u0〜uN): 書き込み用（NextState, Alive/Dead, Counter等）
	D3D12_DESCRIPTOR_RANGE uavRange{};
	uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange.NumDescriptors = 5; // 必要数に合わせて調整
	uavRange.BaseShaderRegister = 0; // u0から始まる
	uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// UAVテーブルの設定
	params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[1].DescriptorTable.NumDescriptorRanges = 1;
	params[1].DescriptorTable.pDescriptorRanges = &uavRange;
	params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// b1 CBV: 射出用定数（エミット位置、速度 等）
	params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[2].Descriptor.ShaderRegister = 1; // b1
	params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// b2 CBV : 時間計測用
	params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[3].Descriptor.ShaderRegister = 2; // b2
	params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// ルートシグネチャの記述
	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	desc.pParameters = params;
	desc.NumParameters = _countof(params);

	ComPtr<ID3DBlob> sig, err;
	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);
	if (FAILED(hr)) { Log(err ? (char*)err->GetBufferPointer() : "Compute RS serialize failed"); assert(false); }

	hr = dxCommon_->GetDevice()->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&computeRootSignature_));
	assert(SUCCEEDED(hr));
}

/// -------------------------------------------------------------
///	コンピュートシェーダー用のパイプラインステートオブジェクトの生成
/// -------------------------------------------------------------
void GpuParticlePipeline::CreateComputePSO()
{
	ComPtr<IDxcBlob> computeShader = ShaderCompiler::CompileShader(L"Resources/Shaders/GpuParticle/GpuParticle.CS.hlsl", L"cs_6_0", dxCommon_->GetDXCCompilerManager());
	assert(computeShader != nullptr);

	// パイプラインステートオブジェクトの設定
	D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = computeRootSignature_.Get();
	desc.CS = { computeShader->GetBufferPointer(), computeShader->GetBufferSize() };

	HRESULT hr = S_FALSE;
	hr = dxCommon_->GetDevice()->CreateComputePipelineState(&desc, IID_PPV_ARGS(&computePipelineState_));
	assert(SUCCEEDED(hr) && "CreateComputePipelineState Failed");
}

/// -------------------------------------------------------------
/// エミット用コンピュートシェーダー用のパイプラインステートオブジェクトの生成
/// -------------------------------------------------------------
void GpuParticlePipeline::CreateEmitComputePSO()
{
	ComPtr<IDxcBlob> emitShader = ShaderCompiler::CompileShader(L"Resources/Shaders/GpuParticle/GpuParticleEmit.CS.hlsl", L"cs_6_0", dxCommon_->GetDXCCompilerManager());
	assert(emitShader != nullptr);

	// パイプラインステートオブジェクトの生成
	D3D12_COMPUTE_PIPELINE_STATE_DESC emitDesc{};
	emitDesc.pRootSignature = computeRootSignature_.Get();
	emitDesc.CS = { emitShader->GetBufferPointer(), emitShader->GetBufferSize() };

	HRESULT hr = S_FALSE;
	hr = dxCommon_->GetDevice()->CreateComputePipelineState(&emitDesc, IID_PPV_ARGS(&emitComputePipelineState_));
	assert(SUCCEEDED(hr) && "CreateEmitComputePipelineState Failed");
}

/// -------------------------------------------------------------
///　	パーティクル更新用コンピュートシェーダーのコンパイル
/// -------------------------------------------------------------
void GpuParticlePipeline::CreateUpdateComputePSO()
{
	ComPtr<IDxcBlob> updateShader = ShaderCompiler::CompileShader(L"Resources/Shaders/GpuParticle/GpuParticleUpdate.CS.hlsl", L"cs_6_0", dxCommon_->GetDXCCompilerManager());
	assert(updateShader != nullptr);

	// パイプラインステートオブジェクトの生成
	D3D12_COMPUTE_PIPELINE_STATE_DESC updateDesc{};
	updateDesc.pRootSignature = computeRootSignature_.Get();
	updateDesc.CS = { updateShader->GetBufferPointer(), updateShader->GetBufferSize() };

	HRESULT hr = S_FALSE;
	hr = dxCommon_->GetDevice()->CreateComputePipelineState(&updateDesc, IID_PPV_ARGS(&updateComputePipelineState_));
	assert(SUCCEEDED(hr) && "CreateUpdateComputePipelineState Failed");
}
