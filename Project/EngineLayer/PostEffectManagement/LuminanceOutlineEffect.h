#pragma once
#include "IPostEffect.h"
#include <Vector2.h>


/// -------------------------------------------------------------
///			輝度ベースのアウトラインエフェクトクラス
/// -------------------------------------------------------------
class LuminanceOutlineEffect : public IPostEffect
{
private: /// ---------- 構造体 ---------- ///

	// アウトラインの設定
	struct LuminanceOutlineSetting
	{
		Vector2 texelSize;
		float edgeStrength;
		float threshold;
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
	const std::string name_ = "LuminanceOutlineEffect";

	// シェーダーコードのパス
	std::string shaderPath_ = "Resources/Shaders/PostEffect/LuminanceOutlineEffect.hlsl";

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
	LuminanceOutlineSetting* luminanceOutlineSetting_ = nullptr;
};

