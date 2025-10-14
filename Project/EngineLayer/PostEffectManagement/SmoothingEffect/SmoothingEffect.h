#pragma once
#include "IPostEffect.h"


/// -------------------------------------------------------------
///					スムージングエフェクトクラス
/// -------------------------------------------------------------
class SmoothingEffect : public IPostEffect
{
private: /// ---------- 構造体 ---------- ///

	// スムージングの設定
	struct SmoothingSetting
	{
		int kernelType = 0; // 0: none, 1: box3x3, ...
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder) override;

	// 適用処理
	void Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndexx) override;

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

	// スムージングの設定
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	SmoothingSetting* smoothingSetting_ = nullptr;
};

