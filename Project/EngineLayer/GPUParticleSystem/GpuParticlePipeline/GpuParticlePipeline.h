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

	/// <summary>
	/// GPU パーティクル用パイプラインの初期化処理。<br/>
	/// ・DirectXCommon インスタンスの取得<br/>
	/// ・描画用ルートシグネチャの生成（CreateRootSignature）<br/>
	/// ・描画用グラフィックス PSO の生成（CreatePSO）<br/>
	/// ・コンピュート用ルートシグネチャの生成（CreateComputeRootSignature）<br/>
	/// ・シミュレーション CS / Emit CS / Update CS 用 PSO の生成<br/>
	/// を順番に呼び出します。
	/// </summary>
	void Initialize();

public: /// ---------- ゲッター ---------- ///

	/// <summary>
	/// 描画用グラフィックスパイプラインのルートシグネチャを取得します。<br/>
	/// ・b0 : シミュレーション定数（PerView / 汎用用 CBV）<br/>
	/// ・t0 : 頂点シェーダ用パーティクル SRV テーブル<br/>
	/// ・b1 : マテリアル用 CBV<br/>
	/// ・t1 : テクスチャ SRV テーブル<br/>
	/// といった構成になっています。
	/// </summary>
	ID3D12RootSignature* GetGfxRootSignature() const { return rootSignature_.Get(); }

	/// <summary>
	/// 描画用グラフィックスパイプラインステートオブジェクトを取得します。<br/>
	/// GpuParticleRenderer 側で SetPipelineState に渡して使用します。
	/// </summary>
	ID3D12PipelineState* GetGfxPSO() const { return pipelineState_.Get(); }

	/// <summary>
	/// コンピュートシェーダ用ルートシグネチャを取得します。<br/>
	/// ・b0 : シミュレーション定数（Δt や最大パーティクル数など）<br/>
	/// ・u0〜u2 : パーティクルバッファ群用 UAV テーブル<br/>
	/// ・b1 : 射出用定数（Emit 設定用）<br/>
	/// ・b2 : 時間計測／デバッグ用定数<br/>
	/// といったレイアウトになっています。
	/// </summary>
	ID3D12RootSignature* GetCsRootSignature() const { return computeRootSignature_.Get(); }

	/// <summary>
	/// メインのシミュレーション用コンピュート PSO を取得します。<br/>
	/// パーティクルステートの更新（位置・速度・寿命など）に使用します。
	/// </summary>
	ID3D12PipelineState* GetCsPSO() const { return computePipelineState_.Get(); }

	/// <summary>
	/// パーティクル射出用コンピュート PSO を取得します。<br/>
	/// Emit 専用の GpuParticleEmit.CS.hlsl を実行する際に使用します。
	/// </summary>
	ID3D12PipelineState* GetCsEmitPSO() const { return emitComputePipelineState_.Get(); }

	/// <summary>
	/// パーティクル更新用コンピュート PSO を取得します。<br/>
	/// GpuParticleUpdate.CS.hlsl を実行する際に使用します。<br/>
	/// ※ 実装上は「更新」と「メインシミュレーション」を分けておきたい場合に利用します。
	/// </summary>
	ID3D12PipelineState* GetCsUpdatePSO() const { return updateComputePipelineState_.Get(); }

private: /// ---------- 内部メンバ関数 ---------- ///

	/// <summary>
	/// GPU パーティクル描画用ルートシグネチャを生成します。<br/>
	/// Vertex/Pixel シェーダで使用する CBV / SRV / サンプラーのレイアウトを定義します。
	/// </summary>
	void CreateRootSignature();

	/// <summary>
	/// GPU パーティクル描画用グラフィックス PSO を生成します。<br/>
	/// ・入力レイアウト（POSITION / TEXCOORD / NORMAL）<br/>
	/// ・頂点 / ピクセルシェーダ（GpuParticle.VS / PS）<br/>
	/// ・ブレンドステート（blendMode_ に基づく加算合成など）<br/>
	/// ・ラスタライザ / DepthStencil 設定<br/>
	/// をまとめて構築します。
	/// </summary>
	void CreatePSO();

	/// <summary>
	/// コンピュートシェーダ用ルートシグネチャを生成します。<br/>
	/// シミュレーション定数 CBV、UAV テーブル、Emit 用 CBV などを定義します。
	/// </summary>
	void CreateComputeRootSignature();

	/// <summary>
	/// シミュレーション用コンピュート PSO を生成します。<br/>
	/// GpuParticle.CS.hlsl をコンパイルして PSO を作成します。
	/// </summary>
	void CreateComputePSO();

	/// <summary>
	/// 射出用コンピュート PSO を生成します。<br/>
	/// GpuParticleEmit.CS.hlsl をコンパイルして PSO を作成します。
	/// </summary>
	void CreateEmitComputePSO();

	/// <summary>
	/// 更新用コンピュート PSO を生成します。<br/>
	/// GpuParticleUpdate.CS.hlsl をコンパイルして PSO を作成します。
	/// </summary>
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

