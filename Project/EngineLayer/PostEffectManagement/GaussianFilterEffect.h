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
		bool isHorizontal; // 水平方向か垂直方向か
		float padding[3]; // アライメント調整
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder) override;

	// 適用処理
	void Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex) override;

	// ImGui描画処理
	void DrawImGui() override;

private: /// ---------- 構造体 ---------- ///

	// DirectX共通クラス
	DirectXCommon* dxCommon_ = nullptr;

	// パイプラインビルダー
	PostEffectPipelineBuilder* pipelineBuilder_;

	// コンピュートパイプラインステートオブジェクト
	Microsoft::WRL::ComPtr<ID3D12PipelineState> computePipelineState_;

	// コンピュートルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;

	// ガウシアンフィルタの設定
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	GaussianFilterSetting* gaussianFilterSetting_ = nullptr;
};

