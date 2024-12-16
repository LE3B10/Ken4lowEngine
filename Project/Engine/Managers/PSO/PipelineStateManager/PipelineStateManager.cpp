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
void PipelineStateManager::Initialize(DirectXCommon* dxCommon, PipelineType pipelineType, BlendMode blendMode)
{
	pipelineType_ = pipelineType;

	/// ---------- RootSignatureManagerの初期化 ---------- ///
	rootSignatureManager = std::make_unique<RootSignatureManager>();
	rootSignatureManager->CreateRootSignature(dxCommon, pipelineType_);

	/// ---------- InputLayoutManagerの初期化 ---------- ///
	inputLayoutManager = std::make_unique<InputLayoutManager>();
	inputLayoutManager->Initialize(pipelineType_);

	/// ---------- BlendStateManagerの初期化 ---------- ///
	blendStateManager = std::make_unique<BlendStateManager>();
	blendStateManager->CreateBlend(blendMode);

	/// ---------- RasterinzerStateManagerの初期化 ---------- ///
	rasterizerStateManager = std::make_unique<RasterizerStateManager>();
	rasterizerStateManager->Initialize();

	/// ---------- ShaderManagerの初期化 ---------- ///
	shaderManager = std::make_unique<ShaderManager>();
	shaderManager->ShaderCompile(dxCommon, pipelineType_);

	/// ---------- DepthStencilの初期化 ---------- ///
	depthStencil = std::make_unique<DX12DepthStencil>();
	depthStencil->Create(true);

	/// ---------- PSOを生成 ---------- ///
	CreatePipelineStateObject(dxCommon, rootSignatureManager->GetRootSignature(pipelineType_), blendStateManager->GetBlendDesc(), rasterizerStateManager->GetRasterizerDesc(),
		inputLayoutManager->GetInputLayoutDesc(pipelineType_), depthStencil->GetDepthStencilDesc(), shaderManager->GetVertexShaderBlob(pipelineType_), shaderManager->GetPixelShaderBlob(pipelineType_)
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
///						パイプラインの設定
/// -------------------------------------------------------------
void PipelineStateManager::SetGraphicsPipeline(ID3D12GraphicsCommandList* commandList)
{
	/*-----シーン（モデル）の描画設定と描画-----*/
	// ルートシグネチャとパイプラインステートの設定
	commandList->SetGraphicsRootSignature(rootSignatureManager->GetRootSignature(pipelineType_));
	commandList->SetPipelineState(graphicsPipelineState_.Get());
}
