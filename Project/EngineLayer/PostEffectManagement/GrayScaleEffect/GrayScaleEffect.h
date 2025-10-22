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
	void Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex) override;

	// ImGui描画処理
	void DrawImGui() override;

public: /// ---------- ゲッター ---------- ///

	// 色の取得
	const Vector4& GetColor() const { return grayScaleSetting_->color; }

public: /// ---------- セッター ---------- ///

	// 色の設定
	void SetColor(const Vector4& color) { grayScaleSetting_->color = color; }

private: /// ---------- メンバ変数 ---------- ///

	// DirectX共通クラス
	DirectXCommon* dxCommon_ = nullptr;

	// パイプラインビルダー
	PostEffectPipelineBuilder* pipelineBuilder_ = nullptr;

	// コンピュートパイプラインステートオブジェクト
	Microsoft::WRL::ComPtr<ID3D12PipelineState> computePipelineState_;

	// コンピュートルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;

	// グレイスケールエフェクトの設定
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	GrayScaleSetting* grayScaleSetting_ = nullptr;
};

