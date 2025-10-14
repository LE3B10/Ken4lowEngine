#pragma once
#include "IPostEffect.h"


/// -------------------------------------------------------------
///						ヴィネットエフェクト
/// -------------------------------------------------------------
class VignetteEffect : public IPostEffect
{
private: /// ---------- 構造体 ---------- ///

	// ヴィネットの設定
	struct VignetteSetting
	{
		float power = 1.0f;
		float range = 0.5f;
		float padding[4];
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

	// ヴィグネットの設定
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;

	// ヴィグネットの設定データ
	VignetteSetting* vignetteSetting_ = nullptr;

	// ヘッダー側に追加
	VignetteSetting setting_ = { 1.0f, 0.5f };
};

