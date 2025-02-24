#pragma once
#include "DX12Include.h"
#include <BlendModeType.h>
#include <array>


/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class ShaderManager;

/// -------------------------------------------------------------
///			アニメーションのパイプラインを管理するクラス
/// -------------------------------------------------------------
class AnimationModelCommon
{
public: /// ---------- メンバ関数 ---------- ///

	// シングルトンインスタンス
	static AnimationModelCommon* GetInstance();

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon);

private: /// ---------- メンバ関数 ---------- ///

	// ルートシグネチャの生成
	void CreateRootSignature();

	// パイプラインの生成
	void CreatePSO();

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;
	ShaderManager* shaderManager_ = nullptr;


	// ブレンドモード
	BlendMode cuurenttype = BlendMode::kBlendModeNone;

	ComPtr <ID3D12RootSignature> rootSignature = nullptr;
	ComPtr <ID3D12PipelineState> graphicsPipelineState = nullptr;
	ComPtr <ID3D12Resource> materialResource;
	ComPtr <ID3D12Resource> vertexResource;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
};

