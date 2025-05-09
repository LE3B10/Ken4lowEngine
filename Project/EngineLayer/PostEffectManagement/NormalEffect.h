#pragma once
#include "IPostEffect.h"


/// -------------------------------------------------------------
///				　		ノーマルエフェクト
/// -------------------------------------------------------------
class NormalEffect : public IPostEffect
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder) override;
	
	// 適用処理
	void Apply(ID3D12GraphicsCommandList* commandList, uint32_t rtvSrvIndex, uint32_t dsvSrvIndex) override;
	
	// 名前の取得
	const std::string& GetName() const override { return name_; }

private: /// ---------- メンバ変数 ---------- ///
	
	// 名前
	const std::string name_ = "NormalEffect";

	// シェーダーコードのパス
	std::string shaderPath_ = "Resources/Shaders/PostEffect/NormalEffect.hlsl";

	// DirectX共通クラス
	DirectXCommon* dxCommon_ = nullptr;

	// パイプラインビルダー
	PostEffectPipelineBuilder* pipelineBuilder_;

	// グラフィックスパイプラインステートオブジェクト
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

	// ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
};

