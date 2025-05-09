#pragma once
#include "IPostEffect.h"
#include <Vector2.h>


/// -------------------------------------------------------------
///				　ラジアルブラーエフェクトクラス
/// -------------------------------------------------------------
class RadialBlurEffect : public IPostEffect
{
private: /// ---------- 構造体 ---------- ///

	// ラジアルブラーの設定
	struct RadialBlurSetting
	{
		Vector2 center = { 0.5f, 0.5f };
		float blurStrength = 1.0f;
		float sampleCount = 16.0f;
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
	const std::string name_ = "RadialBlurEffect";

	// シェーダーコードのパス
	std::string shaderPath_ = "Resources/Shaders/PostEffect/RadialBlurEffect.hlsl";

	// DirectX共通クラス
	DirectXCommon* dxCommon_ = nullptr;

	// パイプラインビルダー
	PostEffectPipelineBuilder* pipelineBuilder_;

	// グラフィックスパイプラインステートオブジェクト
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

	// ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

	// ラジアルブラーの設定
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	RadialBlurSetting* radialBlurSetting_ = nullptr;
};

