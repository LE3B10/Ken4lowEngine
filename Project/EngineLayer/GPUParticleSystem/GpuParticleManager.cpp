#include "GpuParticleManager.h"
#include "Object3DCommon.h"
#include <DebugCamera.h>
#include <DirectXCommon.h>
#include <SRVManager.h>
#include <UAVManager.h>

/// -------------------------------------------------------------
///				　	シングルトンインスタンス
/// -------------------------------------------------------------
GpuParticleManager* GpuParticleManager::GetInstance()
{
	static GpuParticleManager instance;
	return &instance;
}

/// -------------------------------------------------------------
///				　　　	初期化処理
/// -------------------------------------------------------------
void GpuParticleManager::Initialize(Camera* camera)
{
	// 引数でカメラのポインタを受け取ってメンバ変数に記録する
	camera_ = camera;

	// GPUパーティクルパイプラインの生成と初期化
	gpuParticlePipeline_ = std::make_unique<GpuParticlePipeline>();
	gpuParticlePipeline_->Initialize();

	// GPUパーティクルバッファの生成と初期化
	gpuParticleBuffers_ = std::make_unique<GpuParticleBuffers>();
	gpuParticleBuffers_->Initialize(camera_);

	// GPUパーティクルレンダラーの生成と初期化
	gpuParticleRenderer_ = std::make_unique<GpuParticleRenderer>();
	gpuParticleRenderer_->Initialize(gpuParticlePipeline_.get(), gpuParticleBuffers_.get());

	// ディスパッチ処理
	Dispatch();
}

/// -------------------------------------------------------------
///				　　　			更新処理
/// -------------------------------------------------------------
void GpuParticleManager::Update()
{
	// GPUパーティクルバッファの更新処理
	gpuParticleBuffers_->Update();

	// 更新用ディスパッチ処理
	DispatchUpdate();

	// エミット用ディスパッチ処理
	DispatchEmit();
}

/// -------------------------------------------------------------
///				　　　			描画処理
/// -------------------------------------------------------------
void GpuParticleManager::Draw()
{
	// GPUパーティクルレンダラーの描画処理
	gpuParticleRenderer_->Draw(GpuParticleBuffers::GetMaxParticles());
}

/// -------------------------------------------------------------
///				　　　		エミッター作成処理
/// -------------------------------------------------------------
GpuParticleEmitter& GpuParticleManager::CreateEmitter(const std::string& name, const GpuEmitterDesc& desc)
{
	auto emitter = std::make_unique<GpuParticleEmitter>(name, desc);
	auto& ref = *emitter;
	emitters_[name] = std::move(emitter);
	return ref;
}

/// -------------------------------------------------------------
///				　　	指定位置でパーティクルを出す
/// -------------------------------------------------------------
void GpuParticleManager::Emit(const std::string& name, const Vector3& position)
{
	auto it = emitters_.find(name);
	if (it == emitters_.end()) return;

	// 1) EmitterCB をこのエミッタ用に更新
	it->second->Emit(gpuParticleBuffers_.get(), position);

	// 2) その設定で Emit CS を一回 Dispatch
	DispatchEmit();
}

/// -------------------------------------------------------------
///				　　　		ディスパッチ処理
/// -------------------------------------------------------------
void GpuParticleManager::Dispatch()
{
	auto* dxCommon = DirectXCommon::GetInstance();
	auto* commandList = dxCommon->GetCommandManager()->GetCommandList();

	// リソースバリアの設定：UAV書き込み可能状態へ
	dxCommon->ResourceTransition(gpuParticleBuffers_->GetParticleBuffer(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	// パーティクルバッファのUAVをセット
	UAVManager::GetInstance()->PreDispatch();

	// パイプラインの設定
	commandList->SetComputeRootSignature(gpuParticlePipeline_->GetCsRootSignature());
	commandList->SetPipelineState(gpuParticlePipeline_->GetCsPSO());

	// パーティクルバッファUAVをセット
	commandList->SetComputeRootDescriptorTable(1, UAVManager::GetInstance()->GetGPUDescriptorHandle(gpuParticleBuffers_->GetParticleUavIndex())); // u0 : UAV

	const UINT maxParticles = GpuParticleBuffers::GetMaxParticles();
	const UINT threadCount = 1024; // [numthreads(1024,1,1)] を想定
	const UINT groupCountX = (maxParticles + threadCount - 1) / threadCount;
	commandList->Dispatch(groupCountX, 1, 1);

	// バリア処理
	dxCommon->ResourceTransition(gpuParticleBuffers_->GetParticleBuffer(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

/// -------------------------------------------------------------
///				　　	ディスパッチ処理（エミット用）
/// -------------------------------------------------------------
void GpuParticleManager::DispatchEmit()
{
	auto* dxCommon = DirectXCommon::GetInstance();
	auto* commandList = dxCommon->GetCommandManager()->GetCommandList();

	// リソースバリアの設定：UAV書き込み可能状態へ
	dxCommon->ResourceTransition(gpuParticleBuffers_->GetParticleBuffer(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	// パーティクルバッファのUAVをセット
	UAVManager::GetInstance()->PreDispatch();

	// パイプラインの設定
	commandList->SetComputeRootSignature(gpuParticlePipeline_->GetCsRootSignature());
	commandList->SetPipelineState(gpuParticlePipeline_->GetCsEmitPSO());

	// パーティクルバッファUAVをセット
	commandList->SetComputeRootDescriptorTable(1, UAVManager::GetInstance()->GetGPUDescriptorHandle(gpuParticleBuffers_->GetParticleUavIndex())); // u0 : UAV

	// CBVをセット (エミッターバッファ)
	commandList->SetComputeRootConstantBufferView(2, gpuParticleBuffers_->GetEmitterBuffer()->GetGPUVirtualAddress());

	// CBVをセット (時間計測用バッファ)
	commandList->SetComputeRootConstantBufferView(3, gpuParticleBuffers_->GetPerFrameBuffer()->GetGPUVirtualAddress());

	const UINT groupCountX = 1; // [numthreads(1,1,1)] を想定
	commandList->Dispatch(groupCountX, 1, 1);

	// バリア処理
	dxCommon->ResourceTransition(gpuParticleBuffers_->GetParticleBuffer(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

/// -------------------------------------------------------------
///				　　	ディスパッチ処理（更新用）
/// -------------------------------------------------------------
void GpuParticleManager::DispatchUpdate()
{
	auto* dxCommon = DirectXCommon::GetInstance();
	auto* commandList = dxCommon->GetCommandManager()->GetCommandList();

	// リソースバリアの設定：UAV書き込み可能状態へ
	dxCommon->ResourceTransition(gpuParticleBuffers_->GetParticleBuffer(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	// パーティクルバッファのUAVをセット
	UAVManager::GetInstance()->PreDispatch();

	// パイプラインの設定
	commandList->SetComputeRootSignature(gpuParticlePipeline_->GetCsRootSignature());
	commandList->SetPipelineState(gpuParticlePipeline_->GetCsUpdatePSO());

	// パーティクルバッファUAVをセット
	commandList->SetComputeRootDescriptorTable(1, UAVManager::GetInstance()->GetGPUDescriptorHandle(gpuParticleBuffers_->GetParticleUavIndex())); // u0 : UAV

	// CBVをセット (時間計測用バッファ)
	commandList->SetComputeRootConstantBufferView(3, gpuParticleBuffers_->GetPerFrameBuffer()->GetGPUVirtualAddress());

	const UINT maxParticles = GpuParticleBuffers::GetMaxParticles();
	const UINT threadCount = 1024; // [numthreads(1024,1,1)] を想定
	const UINT groupCountX = (maxParticles + threadCount - 1) / threadCount;

	commandList->Dispatch(groupCountX, 1, 1);

	// バリア処理
	dxCommon->ResourceTransition(gpuParticleBuffers_->GetParticleBuffer(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}
