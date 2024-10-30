#pragma once
#include "BlendStateManager.h"
#include "DX12Include.h"
#include "InputLayoutManager.h"
#include "RasterizerStateManager.h"
#include "RootSignatureManager.h"
#include "ShaderManager.h"


/// -------------------------------------------------------------
///		パイプラインステートオブジェクトマネージャークラス
/// -------------------------------------------------------------
class PipelineStateManager
{
public: /// ---------- メンバ関数 ---------- ///
	
	// PSOを生成する処理
	void Initialize(
		ID3D12Device* device,
		RootSignatureManager& rootSignatureManager,
		BlendStateManager& blenderStateManager,
		RasterizerStateManager& rasterizerStateManager,
		InputLayoutManager& inputLayoutManager,
		ID3DBlob* vertexShaderBlob,
		ID3DBlob* pixelShaderBlob
	);

public: /// ---------- ゲッター ---------- ///

	ID3D12PipelineState* GetPipelineState() const;

private: /// ---------- メンバ変数 ---------- ///

	Microsoft::WRL::ComPtr <ID3D12PipelineState> graphicsPipelineState;
	
	// パイプラインステートディスクリプタの初期化
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};	
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
};

