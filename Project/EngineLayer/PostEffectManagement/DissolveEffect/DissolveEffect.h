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
		float threshold;        // 閾値
		float edgeThickness;    // エッジの太さ
		float padding0[2];      // パディング
		Vector4 edgeColor;      // 色
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
	PostEffectPipelineBuilder* pipelineBuilder_ = nullptr;

	// コンピュートパイプラインステートオブジェクト
	Microsoft::WRL::ComPtr<ID3D12PipelineState> computePipelineState_;

	// コンピュートルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;

	// ディゾルブの設定
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	DissolveSetting* dissolveSetting_ = nullptr;

	uint32_t dissolveMaskSrvIndex_ = 0; // SRV index for mask
	uint32_t dissolveMaskSrvIndexOnUAV_ = 0;   // ★UAVヒープ側に複製したSRV
};

