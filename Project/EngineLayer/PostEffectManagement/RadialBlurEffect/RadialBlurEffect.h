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
	void Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex) override;

	// ImGui描画処理
	void DrawImGui() override;

private: /// ---------- メンバ変数 ---------- ///

	// DirectX共通クラス
	DirectXCommon* dxCommon_ = nullptr;

	// パイプラインビルダー
	PostEffectPipelineBuilder* pipelineBuilder_;

	// コンピュートパイプラインステートオブジェクト
	Microsoft::WRL::ComPtr<ID3D12PipelineState> computePipelineState_;

	// コンピュートルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;

	// ラジアルブラーの設定
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	RadialBlurSetting* radialBlurSetting_ = nullptr;
};

