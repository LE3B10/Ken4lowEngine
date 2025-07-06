#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;
class PostEffectPipelineBuilder;


/// -------------------------------------------------------------
///				ポストエフェクトのインターフェース
/// -------------------------------------------------------------
class IPostEffect
{
public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	virtual ~IPostEffect() = default;

	// 初期化処理
	virtual void Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder) = 0;

	// 更新処理
	virtual void Update() {} // 更新処理が必要ない場合もあるので空でOK

	// 適用処理
	virtual void Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex) = 0;

	// ImGui描画処理
	virtual void DrawImGui() {} // ImGuiが必要ない場合もあるので空でOK

};

