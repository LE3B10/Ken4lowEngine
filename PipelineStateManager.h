#pragma once
#include "DX12Include.h"
#include "RootSignatureManager.h"
#include "InputLayoutManager.h"
#include "BlendStateManager.h"
#include "RasterizerStateManager.h"
#include "ShaderManager.h"

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;

/// -------------------------------------------------------------
///		パイプラインステートオブジェクトマネージャークラス
/// -------------------------------------------------------------
class PipelineStateManager
{
public:
	/// ---------- メンバ関数 ---------- ///

	// PSOを生成する処理
	void CreatePipelineStateObject(DirectXCommon* dxCommon);

private:
	/// ---------- メンバ変数 ---------- ///

	Microsoft::WRL::ComPtr <ID3D12PipelineState> graphicsPipelineState = nullptr;

	std::unique_ptr<RootSignatureManager> rootSignature;
	std::unique_ptr<InputLayoutManager> inputLayoutDesc;
	std::unique_ptr<BlendStateManager> blendDesc;
	std::unique_ptr<RasterizerStateManager> rasterizerDesc;
	std::unique_ptr<ShaderManager> vertexShaderBlob;
	std::unique_ptr<ShaderManager> pixelShaderBlob;

	// パイプラインステートディスクリプタの初期化
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};	

};

