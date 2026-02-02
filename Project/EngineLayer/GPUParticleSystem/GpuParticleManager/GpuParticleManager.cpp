#include "GpuParticleManager.h"
#include "Object3DCommon.h"
#include <DebugCamera.h>
#include <DirectXCommon.h>
#include <SRVManager.h>
#include <UAVManager.h>

#include "GpuParticleEmitter.h"
#include "GpuParticleEmitterData.h"

#include "AssimpLoader.h"
#include "ResourceManager.h"
#include <TextureManager.h>
#include <d3dx12.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI
#include <cstdint>


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

	// ★描画（GPUスプライト）用
	spritePipeline_ = std::make_unique<GpuParticleSpritePipeline>();
	spritePipeline_->Initialize();

	// ★計算（CS）用
	computePipeline_ = std::make_unique<GpuParticleComputePipeline>();
	computePipeline_->Initialize();

	// GPUパーティクルバッファの生成と初期化
	gpuParticleBuffers_ = std::make_unique<GpuParticleBuffers>();
	gpuParticleBuffers_->Initialize(camera_);

	// GPUパーティクルレンダラーの生成と初期化
	gpuParticleRenderer_ = std::make_unique<GpuParticleRenderer>();
	gpuParticleRenderer_->Initialize(spritePipeline_.get(), gpuParticleBuffers_.get());

	// メッシュパイプラインの生成と初期化
	meshPipeline_ = std::make_unique<GpuParticleMeshPipeline>();
	meshPipeline_->Initialize();

	// ディスパッチ処理
	Dispatch();
}

void GpuParticleManager::Finalize()
{
	emitters_.clear();
	
	ClearMeshAssets();
	
	meshPipeline_.reset();
	spritePipeline_.reset();
	computePipeline_.reset();
	gpuParticleBuffers_.reset();
	gpuParticleRenderer_.reset();

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

	// ------------------------------------------------------------
	//  UI用データ
	// ------------------------------------------------------------
	auto ToU32 = [](auto e) { return static_cast<uint32_t>(e); };

	// Sprite用：Default(0)を除いた21個
	static const char* kSpriteTypeNames[21] =
	{
		"MuzzleFlash",
		"BulletTracer",
		"HitSpark",
		"Blood",
		"Impact_Dust",
		"Impact_Metal",
		"Impact_Wood",
		"Explosion_Fire",
		"Explosion_Smoke",
		"Foot_Dust",
		"Env_Dust",
		"Pickup_Glow",
		"Skill_Effect",
		"Boss_Appear_Dust",
		"Boss_Aura",
		"Boss_Rush_Trail",
		"Shockwave",
		"Boss_Spin_Slash",
		"Boss_Death_Soul",
		"Boss_Debris_Dust",
		"Heal_Effect",
	};
	static const GpuParticleType kSpriteTypeValues[21] =
	{
		GpuParticleType::MuzzleFlash,
		GpuParticleType::BulletTracer,
		GpuParticleType::HitSpark,
		GpuParticleType::Blood,
		GpuParticleType::Impact_Dust,
		GpuParticleType::Impact_Metal,
		GpuParticleType::Impact_Wood,
		GpuParticleType::Explosion_Fire,
		GpuParticleType::Explosion_Smoke,
		GpuParticleType::Foot_Dust,
		GpuParticleType::Env_Dust,
		GpuParticleType::Pickup_Glow,
		GpuParticleType::Skill_Effect,
		GpuParticleType::Boss_Appear_Dust,
		GpuParticleType::Boss_Aura,
		GpuParticleType::Boss_Rush_Trail,
		GpuParticleType::Shockwave,
		GpuParticleType::Boss_Spin_Slash,
		GpuParticleType::Boss_Death_Soul,
		GpuParticleType::Boss_Debris_Dust,
		GpuParticleType::Heal_Effect,
	};

	// Ribbon用
	static const char* kRibbonTypeNames[] =
	{
		"Ribbon: BulletTracer",
		"Ribbon: BossRushTrail",
		"Ribbon: BossSpinSlash",
	};

	static std::string selected;
	static std::string lastSelected;
	static char textureBuf[256] = {};

	ImGui::Begin("GPU Particle");

	ImGui::SeparatorText("Mesh Assets");

	static char meshModelPath[256] = "cube.gltf"; // Resources/Models/ 以下
	static int  meshBaseId = 1000;

	ImGui::InputText("Model (Models/..)", meshModelPath, IM_ARRAYSIZE(meshModelPath));
	ImGui::DragInt("Base MeshId", &meshBaseId, 1, 0, 1000000);

	if (ImGui::Button("Load Model & Register MeshAssets"))
	{
		LoadMeshAssetsFromAssimp((uint32_t)meshBaseId, meshModelPath, /*loadTextures=*/true);
	}

	ImGui::Text("Registered MeshAssets: %d", (int)meshAssets_.size());
	if (ImGui::TreeNode("MeshAsset List"))
	{
		for (auto& [id, a] : meshAssets_)
		{
			ImGui::BulletText("id=%u  idx=%u  tex=%s",
				id, a.indexCount, a.textureFilePath.c_str());
		}
		ImGui::TreePop();
	}

	// ------------------------------------------------------------
	//  Create Emitter（複数射出の入口：複数Emitter作成）
	// ------------------------------------------------------------
	ImGui::SeparatorText("Create Emitter");
	static char newName[64] = "Emitter_0";
	static char newTexture[256] = "particle.png";
	static int newMode = (int)GpuParticleKind::Sprite;
	static int newSpriteTypeIndex = 0; // 21個の中のindex
	static int newRibbonTypeIndex = 0;
	static float newRadius = 0.0f;
	static int newLoopCount = 0;
	static float newLoopFrequency = 0.0f;

	ImGui::InputText("Name", newName, IM_ARRAYSIZE(newName));
	ImGui::InputText("Texture", newTexture, IM_ARRAYSIZE(newTexture));
	ImGui::DragFloat("Radius", &newRadius, 0.01f, 0.0f, 100.0f);
	ImGui::DragInt("Loop Count", &newLoopCount, 1, 0, 100000);
	ImGui::DragFloat("Loop Frequency (sec)", &newLoopFrequency, 0.01f, 0.0f, 10.0f);

	const char* kModeNames[] = { "GPU Sprite", "Mesh", "Ribbon", "Beam" };
	ImGui::Combo("Mode", &newMode, kModeNames, IM_ARRAYSIZE(kModeNames));

	if (newMode == (int)GpuParticleKind::Sprite)
	{
		ImGui::Combo("Sprite Type (21)", &newSpriteTypeIndex, kSpriteTypeNames, IM_ARRAYSIZE(kSpriteTypeNames));
	}
	else if (newMode == (int)GpuParticleKind::Ribbon)
	{
		ImGui::Combo("Ribbon Type", &newRibbonTypeIndex, kRibbonTypeNames, IM_ARRAYSIZE(kRibbonTypeNames));
	}
	else
	{
		ImGui::TextDisabled("This mode is not fully implemented yet.");
	}

	if (ImGui::Button("Create"))
	{
		GpuParticleEmitter::EmitterInfo ci{};
		ci.textureFilePath = newTexture;
		ci.radius = newRadius;
		ci.loopCount = (newLoopCount < 0) ? 0u : (uint32_t)newLoopCount;
		ci.loopFrequency = (newLoopFrequency < 0.0f) ? 0.0f : newLoopFrequency;
		ci.drawType = 0;
		ci.kind = (GpuParticleKind)newMode;
		ci.billboardFlags = BillboardMode::Camera;

		// 初期Type（Modeで完全分離）
		ci.spriteType = kSpriteTypeValues[(newSpriteTypeIndex < 0) ? 0 : (newSpriteTypeIndex % 21)];
		ci.ribbonType = (GpuRibbonType)((newRibbonTypeIndex < 0) ? 0 : (newRibbonTypeIndex % 3));

		if (CreateEmitter(newName, ci))
		{
			selected = newName;
			lastSelected.clear();
		}
	}

	// ------------------------------------------------------------
	//  Emitter一覧
	// ------------------------------------------------------------
	ImGui::SeparatorText("Emitters");
	if (ImGui::BeginListBox("Emitters"))
	{
		for (auto& [name, emitter] : emitters_)
		{
			bool isSelected = (selected == name);
			if (ImGui::Selectable(name.c_str(), isSelected))
			{
				selected = name;
			}
		}
		ImGui::EndListBox();
	}

	// ------------------------------------------------------------
	//  選択エミッター編集
	// ------------------------------------------------------------
	if (!selected.empty())
	{
		if (auto* e = GetEmitter(selected))
		{
			auto& info = e->GetInfoMutable();

			// 選択が切り替わったらテクスチャ編集バッファを同期
			if (lastSelected != selected)
			{
				lastSelected = selected;
				std::snprintf(textureBuf, sizeof(textureBuf), "%s", info.textureFilePath.c_str());
			}

			ImGui::SeparatorText("Selected Emitter");
			ImGui::Text("Name: %s", selected.c_str());
			ImGui::SameLine();
			if (ImGui::Button("Delete"))
			{
				emitters_.erase(selected);
				selected.clear();
				lastSelected.clear();
				ImGui::End();
				return;
			}

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
			ImGui::DragFloat("Radius##Selected", &info.radius, 0.01f, 0.0f, 100.0f);

			int loopCount = (int)info.loopCount;
			if (ImGui::DragInt("Loop Count##Selected", &loopCount, 1, 0, 100000))
			{
				if (loopCount < 0) loopCount = 0;
				info.loopCount = (uint32_t)loopCount;
			}
			ImGui::DragFloat("Loop Frequency (sec)##Selected", &info.loopFrequency, 0.01f, 0.0f, 10.0f);

			// DrawType（0なら effectiveType を使う設計）
			{
				int drawType = (int)info.drawType;
				if (ImGui::InputInt("DrawType (0=Use EffectiveType)", &drawType))
				{
					if (drawType < 0) drawType = 0;
					info.drawType = (uint32_t)drawType;
				}
			}

			// ---- Mode（kind）----
			{
				int mode = (int)info.kind;
				if (ImGui::Combo("Mode##Selected", &mode, kModeNames, IM_ARRAYSIZE(kModeNames)))
				{
					if (mode < 0) mode = 0;
					if (mode > 3) mode = 0;
					info.kind = (GpuParticleKind)mode;

					// Meshに切り替えたらビルボード無効に寄せる
					if (info.kind == GpuParticleKind::Mesh)
					{
						info.billboardFlags = BillboardMode::None;
					}
					else if (info.billboardFlags == BillboardMode::None)
					{
						info.billboardFlags = BillboardMode::Camera;
					}
				}
			}

			// ---- Type（Modeで完全分離）----
			if (info.kind == GpuParticleKind::Sprite)
			{
				int spriteIndex = 0;
				for (int i = 0; i < 21; ++i)
				{
					if (info.spriteType == kSpriteTypeValues[i]) { spriteIndex = i; break; }
				}
				if (ImGui::Combo("Sprite Type (21)##Selected", &spriteIndex, kSpriteTypeNames, 21))
				{
					info.spriteType = kSpriteTypeValues[spriteIndex];
				}
			}
			else if (info.kind == GpuParticleKind::Ribbon)
			{
				int ribbonIndex = (int)info.ribbonType;
				if (ImGui::Combo("Ribbon Type##Selected", &ribbonIndex, kRibbonTypeNames, IM_ARRAYSIZE(kRibbonTypeNames)))
				{
					if (ribbonIndex < 0) ribbonIndex = 0;
					ribbonIndex %= 3;
					info.ribbonType = (GpuRibbonType)ribbonIndex;
				}
			}
			else
			{
				ImGui::TextDisabled("Type UI for this mode is not implemented yet.");
			}

			// ---- Billboard flags（下位16bit）----
			{
				uint32_t flags = ToU32(info.billboardFlags);
				bool bbCamera = (flags & ToU32(BillboardMode::Camera)) != 0;
				bool bbYAxis = (flags & ToU32(BillboardMode::YAxis)) != 0;

				bool changed = false;
				if (info.kind == GpuParticleKind::Mesh)
				{
					ImGui::TextDisabled("Billboard flags are disabled in Mesh mode.");
				}
				else
				{
					changed |= ImGui::Checkbox("BB: Camera", &bbCamera);
					changed |= ImGui::Checkbox("BB: YAxis", &bbYAxis);
				}

				if (changed)
				{
					flags = 0;
					if (bbCamera) flags |= ToU32(BillboardMode::Camera);
					if (bbYAxis)  flags |= ToU32(BillboardMode::YAxis);
					info.billboardFlags = (BillboardMode)flags;
				}
			}

			// ---- Texture ----
			ImGui::InputText("Texture##Selected", textureBuf, IM_ARRAYSIZE(textureBuf));
			ImGui::SameLine();
			if (ImGui::Button("Apply Texture"))
			{
				info.textureFilePath = textureBuf;
			}

			// ---- Debug info（GPUへ送る値を表示）----
			uint32_t effectiveType = 0;
			if (info.kind == GpuParticleKind::Sprite)
				effectiveType = (uint32_t)info.spriteType;
			else if (info.kind == GpuParticleKind::Ribbon)
				effectiveType = (uint32_t)ToGpuParticleType(info.ribbonType);
			else
				effectiveType = (uint32_t)info.spriteType;

			const uint32_t packed = PackBillboardMode(info.kind, (uint32_t)info.billboardFlags);
			ImGui::Text("EffectiveType (sent to GPU): %u", effectiveType);
			ImGui::Text("Packed billboardMode: 0x%08X", packed);

			// ---- Burst（複数射出：同フレームに何回でも合算）----
			ImGui::SeparatorText("Burst");
			static int burstCount = 50;
			static int burstRepeat = 1;
			ImGui::DragInt("Burst Count", &burstCount, 1, 0, 100000);
			ImGui::DragInt("Burst Repeat", &burstRepeat, 1, 1, 1000);

			if (ImGui::Button("Burst"))
			{
				e->RequestEmit((uint32_t)((burstCount < 0) ? 0 : burstCount));
			}
			ImGui::SameLine();
			if (ImGui::Button("Burst x Repeat"))
			{
				uint32_t c = (uint32_t)((burstCount < 0) ? 0 : burstCount);
				uint32_t r = (uint32_t)((burstRepeat < 1) ? 1 : burstRepeat);
				e->RequestEmit(c * r);
			}
		}
	}

	ImGui::Separator();
	ImGui::Checkbox("Debug Camera", &isDebugCamera_);
	SetDebugCameraEnabled(isDebugCamera_);

	ImGui::End();

#endif // USE_IMGUI
}

bool GpuParticleManager::RegisterMeshAsset(uint32_t meshId, MeshParticleAsset asset, bool overwrite)
{
	if (!overwrite && meshAssets_.contains(meshId))
	{
		return false;
	}
	meshAssets_[meshId] = std::move(asset);
	return true;
}

const MeshParticleAsset* GpuParticleManager::FindMeshAsset(uint32_t meshId) const
{
	auto it = meshAssets_.find(meshId);
	if (it == meshAssets_.end()) { return nullptr; }
	return &it->second;
}

bool GpuParticleManager::LoadMeshAssetsFromAssimp(uint32_t baseMeshId, const std::string& modelFilePath, bool loadTextures)
{
	// AssimpLoader は内部で "Resources/Models/" を付与する仕様
	ModelData model = AssimpLoader::LoadModel(modelFilePath);

	if (model.subMeshes.empty())
	{
		return false;
	}

	for (uint32_t i = 0; i < static_cast<uint32_t>(model.subMeshes.size()); ++i)
	{
		const SubMesh& sub = model.subMeshes[i];

		// 空メッシュは無視
		if (sub.vertices.empty() || sub.indices.empty())
		{
			continue;
		}

		MeshParticleAsset asset = CreateMeshAssetFromSubMesh(sub, loadTextures);

		// submeshごとに連番で登録
		const uint32_t meshId = baseMeshId + i;
		RegisterMeshAsset(meshId, std::move(asset), /*overwrite=*/true);
	}

	return true;
}

void GpuParticleManager::ClearMeshAssets()
{
	meshAssets_.clear();
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
	commandList->SetComputeRootSignature(computePipeline_->GetCsRootSignature());
	commandList->SetPipelineState(computePipeline_->GetCsPSO());

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
	commandList->SetComputeRootSignature(computePipeline_->GetCsRootSignature());
	commandList->SetPipelineState(computePipeline_->GetCsEmitPSO());

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
	commandList->SetComputeRootSignature(computePipeline_->GetCsRootSignature());
	commandList->SetPipelineState(computePipeline_->GetCsUpdatePSO());

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

MeshParticleAsset GpuParticleManager::CreateMeshAssetFromSubMesh(const SubMesh& subMesh, bool loadTexture)
{
	MeshParticleAsset out{};

	auto* dx = DirectXCommon::GetInstance();
	auto* device = dx->GetDevice();
	auto* cmdList = dx->GetCommandManager()->GetCommandList();

	// ----------------------------
	// VB 作成
	// ----------------------------
	const UINT vbStride = static_cast<UINT>(sizeof(VertexData));
	const UINT vbSize = static_cast<UINT>(subMesh.vertices.size() * sizeof(VertexData));

	out.vb = ResourceManager::CreateBufferResource(
		device,
		vbSize,
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_FLAG_NONE,
		D3D12_RESOURCE_STATE_COPY_DEST
	);

	ComPtr<ID3D12Resource> vbUpload = ResourceManager::CreateBufferResource(
		device,
		vbSize,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_FLAG_NONE,
		D3D12_RESOURCE_STATE_GENERIC_READ
	);

	// Uploadへコピー
	{
		void* mapped = nullptr;
		vbUpload->Map(0, nullptr, &mapped);
		std::memcpy(mapped, subMesh.vertices.data(), vbSize);
		vbUpload->Unmap(0, nullptr);
	}

	cmdList->CopyBufferRegion(out.vb.Get(), 0, vbUpload.Get(), 0, vbSize);

	// VB を使用可能ステートへ
	{
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = out.vb.Get();
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		cmdList->ResourceBarrier(1, &barrier);
	}

	// VBV
	out.vbv.BufferLocation = out.vb->GetGPUVirtualAddress();
	out.vbv.SizeInBytes = vbSize;
	out.vbv.StrideInBytes = vbStride;

	// ----------------------------
	// IB 作成
	// ----------------------------
	const UINT ibSize = static_cast<UINT>(subMesh.indices.size() * sizeof(uint32_t));

	out.ib = ResourceManager::CreateBufferResource(
		device,
		ibSize,
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_FLAG_NONE,
		D3D12_RESOURCE_STATE_COPY_DEST
	);

	ComPtr<ID3D12Resource> ibUpload = ResourceManager::CreateBufferResource(
		device,
		ibSize,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_FLAG_NONE,
		D3D12_RESOURCE_STATE_GENERIC_READ
	);

	{
		void* mapped = nullptr;
		ibUpload->Map(0, nullptr, &mapped);
		std::memcpy(mapped, subMesh.indices.data(), ibSize);
		ibUpload->Unmap(0, nullptr);
	}

	cmdList->CopyBufferRegion(out.ib.Get(), 0, ibUpload.Get(), 0, ibSize);

	// IB を使用可能ステートへ
	{
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = out.ib.Get();
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
		cmdList->ResourceBarrier(1, &barrier);
	}

	// IBV
	out.ibv.BufferLocation = out.ib->GetGPUVirtualAddress();
	out.ibv.SizeInBytes = ibSize;
	out.ibv.Format = DXGI_FORMAT_R32_UINT;
	out.indexCount = static_cast<uint32_t>(subMesh.indices.size());

	// コマンド実行＆待機（Upload破棄のため）
	dx->GetCommandManager()->ExecuteAndWait();

	// ----------------------------
	// Texture（任意）
	// ----------------------------
	out.textureFilePath = subMesh.material.textureFilePath;
	if (loadTexture && !out.textureFilePath.empty())
	{
		TextureManager::GetInstance()->LoadTexture(out.textureFilePath);
	}

	return out;
}
