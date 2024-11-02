#pragma once
#include "DX12Include.h"
#include "RootSignatureManager.h"
#include "InputLayoutManager.h"
#include "BlendStateManager.h"
#include "BlendModeType.h"
#include "RasterizerStateManager.h"
#include "ShaderManager.h"
#include "DXDepthStencil.h"

#include "BlendModeType.h"

#include <array>
#include <memory>

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;

/// ---------- パイプライン列挙型 ---------- ///
enum class PipelineType
{
	Object3d,		// 3Dオブジェクト
	PipelineTypeNum // 合計
};

// パイプラインタイプの数
static inline const uint32_t pipelineTypeNum = static_cast<size_t>(PipelineType::PipelineTypeNum);


/// -------------------------------------------------------------
///		パイプラインステートオブジェクトマネージャークラス
/// -------------------------------------------------------------
class PipelineStateManager
{
public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	~PipelineStateManager();

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon);

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

	ID3D12PipelineState* GetPipelineState() const;

public: /// ---------- セッター ----- ///

	void SetGraphicsPipeline(ID3D12GraphicsCommandList* commandList);

private: /// ---------- メンバ変数 ---------- ///

	Microsoft::WRL::ComPtr <ID3D12PipelineState> graphicsPipelineState_;

	std::array<BlendMode, blendModeNum> blendModeTypes_{};
	std::array<PipelineType, pipelineTypeNum> pipelineTypeNums_{};

	std::unique_ptr<RootSignatureManager> rootSignatureManager;
	std::unique_ptr<InputLayoutManager> inputLayoutManager;
	std::unique_ptr<BlendStateManager> blendStateManager;
	std::unique_ptr<RasterizerStateManager> rasterizerStateManager;
	std::unique_ptr<ShaderManager> shaderManager;
	std::unique_ptr<DXDepthStencil> depthStencil;

	// パイプラインステートディスクリプタの初期化
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
};

