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
	void Apply(ID3D12GraphicsCommandList* commandList, uint32_t rtvSrvIndex, uint32_t dsvSrvIndex) override;

	// ImGui描画処理
	void DrawImGui() override;

	// 名前の取得
	const std::string& GetName() const override { return name_; }

private: /// ---------- メンバ変数 ---------- ///

	// 名前
	const std::string name_ = "VignetteEffect";

	// シェーダーコードのパス
	std::string shaderPath_ = "Resources/Shaders/PostEffect/VignetteEffect.hlsl";

	// DirectX共通クラス
	DirectXCommon* dxCommon_ = nullptr;

	// パイプラインビルダー
	PostEffectPipelineBuilder* pipelineBuilder_;

	// グラフィックスパイプラインステートオブジェクト
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

	// ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

	// ヴィグネットの設定
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;

	// ヴィグネットの設定データ
	VignetteSetting* vignetteSetting_ = nullptr;

	// ヘッダー側に追加
	VignetteSetting setting_ = { 1.0f, 0.5f };
};

