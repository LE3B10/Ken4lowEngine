#include "PipelineStateManager.h"

#include "DirectXCommon.h"

void PipelineStateManager::CreatePipelineStateObject(DirectXCommon* dxCommon)
{
	HRESULT hr{};

	#pragma region グラフィックスパイプラインステートオブジェクト（Pipeline State Object, PSO）を生成する
	graphicsPipelineStateDesc.pRootSignature = rootSignature->GetRootSignature();								// RootSgnature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc->GetInputLayoutDesc();								// InputLayout
	graphicsPipelineStateDesc.VS = { 
		vertexShaderBlob->GetVertexShaderBlob()->GetBufferPointer(),
		vertexShaderBlob->GetVertexShaderBlob()->GetBufferSize()};	// VertexDhader
	graphicsPipelineStateDesc.PS = {
		pixelShaderBlob->GetPixelShaderBlob()->GetBufferPointer(),
		pixelShaderBlob->GetPixelShaderBlob()->GetBufferSize()};	// PixelShader
	graphicsPipelineStateDesc.BlendState = blendDesc->GetBlendDesc();															// BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc->GetRasterizerDesc();							// RasterizeerState

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
	hr = dxCommon->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));
#pragma endregion
}
