#pragma once
#include "PipelineState.h"

#include <memory>


/// -------------------------------------------------------------
///				　	　	3Dパイプラインクラス
/// -------------------------------------------------------------
class Pipeline3D : public PipelineState
{
public: /// ---------- メンバ関数 ---------- ///

	// コンストラクタ
	Pipeline3D(DirectXCommon* dxCommon);

	// 初期化処理
	void Initialize() override;

	// 描画設定処理
	void Render() override;

public: /// ---------- ゲッター ---------- ///

	ID3D12PipelineState* GetPipelineState() const override { return graphicsPipelineState_.Get(); }
	ID3D12RootSignature* GetRootSignature() const override { return rootSignature_.Get(); }

private: /// ---------- メンバ関数 ---------- ///

	void CreateRootSignature();

	void CreatePSO();

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;

};

