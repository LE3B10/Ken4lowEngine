#include "PipelineStateManager.h"


/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void PipelineStateManager::Initialize(
	ID3D12Device* device,
	RootSignatureManager& rootSignatureManager,
	BlendStateManager& blenderStateManager,
	RasterizerStateManager& rasterizerStateManager,
	InputLayoutManager& inputLayoutManager,
	ID3DBlob* vertexShaderBlob,
	ID3DBlob* pixelShaderBlob
){
	HRESULT hr{};

	//Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	//書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	//比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	graphicsPipelineStateDesc.pRootSignature = rootSignatureManager.GetRootSignature();							// RootSgnature
	graphicsPipelineStateDesc.InputLayout = inputLayoutManager.GetInputLayoutDesc();							// InputLayout
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),vertexShaderBlob->GetBufferSize() };	// VertexDhader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),pixelShaderBlob->GetBufferSize() };	// PixelShader
	graphicsPipelineStateDesc.BlendState.RenderTarget[0] = blenderStateManager.GetBlendDesc();					// BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterizerStateManager.GetRasterizerDesc();						// RasterizeerState

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
	hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));
}


/// -------------------------------------------------------------
///							ゲッター
/// -------------------------------------------------------------
ID3D12PipelineState* PipelineStateManager::GetPipelineState() const
{
	return graphicsPipelineState.Get();
}
