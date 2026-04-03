#pragma once
#include <DX12Include.h>
#include "ParticleMesh.h"
#include "ParticleMaterial.h"

namespace Ken4lowEngine
{

/// ---------- 前方宣言 ---------- ///
class GpuParticleSpritePipeline;
class GpuParticleBuffers;

/// -------------------------------------------------------------
///			　	GPUパーティクルレンダラークラス
/// -------------------------------------------------------------
class GpuParticleRenderer
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// GPU パーティクルレンダラーの初期化を行います。<br/>
	/// ・パイプライン / バッファのポインタを保持<br/>
	/// ・パーティクルメッシュの生成と初期化<br/>
	/// ・パーティクルマテリアルの生成と初期化<br/>
	/// ・使用テクスチャ（textureFilePath_）の読み込み<br/>
	/// を行います。
	/// </summary>
	/// <param name="pipeline">描画に使用する GPU パーティクル用グラフィックスパイプライン。</param>
	/// <param name="buffers">PerView 定数バッファやパーティクル用 SRV を持つバッファ管理クラス。</param>
	void Initialize(GpuParticleSpritePipeline* pipeline, GpuParticleBuffers* buffers);

	/// <summary>
	/// GPU パーティクルの描画処理を行います。<br/>
	/// ・パイプライン / ルートシグネチャのセット<br/>
	/// ・パーティクルメッシュ（VB / 必要なら IB）のセット<br/>
	/// ・SRVManager を使ったディスクリプタヒープのセットアップ<br/>
	/// ・RootParameter 0: PerView 用 CBV（ビュー射影など）<br/>
	/// ・RootParameter 1: パーティクルバッファの SRV（GpuParticleBuffers 側で確保したもの）<br/>
	/// ・RootParameter 2: パーティクルマテリアルの CBV<br/>
	/// ・RootParameter 3: テクスチャ SRV（textureFilePath_）<br/>
	/// を設定した上で、instanceCount 個分のインスタンシング描画を行います。
	/// </summary>
	/// <param name="instanceCount">描画するパーティクルインスタンス数（GPU 上で生存しているパーティクル数など）。</param>
	void Draw(UINT instanceCount, uint32_t slot = 0);

public: /// ---------- セッター ---------- ///

	// テクスチャファイルパスのセッター
	void SetTextureFilePath(const std::string& path);

	// 描画タイプのセッター
	void SetDrawType(uint32_t type, uint32_t slot);

private: /// ---------- メンバ変数 ---------- ///

	// GPUパーティクルスプライトパイプライン
	GpuParticleSpritePipeline* gpuParticlePipeline_ = nullptr;

	// GPUパーティクルバッファ
	GpuParticleBuffers* gpuParticleBuffers_ = nullptr;

	// パーティクルメッシュ
	std::unique_ptr<ParticleMesh> particleMesh_;

	// パーティクルマテリアル
	std::unique_ptr<ParticleMaterial> particleMaterial_;

	std::string textureFilePath_ = "Effects/circle2.dds";
};


} // namespace Ken4lowEngine
