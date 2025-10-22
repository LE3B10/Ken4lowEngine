#pragma once
#include "IPostEffect.h"
#include <Vector2.h>
#include <Vector4.h>


/// -------------------------------------------------------------
///			輝度ベースのアウトラインエフェクトクラス
/// -------------------------------------------------------------
class LuminanceOutlineEffect : public IPostEffect
{
private: /// ---------- 構造体 ---------- ///

	// アウトラインの設定
	struct LuminanceOutlineSetting
	{
		Vector4 color; // アウトラインの色
		Vector2 texelSize;
		float edgeStrength;
		float threshold;
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
	PostEffectPipelineBuilder* pipelineBuilder_ = nullptr;

	// コンピュートパイプラインステートオブジェクト
	Microsoft::WRL::ComPtr<ID3D12PipelineState> computePipelineState_;

	// コンピュートルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;

	// ガウシアンフィルタの設定
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	LuminanceOutlineSetting* luminanceOutlineSetting_ = nullptr;
};

