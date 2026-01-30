#include "GpuParticleManager.h"
#include "Object3DCommon.h"
#include <DebugCamera.h>
#include <DirectXCommon.h>
#include <SRVManager.h>
#include <UAVManager.h>

#include "GpuParticleEmitter.h"
#include "GpuParticleEmitterData.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI


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

void GpuParticleManager::Finalize()
{
	emitters_.clear();
	gpuParticleRenderer_.reset();
	gpuParticleBuffers_.reset();
	gpuParticlePipeline_.reset();

	camera_ = nullptr;
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

	uint32_t slot = 0;

	for (auto& [name, emitter] : emitters_)
	{
		// slotの場所に書く
		auto* cb = gpuParticleBuffers_->GetEmitterCBData(slot);

		if (emitter->BuildCB(*cb, deltaTime))
		{
			DispatchEmit(gpuParticleBuffers_->GetEmitterCBAddress(slot));
		}

		slot++;
	}
}

/// -------------------------------------------------------------
///				　　　			描画処理
/// -------------------------------------------------------------
void GpuParticleManager::Draw()
{
	const UINT instanceCount = GpuParticleBuffers::GetMaxParticles();

	uint32_t drawSlot = 0;

	for (auto& [name, emitter] : emitters_)
	{
		const auto& info = emitter->GetInfo();

		gpuParticleRenderer_->SetTextureFilePath(info.textureFilePath);

		// emitter->GetDrawType() を使う（drawType=0ならtype）
		gpuParticleRenderer_->SetDrawType(emitter->GetDrawType(), drawSlot);

		// 通常のパーティクルとして描画
		gpuParticleRenderer_->Draw(instanceCount, drawSlot);

		++drawSlot;
	}
}

void GpuParticleManager::DrawImGui()
{
#ifdef USE_IMGUI

	ImGui::Begin("GPU Particle");
	// 選択中のエミッター名
	static const char* kTypeNames[] =
	{
		"Default",            // 0
		"MuzzleFlash",        // 1
		"BulletTracer",       // 2
		"HitSpark",           // 3
		"Blood",              // 4
		"Impact_Dust",        // 5
		"Impact_Metal",       // 6
		"Impact_Wood",        // 7
		"Explosion_Fire",     // 8
		"Explosion_Smoke",    // 9
		"Foot_Dust",          // 10
		"Env_Dust",           // 11
		"Pickup_Glow",        // 12
		"Skill_Effect",       // 13
		"Boss_Appear_Dust",   // 14
		"Boss_Aura",          // 15
		"Boss_Rush_Trail",    // 16
		"Shockwave",          // 17
		"Boss_Spin_Slash",    // 18
		"Boss_Death_Soul",    // 19
		"Boss_Debris_Dust",   // 20
		"Heal_Effect",        // 21
	};

	auto ToU32 = [](auto e) {return static_cast<uint32_t>(e); };

	static std::string selected;

	// ---- エミッター一覧 ----
	if (ImGui::BeginListBox("Emitters"))
	{
		for (auto& [name, emitter] : emitters_)
		{
			bool isSelected = (selected == name);
			if (ImGui::Selectable(name.c_str(), isSelected)) { selected = name; }
		}
		ImGui::EndListBox();
	}

	// ---- 選択エミッター編集 ----
	if (!selected.empty())
	{
		if (auto* e = GetEmitter(selected))
		{
			auto& info = e->GetInfoMutable();

			ImGui::SeparatorText("Emitter Params");

			// 位置
			{
				Vector3 pos = e->GetPosition();
				float p[3] = { pos.x, pos.y, pos.z };
				if (ImGui::DragFloat3("Position", p, 0.01f))
				{
					e->SetPosition({ p[0], p[1], p[2] });
				}
			}

			// 発生設定
			ImGui::DragFloat("Radius", &info.radius, 0.01f, 0.0f, 100.0f);

			int loopCount = (int)info.loopCount;
			if (ImGui::DragInt("Loop Count", &loopCount, 1, 0, 100000))
			{
				if (loopCount < 0) loopCount = 0;
				info.loopCount = (uint32_t)loopCount;
			}

			ImGui::DragFloat("Loop Frequency (sec)", &info.loopFrequency, 0.01f, 0.0f, 10.0f);

			// DrawType（0なら type を使う設計）
			{
				int drawType = (int)info.drawType;
				if (ImGui::InputInt("DrawType (0=Use Type)", &drawType))
				{
					if (drawType < 0) drawType = 0;
					info.drawType = (uint32_t)drawType;
				}
			}

			// ---- Type（= gEmitter.type）----
			{
				int typeIndex = (int)info.type;
				const int typeCount = (int)IM_ARRAYSIZE(kTypeNames);
				if (typeIndex < 0) typeIndex = 0;
				if (typeIndex >= typeCount) typeIndex = 0;

				if (ImGui::Combo("Particle Type", &typeIndex, kTypeNames, typeCount))
				{
					info.type = (GpuParticleType)typeIndex;
				}

				// shader側の DecideRenderKind 相当（確認用）
				const uint32_t t = (uint32_t)typeIndex;
				const bool isRibbon =
					(t == 2 /*BulletTracer*/ || t == 16 /*Boss_Rush_Trail*/ || t == 18 /*Boss_Spin_Slash*/);
				ImGui::Text("Shader Kind (auto) : %s", isRibbon ? "RIBBON" : "SPRITE");
			}

			// ---- BillboardMode（フラグをbitで編集する）----
			{
				uint32_t bb = ToU32(info.billboardMode);

				bool bbCamera = (bb & ToU32(BillboardMode::Camera)) != 0;
				bool bbYAxis = (bb & ToU32(BillboardMode::YAxis)) != 0;

				// Ribbonフラグは基本「kind=RIBBONになった時にCSが付与」する設計なのでUIでは触らなくてOK
				// もし手動で試したいならチェックボックスを出してもOK
				bool bbRibbon = (bb & ToU32(BillboardMode::Ribbon)) != 0;

				bool changed = false;
				changed |= ImGui::Checkbox("BB: Camera", &bbCamera);
				changed |= ImGui::Checkbox("BB: YAxis", &bbYAxis);
				changed |= ImGui::Checkbox("BB: Ribbon (debug)", &bbRibbon);

				if (changed)
				{
					bb = 0;
					if (bbCamera) bb |= ToU32(BillboardMode::Camera);
					if (bbYAxis)  bb |= ToU32(BillboardMode::YAxis);
					if (bbRibbon) bb |= ToU32(BillboardMode::Ribbon);

					info.billboardMode = (BillboardMode)bb;
				}

				ImGui::Text("billboardMode flags = 0x%08X", bb);
			}

			// テクスチャパスは表示だけ（InputTextで編集したいならバッファ化が必要）
			ImGui::Text("Texture: %s", info.textureFilePath.c_str());

			// ---- その場バースト ----
			static int burstCount = 50;
			ImGui::DragInt("Burst Count", &burstCount, 1, 0, 100000);
			if (ImGui::Button("Burst"))
			{
				e->RequestEmit((uint32_t)burstCount);
			}
		}
	}

	ImGui::Separator();
	ImGui::Checkbox("Debug Camera", &isDebugCamera_);
	SetDebugCameraEnabled(isDebugCamera_);

	ImGui::End();
#endif // USE_IMGUI

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
void GpuParticleManager::DispatchEmit(D3D12_GPU_VIRTUAL_ADDRESS emitterCbAddr)
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
	commandList->SetComputeRootConstantBufferView(2, emitterCbAddr);

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
