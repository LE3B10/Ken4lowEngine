#pragma once
#include "DX12Include.h"
#include <BlendModeType.h>
#include "Vector3.h"
#include "Quaternion.h"
#include <ModelData.h>
#include <TransformationMatrix.h>
#include "LightManager.h"

#include <array>
#include <string>
#include <vector>
#include <numbers>
#include <map>


/// ---------- 前方宣言 ---------- ///
class DirectXCommon;


/// -------------------------------------------------------------
///				　アニメーションパイプラインビルダー
/// -------------------------------------------------------------
class AnimationPipelineBuilder
{
public: /// ---------- メンバ関数 ---------- ///

	// シングルトンインスタンス
	static AnimationPipelineBuilder* GetInstance();

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon);

	// 共通描画処理設定
	void SetRenderSetting();

	// コンピュートシェーダー用の設定
	void SetComputeSetting();

	// ルートシグネチャの取得
	ID3D12RootSignature* GetRootSignature() const { return rootSignature.Get(); }

	// パイプラインの取得
	ID3D12PipelineState* GetPipelineState() const { return graphicsPipelineState.Get(); }

	// コンピュート用ルートシグネチャの取得
	ID3D12RootSignature* GetComputeRootSignature() const { return computeRootSignature_.Get(); }

	// コンピュート用パイプラインの取得
	ID3D12PipelineState* GetComputePipelineState() const { return computePipelineState_.Get(); }

private: /// ---------- メンバ関数 ---------- ///

	// ルートシグネチャの生成
	void CreateRootSignature();

	// パイプラインの生成
	void CreatePSO();

	// ルートシグネチャの生成（コンピュート用）
	void CreateComputeRootSignature();

	// パイプラインの生成（コンピュート用）
	void CreateComputePSO();

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;

	// ルートシグネチャとパイプラインステート
	ComPtr<ID3D12RootSignature> rootSignature;
	ComPtr<ID3D12PipelineState> graphicsPipelineState;

	// コンピュート用ルートシグネチャとパイプライン
	ComPtr <ID3D12RootSignature> computeRootSignature_;
	ComPtr<ID3D12PipelineState> computePipelineState_;

	// ブレンドモード
	BlendMode blendMode_ = BlendMode::kBlendModeNone;
};

