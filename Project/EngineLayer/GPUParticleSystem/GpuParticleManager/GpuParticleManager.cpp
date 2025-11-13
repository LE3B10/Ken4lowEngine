#include "GpuParticleManager.h"
#include "Object3DCommon.h"
#include <DebugCamera.h>
#include <DirectXCommon.h>
#include <SRVManager.h>
#include <UAVManager.h>

#include "GpuParticleEmitter.h"
#include "GpuParticleEmitterData.h"

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
void GpuParticleManager::Update(float deltaTime)
{
	// GPUパーティクルバッファの更新処理
	gpuParticleBuffers_->Update(deltaTime);

	// 更新用ディスパッチ処理
	DispatchUpdate();

	GpuEmitterCBData* emitterCBData = gpuParticleBuffers_->GetEmitterCBData();

	for (auto& [name, emitter] : emitters_)
	{
		// エミッターのCBデータを構築
		if (emitter->BuildCB(*emitterCBData, deltaTime))
		{
			// エミット用ディスパッチ処理
			DispatchEmit();
		}
	}
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
///				　　　		エミッター作成
/// -------------------------------------------------------------
GpuParticleEmitter* GpuParticleManager::CreateEmitter(const std::string& name, const GpuParticleEmitter::EmitterInfo& info)
{
	auto it = emitters_.find(name); // すでに同じ名前のエミッターが存在するか確認
	if (it != emitters_.end())
	{
		// すでに存在する場合はnullptrを返す
		return nullptr;
	}

	// 新規作成
	auto emitter = std::make_unique<GpuParticleEmitter>(name, info);
	auto* emitterPtr = emitter.get();
	emitters_[name] = std::move(emitter);

	// 作成したエミッターのポインタを返す
	return emitterPtr;
}

/// -------------------------------------------------------------
///				　　　		エミッター取得
/// -------------------------------------------------------------
GpuParticleEmitter* GpuParticleManager::GetEmitter(const std::string& name)
{
	auto it = emitters_.find(name); // 指定された名前のエミッターを検索

	// 見つかった場合はポインタを返す
	if (it != emitters_.end())
	{
		return it->second.get(); // エミッターのポインタを返す
	}

	// 見つからなかった場合はnullptrを返す
	return nullptr;
}

/// -------------------------------------------------------------
///				　　　		名前指定でバースト
/// -------------------------------------------------------------
void GpuParticleManager::BurstEmitter(const std::string& name, uint32_t count)
{
	// 指定された名前のエミッターを取得
	if (auto* it = GetEmitter(name))
	{
		// エミット要求を出す
		it->RequestEmit(count);
	}
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
	commandList->SetComputeRootConstantBufferView(2, gpuParticleBuffers_->GetEmitterBuffer()->GetGPUVirtualAddress());
	commandList->SetComputeRootConstantBufferView(3, gpuParticleBuffers_->GetPerFrameBuffer()->GetGPUVirtualAddress());

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
