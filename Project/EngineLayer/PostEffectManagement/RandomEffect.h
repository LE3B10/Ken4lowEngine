#pragma once
#include "IPostEffect.h"
#include "Vector2.h"


/// -------------------------------------------------------------
///					ランダムエフェクトクラス
/// -------------------------------------------------------------
class RandomEffect : public IPostEffect
{
private: /// ---------- 構造体 ---------- ///

	// ランダムエフェクトの設定
	struct RandomSetting
	{
		float time = 0.0f;
		bool useMultiply = false; // 乗算を使用するかどうか
		float padding[3]; // 16バイトアライメントを守る
		Vector2 textureSize; // テクスチャのサイズ
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder) override;

	// 更新処理
	void Update() override;

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

	// ランダムエフェクトの設定
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	RandomSetting* randomSetting_ = nullptr;
};

