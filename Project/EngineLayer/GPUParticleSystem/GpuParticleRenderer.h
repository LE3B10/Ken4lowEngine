#pragma once
#include <DX12Include.h>
#include "ParticleMesh.h"
#include "ParticleMaterial.h"

/// ---------- 前方宣言 ---------- ///
class GpuParticlePipeline;
class GpuParticleBuffers;

/// -------------------------------------------------------------
///			　	GPUパーティクルレンダラークラス
/// -------------------------------------------------------------
class GpuParticleRenderer
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(GpuParticlePipeline* pipeline, GpuParticleBuffers* buffers);

	// 描画処理
	void Draw(UINT instanceCount);

private: /// ---------- メンバ変数 ---------- ///

	// GPUパーティクルパイプライン
	GpuParticlePipeline* gpuParticlePipeline_ = nullptr;

	// GPUパーティクルバッファ
	GpuParticleBuffers* gpuParticleBuffers_ = nullptr;

	// パーティクルメッシュ
	std::unique_ptr<ParticleMesh> particleMesh_;

	// パーティクルマテリアル
	std::unique_ptr<ParticleMaterial> particleMaterial_;

	std::string textureFilePath_ = "circle2.png";
};

