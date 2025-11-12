#include "GpuParticleRenderer.h"
#include "DirectXCommon.h"
#include "GpuParticlePipeline.h"
#include "GpuParticleBuffers.h"
#include "SRVManager.h"
#include <TextureManager.h>

/// -------------------------------------------------------------
///				　　　	初期化処理
/// -------------------------------------------------------------
void GpuParticleRenderer::Initialize(GpuParticlePipeline* pipeline, GpuParticleBuffers* buffers)
{
	TextureManager::GetInstance()->LoadTexture(textureFilePath_);

	// 引数でパイプラインとバッファのポインタを受け取ってメンバ変数に記録する
	gpuParticlePipeline_ = pipeline;
	gpuParticleBuffers_ = buffers;

	// パーティクルメッシュの生成と初期化
	particleMesh_ = std::make_unique<ParticleMesh>();
	particleMesh_->Initialize();

	// パーティクルマテリアルの生成と初期化
	particleMaterial_ = std::make_unique<ParticleMaterial>();
	particleMaterial_->Initialize();
}

/// -------------------------------------------------------------
///				　　　			描画処理
/// -------------------------------------------------------------
void GpuParticleRenderer::Draw(UINT instanceCount)
{
	auto* commandList = DirectXCommon::GetInstance()->GetCommandManager()->GetCommandList();

	// パイプラインの設定
	commandList->SetGraphicsRootSignature(gpuParticlePipeline_->GetGfxRootSignature());
	commandList->SetPipelineState(gpuParticlePipeline_->GetGfxPSO());

	// メッシュ（VB/IB）セット
	const auto& vbView = particleMesh_->GetVertexBufferView();
	commandList->IASetVertexBuffers(0, 1, &vbView);

	// ディスクリプタヒープの設定
	SRVManager::GetInstance()->PreDraw();

	// PerView を CBV として設定
	commandList->SetGraphicsRootConstantBufferView(0, gpuParticleBuffers_->GetPerViewBuffer()->GetGPUVirtualAddress());

	// パーティクルバッファのSRVをセット
	SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(1, gpuParticleBuffers_->GetParticleSrvIndex());

	// マテリアル設定
	particleMaterial_->SetPipeline(2);

	TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 3, TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_));

	// インスタンス数分描画
	if (particleMesh_->HasIndex())
	{
		const auto& ibView = particleMesh_->GetIndexBufferView();
		commandList->IASetIndexBuffer(&ibView);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawIndexedInstanced(static_cast<UINT>(ibView.SizeInBytes / sizeof(uint32_t)), instanceCount, 0, 0, 0);
	}
	else
	{
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawInstanced(static_cast<UINT>(vbView.SizeInBytes / vbView.StrideInBytes), instanceCount, 0, 0);
	}
}