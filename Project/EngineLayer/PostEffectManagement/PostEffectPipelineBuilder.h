#pragma once
#include "DX12Include.h"
#include <string>

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;

/// -------------------------------------------------------------
///				　ポストエフェクトパイプラインビルダー
/// -------------------------------------------------------------
class PostEffectPipelineBuilder
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon);

	/// <summary>
	/// ルートシグネチャの生成
	/// </summary>
	/// <param name="useDepthSRV">depthあり／なし選択可</param>
	/// <returns></returns>
	ComPtr<ID3D12RootSignature> CreateRootSignature();

	/// <summary>
	/// パイプラインステートの生成
	/// </summary>
	/// <param name="pixelShaderPath">ピクセルシェーダーのパス</param>
	/// <param name="rootSignature">ルートシグネチャ</param>
	/// <param name="enableDepth">depthあり／なし選択可</param>
	/// <returns></returns>
	ComPtr<ID3D12PipelineState> CreateGraphicsPipeline(const std::wstring& pixelShaderPath, ID3D12RootSignature* rootSignature, bool enableDepth = false);

private: /// ---------- メンバ変数 ---------- ///

	// DirectX共通クラス
	DirectXCommon* dxCommon_ = nullptr;
};

