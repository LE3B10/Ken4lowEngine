#pragma once
#include <DX12Include.h>

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;

/// -------------------------------------------------------------
///			　GPUパーティクルパイプラインクラス
/// -------------------------------------------------------------
class GpuParticlePipeline
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

public: /// ---------- ゲッター ---------- ///

	// ルートシグネチャの取得
	ID3D12RootSignature* GetGfxRootSignature() const { return rootSignature_.Get(); }

	// パイプラインステートオブジェクトの取得
	ID3D12PipelineState* GetGfxPSO() const { return pipelineState_.Get(); }

	// コンピュートルートシグネチャの取得
	ID3D12RootSignature* GetCsRootSignature() const { return computeRootSignature_.Get(); }

	// コンピュートパイプラインステートオブジェクトの取得
	ID3D12PipelineState* GetCsPSO() const { return computePipelineState_.Get(); }

	// エミット用コンピュートパイプラインステートオブジェクトの取得
	ID3D12PipelineState* GetCsEmitPSO() const { return emitComputePipelineState_.Get(); }

	// パーティクル更新用コンピュートパイプラインステートオブジェクトの取得
	ID3D12PipelineState* GetCsUpdatePSO() const { return updateComputePipelineState_.Get(); }

private: /// ---------- メンバ関数 ---------- ///

	// ルートシグネチャの生成
	void CreateRootSignature();

	// PSOを生成
	void CreatePSO();

	// コンピュートシェーダー用のルートシグネチャの生成
	void CreateComputeRootSignature();

	// コンピュートシェーダー用のパイプラインステートオブジェクトの生成
	void CreateComputePSO();

	// エミット用コンピュートシェーダー用のパイプラインステートオブジェクトの生成
	void CreateEmitComputePSO();

	// パーティクル更新用コンピュートシェーダーのコンパイル
	void CreateUpdateComputePSO();

private: /// ---------- メンバ変数 ---------- ///

	// DirectX共通管理
	DirectXCommon* dxCommon_ = nullptr;

	// ルートシグネチャ
	ComPtr<ID3D12RootSignature> rootSignature_;

	// グラフィックスパイプラインステートオブジェクト
	ComPtr<ID3D12PipelineState> pipelineState_;

	// コンピュートパイプラインステートオブジェクト
	ComPtr<ID3D12PipelineState> computePipelineState_;

	// コンピュートルートシグネチャ
	ComPtr<ID3D12RootSignature> computeRootSignature_;

	// エミット用コンピュートパイプラインステートオブジェクト
	ComPtr<ID3D12PipelineState> emitComputePipelineState_;  // Emit用

	// パーティクル更新用コンピュートパイプラインステートオブジェクト
	ComPtr<ID3D12PipelineState> updateComputePipelineState_;

	// ブレンドモード
	BlendMode blendMode_ = BlendMode::kBlendModeAdd; // 加算
};

