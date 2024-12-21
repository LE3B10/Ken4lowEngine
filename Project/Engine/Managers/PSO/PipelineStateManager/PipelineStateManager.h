#pragma once
#include "DX12Include.h"
#include "RootSignatureManager.h"
#include "InputLayoutManager.h"
#include "BlendStateManager.h"
#include "BlendModeType.h"
#include "RasterizerStateManager.h"
#include "ShaderManager.h"
#include "DX12DepthStencil.h"

#include <array>
#include <memory>

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;


/// -------------------------------------------------------------
///		パイプラインステートオブジェクトマネージャークラス
/// -------------------------------------------------------------
class PipelineStateManager
{
public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	~PipelineStateManager();

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon, PipelineType pipelineType, BlendMode blendMode);

private: /// ---------- メンバ関数 ---------- ///

	// PSOを生成する処理
	void CreatePipelineStateObject(
		DirectXCommon* dxCommon,
		ID3D12RootSignature* rootSignature,
		D3D12_RENDER_TARGET_BLEND_DESC blendDesc,
		D3D12_RASTERIZER_DESC rasterizerDesc,
		D3D12_INPUT_LAYOUT_DESC inputLayout,
		D3D12_DEPTH_STENCIL_DESC depthStencilDesc,
		IDxcBlob* vertexShaderBlob,
		IDxcBlob* pixelShaderBlob
	);

public: /// ---------- ゲッター ---------- ///

	ID3D12PipelineState* GetPipelineState() const { return graphicsPipelineState_.Get(); }
	RootSignatureManager* GetRootSignature() const { return rootSignatureManager.get(); }

public: /// ---------- セッター ----- ///

	void SetGraphicsPipeline(ID3D12GraphicsCommandList* commandList);

private: /// ---------- メンバ変数 ---------- ///

	ComPtr <ID3D12PipelineState> graphicsPipelineState_;

	std::unique_ptr<RootSignatureManager> rootSignatureManager;
	std::unique_ptr<InputLayoutManager> inputLayoutManager;
	std::unique_ptr<BlendStateManager> blendStateManager;
	std::unique_ptr<RasterizerStateManager> rasterizerStateManager;
	std::unique_ptr<ShaderManager> shaderManager;
	std::unique_ptr<DX12DepthStencil> depthStencil;

	PipelineType pipelineType_;

	// パイプラインステートディスクリプタの初期化
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
};

