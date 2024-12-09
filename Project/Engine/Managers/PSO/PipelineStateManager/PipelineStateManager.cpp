#include "PipelineStateManager.h"
#include "DirectXCommon.h"


/// -------------------------------------------------------------
///							デストラクタ
/// -------------------------------------------------------------
PipelineStateManager::~PipelineStateManager()
{
	rootSignatureManager.reset();
	inputLayoutManager.reset();
	blendStateManager.reset();
	rasterizerStateManager.reset();
	shaderManager.reset();
	depthStencil.reset();
}


/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void PipelineStateManager::Initialize(DirectXCommon* dxCommon)
{
	/// ---------- RootSignatureManagerの初期化 ---------- ///
	rootSignatureManager = std::make_unique<RootSignatureManager>();
	rootSignatureManager->CreateRootSignature(dxCommon);

	/// ---------- InputLayoutManagerの初期化 ---------- ///
	inputLayoutManager = std::make_unique<InputLayoutManager>();
	inputLayoutManager->Initialize();

	/// ---------- BlendStateManagerの初期化 ---------- ///
	blendStateManager = std::make_unique<BlendStateManager>();
	blendStateManager->CreateBlend(BlendMode::kBlendModeNone);

	/// ---------- RasterinzerStateManagerの初期化 ---------- ///
	rasterizerStateManager = std::make_unique<RasterizerStateManager>();
	rasterizerStateManager->Initialize();

	/// ---------- ShaderManagerの初期化 ---------- ///
	shaderManager = std::make_unique<ShaderManager>();
	shaderManager->ShaderCompileObject3D(dxCommon);

	/// ---------- DepthStencilの初期化 ---------- ///
	depthStencil = std::make_unique<DX12DepthStencil>();
	depthStencil->Create(true);

	/// ---------- PSOを生成 ---------- ///
	CreatePipelineStateObject(dxCommon, rootSignatureManager->GetRootSignature(), blendStateManager->GetBlendDesc(), rasterizerStateManager->GetRasterizerDesc(),
		inputLayoutManager->GetInputLayoutDesc(), depthStencil->GetDepthStencilDesc(), shaderManager->GetVertexShaderBlob(), shaderManager->GetPixelShaderBlob()
	);

	InitializeParticle(dxCommon);
}

void PipelineStateManager::InitializeParticle(DirectXCommon* dxCommon)
{
	/// ---------- RootSignatureManagerの初期化 ---------- ///
	rootSignatureManager = std::make_unique<RootSignatureManager>();
	rootSignatureManager->CreateRootSignatureForParticle(dxCommon);

	/// ---------- InputLayoutManagerの初期化 ---------- ///
	inputLayoutManager = std::make_unique<InputLayoutManager>();
	inputLayoutManager->InitializeParticle();

	/// ---------- BlendStateManagerの初期化 ---------- ///
	blendStateManager = std::make_unique<BlendStateManager>();
	blendStateManager->CreateBlendParticle(BlendMode::kBlendModeAdd);

	/// ---------- RasterinzerStateManagerの初期化 ---------- ///
	rasterizerStateManager = std::make_unique<RasterizerStateManager>();
	rasterizerStateManager->InitializeParticle();

	/// ---------- ShaderManagerの初期化 ---------- ///
	shaderManager = std::make_unique<ShaderManager>();
	shaderManager->ShaderCompileParticle(dxCommon);

	/// ---------- DepthStencilの初期化 ---------- ///
	depthStencil = std::make_unique<DX12DepthStencil>();
	depthStencil->CreateParticle(true);

	/// ---------- PSOを生成 ---------- ///
	CreatePipelineStateObject(dxCommon, rootSignatureManager->GetRootSignature(), blendStateManager->GetBlendDesc(), rasterizerStateManager->GetRasterizerDesc(),
		inputLayoutManager->GetInputLayoutDesc(), depthStencil->GetDepthStencilDesc(), shaderManager->GetVertexShaderBlob(), shaderManager->GetPixelShaderBlob()
	);
}


/// -------------------------------------------------------------
///						PSOを生成する処理
/// -------------------------------------------------------------
void PipelineStateManager::CreatePipelineStateObject(
	DirectXCommon* dxCommon, ID3D12RootSignature* rootSignature,
	D3D12_RENDER_TARGET_BLEND_DESC blendDesc, D3D12_RASTERIZER_DESC rasterizerDesc,
	D3D12_INPUT_LAYOUT_DESC inputLayout, D3D12_DEPTH_STENCIL_DESC depthStencilDesc,
	IDxcBlob* vertexShaderBlob, IDxcBlob* pixelShaderBlob)
{
	HRESULT hr{};

	graphicsPipelineStateDesc.pRootSignature = rootSignature;													// RootSgnature
	graphicsPipelineStateDesc.InputLayout = inputLayout;														// InputLayout
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };	// VertexDhader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };	// PixelShader
	graphicsPipelineStateDesc.BlendState.RenderTarget[0] = blendDesc;											// BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;													// RasterizeerState

	//レンダーターゲットの設定
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	//利用するトポロジー（形態）のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;					// プリミティブトポロジーの設定

	// サンプルマスクとサンプル記述子の設定
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// DepthStencilステートの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// パイプラインステートオブジェクトの生成
	hr = dxCommon->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState_));
	assert(SUCCEEDED(hr));
}


/// -------------------------------------------------------------
///							ゲッター
/// -------------------------------------------------------------
ID3D12PipelineState* PipelineStateManager::GetPipelineState() const
{
	return graphicsPipelineState_.Get();
}


/// -------------------------------------------------------------
///						パイプラインの設定
/// -------------------------------------------------------------
void PipelineStateManager::SetGraphicsPipeline(ID3D12GraphicsCommandList* commandList)
{
	/*-----シーン（モデル）の描画設定と描画-----*/
	// ルートシグネチャとパイプラインステートの設定
	commandList->SetGraphicsRootSignature(rootSignatureManager->GetRootSignature());
	commandList->SetPipelineState(graphicsPipelineState_.Get());
}
