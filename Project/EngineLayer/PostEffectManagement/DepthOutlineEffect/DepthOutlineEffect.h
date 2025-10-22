#pragma once
#include <IPostEffect.h>
#include "Vector2.h"
#include "Vector4.h"
#include "Matrix4x4.h"

/// ---------- 前方宣言 ---------- ///
class Camera;

/// -------------------------------------------------------------
///				　  DepthOutlineEffectクラス
/// -------------------------------------------------------------
class DepthOutlineEffect : public IPostEffect
{
private: /// ---------- 構造体 ---------- ///

	// 深度アウトライン設定構造体
	struct DepthOutlineSetting
	{
		Vector2 texelSize;			 // 1 / 解像度
		float depthScale;			 // 深度差
		float edgeThickness;		 // ピクセル単位の太さ
		Vector4 edgeColor;			 // アウトラインの色
		Matrix4x4 projectionInverse; // 逆射影行列
	};

public: /// ---------- メンバ関数 ---------- ///

	// コンストラクタ
	DepthOutlineEffect(Camera* camera);

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder) override;

	// 適用処理
	void Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex) override;

	// ImGui描画処理
	void DrawImGui() override;

private: /// ---------- 構造体 ---------- ///

	Camera* camera_ = nullptr; // カメラへのポインタ

	// DirectX共通クラス
	DirectXCommon* dxCommon_ = nullptr;

	// パイプラインビルダー
	PostEffectPipelineBuilder* pipelineBuilder_ = nullptr;

	// グラフィックスパイプラインステートオブジェクト
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

	// ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

	// ガウシアンフィルタの設定
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	DepthOutlineSetting* depthOutlineSetting_ = nullptr;
};

