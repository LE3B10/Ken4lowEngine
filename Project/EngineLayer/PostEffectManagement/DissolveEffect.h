#pragma once
#include "IPostEffect.h"
#include <Vector4.h>


/// -------------------------------------------------------------
///					ディゾルブエフェクトクラス
/// -------------------------------------------------------------
class DissolveEffect : public IPostEffect
{
private: /// ---------- 構造体 ---------- ///

	// ディゾルブの設定
	struct DissolveSetting
	{
		float threshold = 0.5f; // 閾値
		float edgeThickness = 0.05f; // エッジの太さ
		Vector4 edgeColor = { 1.0f, 1.0f, 1.0f, 1.0f }; // エッジの色
		float padding[3];
	};

public: /// ---------- メンバ関数 ---------- ///
	
	// 初期化処理
	void Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder) override;
	
	// 適用処理
	void Apply(ID3D12GraphicsCommandList* commandList, uint32_t rtvSrvIndex, uint32_t dsvSrvIndex) override;
	
	// ImGui描画処理
	void DrawImGui() override;
	
	// 名前の取得
	const std::string& GetName() const override { return name_; }

private: /// ---------- メンバ変数 ---------- ///

	// 名前
	const std::string name_ = "DissolveEffect";

	// シェーダーコードのパス
	std::string shaderPath_ = "Resources/Shaders/PostEffect/DissolveEffect.hlsl";

	// DirectX共通クラス
	DirectXCommon* dxCommon_ = nullptr;

	// パイプラインビルダー
	PostEffectPipelineBuilder* pipelineBuilder_;

	// グラフィックスパイプラインステートオブジェクト
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

	// ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

	// ディゾルブの設定
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	DissolveSetting* dissolveSetting_ = nullptr;

	uint32_t dissolveMaskSrvIndex_ = 0; // SRV index for mask
};

