#pragma once
#include "IPostEffect.h"
#include "Vector4.h"


/// -------------------------------------------------------------
///				　グレイスケールエフェクトクラス
/// -------------------------------------------------------------
class GrayScaleEffect : public IPostEffect
{
private: /// ---------- 構造体 ---------- ///

	// グレイスケールエフェクトの設定
	struct GrayScaleSetting
	{
		Vector4 color; // 強度
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder) override;

	// 適用処理
	void Apply(ID3D12GraphicsCommandList* commandList, uint32_t rtvSrvIndex, uint32_t dsvSrvIndex) override;

	// ImGui描画処理
	void DrawImGui() override;

public: /// ---------- ゲッター ---------- ///

	// 名前の取得
	const std::string& GetName() const override { return name_; }

	// 色の取得
	const Vector4& GetColor() const { return grayScaleSetting_->color; }

public: /// ---------- セッター ---------- ///

	// 色の設定
	void SetColor(const Vector4& color) { grayScaleSetting_->color = color; }

private: /// ---------- メンバ変数 ---------- ///

	// 名前
	const std::string name_ = "GrayScaleEffect";

	// シェーダーコードのパス
	std::string shaderPath_ = "Resources/Shaders/PostEffect/GrayScaleEffect.hlsl";

	// DirectX共通クラス
	DirectXCommon* dxCommon_ = nullptr;

	// パイプラインビルダー
	PostEffectPipelineBuilder* pipelineBuilder_;

	// グラフィックスパイプラインステートオブジェクト
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

	// ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

	// グレイスケールエフェクトの設定
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	GrayScaleSetting* grayScaleSetting_ = nullptr;
};

