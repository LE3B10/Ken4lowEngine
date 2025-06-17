#pragma once
#include <IPostEffect.h>
#include "Vector2.h"
#include "Vector4.h"
#include "Matrix4x4.h"

/// ---------- 前方宣言 ---------- ///
class Camera;


class DepthOutlineEffect : public IPostEffect
{
private:

	struct DepthOutlineSetting
	{
		Vector2 texelSize; // 1 / 解像度
		float depthScale; // 深度差
		float edgeThickness; // ピクセル単位の太さ
		Vector4 edgeColor; // アウトラインの色
		Matrix4x4 projectionInverse; // +64 = 96
	};

public: /// ---------- メンバ関数 ---------- ///

	// コンストラクタ
	DepthOutlineEffect(Camera* camera);

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder) override;

	// 適用処理
	void Apply(ID3D12GraphicsCommandList* commandList, uint32_t rtvSrvIndex, uint32_t dsvSrvIndex) override;

	// ImGui描画処理
	void DrawImGui() override;

	// 名前の取得
	const std::string& GetName() const override { return name_; }

private: /// ---------- 構造体 ---------- ///

	Camera* camera_ = nullptr; // カメラへのポインタ

	// 名前
	const std::string name_ = "DepthOutlineEffect";

	// シェーダーコードのパス
	std::string shaderPath_ = "Resources/Shaders/PostEffect/DepthOutlineEffect.PS.hlsl";

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
	DepthOutlineSetting* depthOutlineSetting_ = nullptr;
};

