#pragma once
#include "IPostEffect.h"


/// -------------------------------------------------------------
///				ガウシアンフィルタエフェクトクラス
/// -------------------------------------------------------------
class GaussianFilterEffect : public IPostEffect
{
private: /// ---------- 構造体 ---------- ///

	// ガウシアンフィルタの設定
	struct GaussianFilterSetting
	{
		int kernelType; // カーネルサイズ
		float intensity; // 強度
		float threshold; // 閾値
		float sigma; // ガウス関数の標準偏差
		float padding[3]; // アライメント調整
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

private: /// ---------- 構造体 ---------- ///

	// 名前
	const std::string name_ = "GaussianFilterEffect";

	// シェーダーコードのパス
	std::string shaderPath_ = "Resources/Shaders/PostEffect/GaussianFilterEffect.hlsl";

	// DirectX共通クラス
	DirectXCommon* dxCommon_ = nullptr;

	// パイプラインビルダー
	PostEffectPipelineBuilder* pipelineBuilder_;

	// グラフィックスパイプラインステートオブジェクト
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

	// ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

	// ガウシアンフィルタの設定
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	GaussianFilterSetting* gaussianFilterSetting_ = nullptr;
};

