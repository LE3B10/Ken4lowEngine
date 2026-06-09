#define NOMINMAX
#include "GpuParticleManager.h"
#include "Object3DCommon.h"
#include <DebugCamera.h>
#include <DirectXCommon.h>
#include <SRVManager.h>
#include <UAVManager.h>

#include "GpuParticleEmitterPreset.h"
#include "GpuParticleEmitter.h"
#include "GpuParticleEmitterData.h"
#include "GpuParticleEmitterSerializer.h"

#include "AssimpLoader.h"
#include "ParameterManager.h"
#include "ResourceManager.h"
#include <TextureManager.h>
#include <d3dx12.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI
#include <cstdint>
#include <string>
#include <unordered_set>
#include <algorithm>

namespace Ken4lowEngine
{
	namespace
	{
		constexpr int kRibbonTypeCount = 3;

		uint32_t ClampU32(uint32_t value, uint32_t minValue, uint32_t maxValue)
		{
			return std::min(std::max(value, minValue), maxValue);
		}

		/// <summary>
		/// Jsonから不正値が入った場合も、GPUへ渡す列挙値が範囲外にならないように丸めます。
		/// </summary>
		GpuParticleKind ToSafeParticleKind(uint32_t value)
		{
			return static_cast<GpuParticleKind>(ClampU32(value, 0u, 3u));
		}

		/// <summary>
		/// SpriteTypeはCount未満に丸め、既存HLSL側の分岐外へ飛ばないようにします。
		/// </summary>
		GpuParticleType ToSafeSpriteType(uint32_t value)
		{
			const uint32_t maxValue = static_cast<uint32_t>(GpuParticleType::Count) - 1u;
			return static_cast<GpuParticleType>(ClampU32(value, 0u, maxValue));
		}

		/// <summary>
		/// BillboardModeは下位フラグだけを使うため、現在対応している値に丸めます。
		/// </summary>
		BillboardMode ToSafeBillboardMode(uint32_t value)
		{
			constexpr uint32_t kSupportedFlags =
				static_cast<uint32_t>(BillboardMode::Camera) |
				static_cast<uint32_t>(BillboardMode::YAxis) |
				static_cast<uint32_t>(BillboardMode::Ribbon);
			return static_cast<BillboardMode>(value & kSupportedFlags);
		}
	}

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

		// 描画（GPUスプライト）用
		spritePipeline_ = std::make_unique<GpuParticleSpritePipeline>();
		spritePipeline_->Initialize();

		// 計算（CS）用
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
		for (const auto& [name, emitter] : emitters_)
		{
			(void)name;
			if (emitter)
			{
				UnregisterEmitterParameters(*emitter);
			}
		}
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
			emitter->UpdateActivity(deltaTime);

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
		lastDrawCallCount_ = 0;
		std::unordered_set<std::string> drawnKeys;

		for (auto& [name, emitter] : emitters_)
		{
			(void)name;
			if (!emitter->HasActiveParticles())
			{
				continue;
			}

			const auto& info = emitter->GetInfo();
			const uint32_t drawType = emitter->GetDrawType();
			const std::string drawKey = info.textureFilePath + "#" + std::to_string(drawType);
			if (!drawnKeys.insert(drawKey).second)
			{
				continue;
			}

			gpuParticleRenderer_->SetTextureFilePath(info.textureFilePath);

			// emitter->GetDrawType() を使う（drawType=0ならtype）
			gpuParticleRenderer_->SetDrawType(drawType, drawSlot);

			// 同じ描画タイプは1回だけ描画し、寿命切れ後のフルインスタンスDrawも抑える。
			gpuParticleRenderer_->Draw(instanceCount, drawSlot);
			++lastDrawCallCount_;
			++drawSlot;
		}
	}

	void GpuParticleManager::DrawImGui()
	{
#ifdef USE_IMGUI

		const uint32_t spritePresetCount = GpuParticleEmitterPresetTable::GetSpritePresetCount();

		static char newName[128] = "NewEmitter";
		static char newTexture[128] = "Effects/white.dds";
		static bool createEmitterDefaultsInitialized = false;

		static int newMode = static_cast<int>(GpuParticleKind::Sprite);
		static int newSpriteTypeIndex = 0;
		static int newRibbonTypeIndex = 0;

		static float newRadius = 0.25f;
		static int newLoopCount = 0;
		static float newLoopFrequency = 0.0f;

		static bool newEmitterPresetInitialized = false;

		static bool createAndSave = false;

		auto ApplyCreateEmitterPreset = [&](int spriteIndex)
			{
				const GpuParticleType type =
					GpuParticleEmitterPresetTable::GetSpriteTypeByIndex(static_cast<uint32_t>(spriteIndex));

				const auto info = GpuParticleEmitterPresetTable::MakeEmitterInfo(type);

				std::snprintf(newTexture, sizeof(newTexture), "%s", info.textureFilePath.c_str());
				newRadius = info.radius;
				newLoopCount = static_cast<int>(info.loopCount);
				newLoopFrequency = info.loopFrequency;
			};

		if (!newEmitterPresetInitialized)
		{
			ApplyCreateEmitterPreset(newSpriteTypeIndex);
			newEmitterPresetInitialized = true;
		}

		static const char* kRibbonTypeNames[] =
		{
			"Ribbon: Trail",
		};

		static std::string selected;

		ImGui::Begin("GPU Particle");

		ImGui::SeparatorText("ParameterManager");
		ImGui::TextDisabled("Emitter settings are edited/saved in the Parameters window.");

		ImGui::SeparatorText("Mesh Assets");

		static char meshModelPath[256] = {};
		static bool meshModelPathInitialized = false;
		if (!meshModelPathInitialized)
		{
			std::snprintf(meshModelPath, sizeof(meshModelPath), "%s", GpuParticleDebugPresets::kDefaultMeshModelPath);
			meshModelPathInitialized = true;
		}

		static int meshBaseId = GpuParticleDebugPresets::kDefaultMeshBaseId;

		ImGui::InputText("Model (Models/..)", meshModelPath, IM_ARRAYSIZE(meshModelPath));
		ImGui::DragInt(
			"Base MeshId",
			&meshBaseId,
			GpuParticleDebugPresets::kMeshBaseIdStep,
			GpuParticleDebugPresets::kMinMeshBaseId,
			GpuParticleDebugPresets::kMaxMeshBaseId);

		if (ImGui::Button("Load Model & Register MeshAssets"))
		{
			LoadMeshAssetsFromAssimp(static_cast<uint32_t>(meshBaseId), meshModelPath, true);
		}

		ImGui::Text("Registered MeshAssets: %d", static_cast<int>(meshAssets_.size()));
		if (ImGui::TreeNode("MeshAsset List"))
		{
			for (auto& [id, a] : meshAssets_)
			{
				ImGui::BulletText("id=%u  idx=%u  tex=%s",
					id, a.indexCount, a.textureFilePath.c_str());
			}
			ImGui::TreePop();
		}

		ImGui::SeparatorText("Create Emitter");

		if (!createEmitterDefaultsInitialized)
		{
			std::snprintf(newName, sizeof(newName), "%s", GpuParticleDebugPresets::kDefaultEmitterName);
			std::snprintf(newTexture, sizeof(newTexture), "%s", GpuParticleDebugPresets::kDefaultEmitterTexture);
			createEmitterDefaultsInitialized = true;
		}

		ImGui::InputText("Name", newName, IM_ARRAYSIZE(newName));

		const char* kModeNames[] = { "GPU Sprite", "Mesh", "Ribbon", "Beam" };
		if (ImGui::Combo("Mode", &newMode, kModeNames, IM_ARRAYSIZE(kModeNames)))
		{
			if (newMode == static_cast<int>(GpuParticleKind::Sprite))
			{
				ApplyCreateEmitterPreset(newSpriteTypeIndex);
			}
		}

		if (newMode == static_cast<int>(GpuParticleKind::Sprite))
		{
			const GpuParticleType previewType =
				GpuParticleEmitterPresetTable::GetSpriteTypeByIndex(static_cast<uint32_t>(newSpriteTypeIndex));

			const char* previewName =
				GpuParticleEmitterPresetTable::GetSpriteDisplayName(previewType);

			if (ImGui::BeginCombo("Sprite Type", previewName))
			{
				for (uint32_t i = 0; i < spritePresetCount; ++i)
				{
					const GpuParticleType type =
						GpuParticleEmitterPresetTable::GetSpriteTypeByIndex(i);

					const bool isSelected = (newSpriteTypeIndex == static_cast<int>(i));

					if (ImGui::Selectable(
						GpuParticleEmitterPresetTable::GetSpriteDisplayName(type),
						isSelected))
					{
						newSpriteTypeIndex = static_cast<int>(i);
						ApplyCreateEmitterPreset(newSpriteTypeIndex);
					}

					if (isSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();
			if (ImGui::Button("Reset To Sprite Preset"))
			{
				ApplyCreateEmitterPreset(newSpriteTypeIndex);
			}
		}
		else if (newMode == static_cast<int>(GpuParticleKind::Ribbon))
		{
			ImGui::Combo("Ribbon Type", &newRibbonTypeIndex, kRibbonTypeNames, IM_ARRAYSIZE(kRibbonTypeNames));
		}
		else
		{
			ImGui::TextDisabled("This mode is not fully implemented yet.");
		}

		ImGui::InputText("Texture", newTexture, IM_ARRAYSIZE(newTexture));
		ImGui::DragFloat(
			"Radius",
			&newRadius,
			GpuParticleDebugPresets::kRadiusDragSpeed,
			GpuParticleDebugPresets::kMinRadius,
			GpuParticleDebugPresets::kMaxRadius);
		ImGui::DragInt(
			"Loop Count",
			&newLoopCount,
			GpuParticleDebugPresets::kLoopCountStep,
			GpuParticleDebugPresets::kMinLoopCount,
			GpuParticleDebugPresets::kMaxLoopCount);
		ImGui::DragFloat(
			"Loop Frequency (sec)",
			&newLoopFrequency,
			GpuParticleDebugPresets::kLoopFrequencyDragSpeed,
			GpuParticleDebugPresets::kMinLoopFrequency,
			GpuParticleDebugPresets::kMaxLoopFrequency);

		ImGui::Checkbox("Create And Save", &createAndSave);

		if (ImGui::Button("Create"))
		{
			GpuParticleEmitter::EmitterInfo ci{};

			if (newMode == static_cast<int>(GpuParticleKind::Sprite))
			{
				const GpuParticleType spriteType =
					GpuParticleEmitterPresetTable::GetSpriteTypeByIndex(
						static_cast<uint32_t>(newSpriteTypeIndex));

				ci = GpuParticleEmitterPresetTable::MakeEmitterInfo(spriteType);
				ci.textureFilePath = newTexture;
				ci.radius = newRadius;
				ci.loopCount = static_cast<uint32_t>(std::max(newLoopCount, 0));
				ci.loopFrequency = std::max(newLoopFrequency, 0.0f);
				ci.kind = GpuParticleKind::Sprite;
				ci.spriteType = spriteType;
			}
			else
			{
				ci.textureFilePath = newTexture;
				ci.radius = newRadius;
				ci.loopCount = static_cast<uint32_t>(std::max(newLoopCount, 0));
				ci.loopFrequency = std::max(newLoopFrequency, 0.0f);
				ci.drawType = 0;
				ci.kind = static_cast<GpuParticleKind>(newMode);
				ci.billboardFlags = BillboardMode::Camera;
				ci.ribbonType = static_cast<GpuRibbonType>((newRibbonTypeIndex < 0) ? 0 : (newRibbonTypeIndex % kRibbonTypeCount));
				ci.spriteType = GpuParticleType::Default;
			}

			if (GpuParticleEmitter* created = CreateEmitter(newName, ci))
			{
				selected = newName;

				if (createAndSave)
				{
					ParameterManager::GetInstance()->SaveFile(BuildEmitterParameterGroupName(created->GetName()));
				}
			}
		}

		ImGui::SeparatorText("Emitters");
		if (ImGui::BeginListBox("Emitters"))
		{
			std::vector<std::string> emitterNames;
			emitterNames.reserve(emitters_.size());

			for (const auto& [name, emitter] : emitters_)
			{
				(void)emitter;
				emitterNames.push_back(name);
			}

			std::sort(emitterNames.begin(), emitterNames.end());

			for (const std::string& name : emitterNames)
			{
				const bool isSelected = (selected == name);
				if (ImGui::Selectable(name.c_str(), isSelected))
				{
					selected = name;
				}
			}
			ImGui::EndListBox();
		}

		if (!selected.empty())
		{
			if (auto* e = GetEmitter(selected))
			{
				ImGui::SeparatorText("Selected Emitter");
				ImGui::Text("Name: %s", selected.c_str());

				ImGui::SameLine();
				if (ImGui::Button("Delete"))
				{
					RemoveEmitter(selected);
					selected.clear();
					ImGui::End();
					return;
				}

				const std::string parameterGroupName = BuildEmitterParameterGroupName(selected);

				ImGui::SameLine();
				if (ImGui::Button("Save Parameters"))
				{
					ParameterManager::GetInstance()->SaveFile(parameterGroupName);
				}

				ImGui::SameLine();
				if (ImGui::Button("Load Parameters"))
				{
					ParameterManager::GetInstance()->LoadFile(parameterGroupName);
					ApplyEmitterParameters(*e);
				}

				ImGui::SameLine();
				if (ImGui::Button("Duplicate"))
				{
					GpuParticleEmitterAsset dup = BuildAssetFromEmitter(selected);
					dup.name = MakeUniqueEmitterName(dup.name + "_copy");
					if (CreateEmitterFromAsset(dup, false))
					{
						selected = dup.name;
					}
				}

				const auto& info = e->GetInfo();
				const uint32_t effectiveType = (info.kind == GpuParticleKind::Ribbon)
					? static_cast<uint32_t>(ToGpuParticleType(info.ribbonType))
					: static_cast<uint32_t>(info.spriteType);
				const uint32_t packed = PackBillboardMode(info.kind, static_cast<uint32_t>(info.billboardFlags));
				ImGui::Text("Parameter Group: %s", parameterGroupName.c_str());
				ImGui::Text("Edit values in: Parameters window");
				ImGui::Text("Texture: %s", info.textureFilePath.c_str());
				ImGui::Text("EffectiveType (sent to GPU): %u", effectiveType);
				ImGui::Text("Packed billboardMode: 0x%08X", packed);

				ImGui::SeparatorText("Burst");
				static int burstCount = GpuParticleDebugPresets::kDefaultBurstCount;
				static int burstRepeat = GpuParticleDebugPresets::kDefaultBurstRepeat;

				ImGui::DragInt(
					"Burst Count",
					&burstCount,
					GpuParticleDebugPresets::kBurstCountStep,
					GpuParticleDebugPresets::kMinBurstCount,
					GpuParticleDebugPresets::kMaxBurstCount);
				ImGui::DragInt(
					"Burst Repeat",
					&burstRepeat,
					GpuParticleDebugPresets::kBurstRepeatStep,
					GpuParticleDebugPresets::kMinBurstRepeat,
					GpuParticleDebugPresets::kMaxBurstRepeat);

				if (ImGui::Button("Burst"))
				{
					e->RequestEmit(static_cast<uint32_t>((burstCount < 0) ? 0 : burstCount));
				}
				ImGui::SameLine();
				if (ImGui::Button("Burst x Repeat"))
				{
					const uint32_t c = static_cast<uint32_t>((burstCount < 0) ? 0 : burstCount);
					const uint32_t r = static_cast<uint32_t>((burstRepeat < 1) ? 1 : burstRepeat);
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
		RegisterEmitterParameters(*emitterPtr);

		// 作成したエミッターのポインタを返す
		return emitterPtr;
	}

	GpuParticleEmitter* GpuParticleManager::CreateEmitter(const std::string& name, GpuParticleType type)
	{
		const GpuParticleEmitter::EmitterInfo info =
			GpuParticleEmitterPresetTable::MakeEmitterInfo(type);

		return CreateEmitter(name, info);
	}

	GpuParticleEmitter* GpuParticleManager::CreateEmitter(const std::string& name, GpuParticleType type, const Vector3& position)
	{
		GpuParticleEmitter* emitter = CreateEmitter(name, type);
		if (!emitter)
		{
			return nullptr;
		}

		emitter->SetPosition(position);
		return emitter;
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
	size_t GpuParticleManager::GetActiveEmitterCount() const
	{
		size_t count = 0;
		for (const auto& [name, emitter] : emitters_)
		{
			(void)name;
			if (emitter && emitter->HasActiveParticles())
			{
				++count;
			}
		}
		return count;
	}

	uint32_t GpuParticleManager::GetEstimatedActiveParticleCount() const
	{
		uint32_t count = 0;
		for (const auto& [name, emitter] : emitters_)
		{
			(void)name;
			if (emitter)
			{
				count += emitter->GetEstimatedActiveParticleCount();
			}
		}
		return count;
	}

	void GpuParticleManager::BurstEmitter(const std::string& name, uint32_t count)
	{
		// 指定された名前のエミッターを取得
		if (auto* it = GetEmitter(name))
		{
			// エミット要求を出す
			it->RequestEmit(count);
		}
	}

	GpuParticleEmitter* GpuParticleManager::EmitBurst(const std::string& name, GpuParticleType type, const Vector3& position, uint32_t count)
	{
		GpuParticleEmitter* emitter = GetEmitter(name);
		if (!emitter)
		{
			emitter = CreateEmitter(name, type, position);
			if (!emitter)
			{
				return nullptr;
			}
		}
		else
		{
			emitter->SetPosition(position);
		}

		emitter->RequestEmit(count);
		return emitter;
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

	std::string GpuParticleManager::BuildEmitterJsonPath(const std::string& directoryPath, const std::string& emitterName)
	{
		if (directoryPath.empty())
		{
			return emitterName + ".json";
		}

		const char last = directoryPath.back();
		if (last == '/' || last == '\\')
		{
			return directoryPath + emitterName + ".json";
		}

		return directoryPath + "/" + emitterName + ".json";
	}

	std::string GpuParticleManager::MakeUniqueEmitterName(const std::string& baseName) const
	{
		if (!emitters_.contains(baseName))
		{
			return baseName;
		}

		for (uint32_t i = 2; i < 1000000; ++i)
		{
			const std::string candidate = baseName + std::to_string(i);
			if (!emitters_.contains(candidate))
			{
				return candidate;
			}
		}

		return baseName + "_new";
	}

	bool GpuParticleManager::RemoveEmitter(const std::string& name)
	{
		auto it = emitters_.find(name);
		if (it == emitters_.end())
		{
			return false;
		}

		UnregisterEmitterParameters(*it->second);
		emitters_.erase(it);
		return true;
	}

	std::string GpuParticleManager::BuildEmitterParameterGroupName(const std::string& emitterName)
	{
		// ParameterManagerの保存先を階層化し、エミッター単位のJsonに分ける。
		return "GPUParticle/" + emitterName;
	}

	void GpuParticleManager::RegisterEmitterParameters(GpuParticleEmitter& emitter)
	{
		auto* parameters = ParameterManager::GetInstance();
		const std::string groupName = BuildEmitterParameterGroupName(emitter.GetName());
		const auto& info = emitter.GetInfo();

		parameters->CreateGroup(groupName);

		// GPUへ送るエミッター調整値をParameterManagerへ集約し、ImGuiとJson保存の入口を一本化する。
		parameters->SetValue(groupName, "position", emitter.GetPosition());
		parameters->SetRange(groupName, "position", Vector3{ -1000.0f, -1000.0f, -1000.0f }, Vector3{ 1000.0f, 1000.0f, 1000.0f });
		parameters->SetValue(groupName, "textureFilePath", info.textureFilePath);
		parameters->SetValue(groupName, "radius", info.radius);
		parameters->SetRange(groupName, "radius", GpuParticleDebugPresets::kMinRadius, GpuParticleDebugPresets::kMaxRadius);
		parameters->SetValue(groupName, "loopCount", info.loopCount);
		parameters->SetRange(groupName, "loopCount", static_cast<uint32_t>(GpuParticleDebugPresets::kMinLoopCount), static_cast<uint32_t>(GpuParticleDebugPresets::kMaxLoopCount));
		parameters->SetValue(groupName, "loopFrequency", info.loopFrequency);
		parameters->SetRange(groupName, "loopFrequency", GpuParticleDebugPresets::kMinLoopFrequency, GpuParticleDebugPresets::kMaxLoopFrequency);
		parameters->SetValue(groupName, "drawType", info.drawType);
		parameters->SetRange(groupName, "drawType", 0u, 1000000u);
		parameters->SetValue(groupName, "kind", static_cast<uint32_t>(info.kind));
		parameters->SetRange(groupName, "kind", 0u, 3u);
		parameters->SetValue(groupName, "spriteType", static_cast<uint32_t>(info.spriteType));
		parameters->SetRange(groupName, "spriteType", 0u, static_cast<uint32_t>(GpuParticleType::Count) - 1u);
		parameters->SetValue(groupName, "ribbonType", static_cast<uint32_t>(info.ribbonType));
		parameters->SetRange(groupName, "ribbonType", 0u, static_cast<uint32_t>(kRibbonTypeCount - 1));
		parameters->SetValue(groupName, "billboardFlags", static_cast<uint32_t>(info.billboardFlags));
		parameters->SetRange(groupName, "billboardFlags", 0u, 7u);
		parameters->SetValue(groupName, "lifeScale", info.lifeScale);
		parameters->SetRange(groupName, "lifeScale", 0.01f, 10.0f);
		parameters->SetValue(groupName, "speedScale", info.speedScale);
		parameters->SetRange(groupName, "speedScale", 0.0f, 10.0f);

		parameters->SetDisplayName(groupName, "position", "発生位置");
		parameters->SetDisplayName(groupName, "textureFilePath", "テクスチャ");
		parameters->SetDisplayName(groupName, "radius", "発生半径");
		parameters->SetDisplayName(groupName, "loopCount", "ループ発生数");
		parameters->SetDisplayName(groupName, "loopFrequency", "ループ間隔(秒)");
		parameters->SetDisplayName(groupName, "drawType", "描画タイプ(0=自動)");
		parameters->SetDisplayName(groupName, "kind", "描画モード");
		parameters->SetDisplayName(groupName, "spriteType", "スプライト種別");
		parameters->SetDisplayName(groupName, "ribbonType", "リボン種別");
		parameters->SetDisplayName(groupName, "billboardFlags", "ビルボードフラグ");
		parameters->SetDisplayName(groupName, "lifeScale", "寿命倍率");
		parameters->SetDisplayName(groupName, "speedScale", "初速倍率");

		parameters->RegisterParameterApplier(groupName, &emitter, [this, emitterName = emitter.GetName()]()
			{
				if (auto* target = GetEmitter(emitterName))
				{
					ApplyEmitterParameters(*target);
				}
			});

		// 起動時にLoadFiles済みでも、後から生成されたエミッターはここで自分のJsonを読み直す。
		parameters->LoadFile(groupName);
		ApplyEmitterParameters(emitter);
	}

	void GpuParticleManager::ApplyEmitterParameters(GpuParticleEmitter& emitter)
	{
		auto* parameters = ParameterManager::GetInstance();
		const std::string groupName = BuildEmitterParameterGroupName(emitter.GetName());
		auto& info = emitter.GetInfoMutable();

		// ParameterManagerに登録された値だけを取得し、エミッター本体へ反映する。
		emitter.SetPosition(parameters->GetValue<Vector3>(groupName, "position"));
		info.textureFilePath = parameters->GetValue<std::string>(groupName, "textureFilePath");
		info.radius = std::max(parameters->GetValue<float>(groupName, "radius"), 0.0f);
		info.loopCount = parameters->GetValue<uint32_t>(groupName, "loopCount");
		info.loopFrequency = std::max(parameters->GetValue<float>(groupName, "loopFrequency"), 0.0f);
		info.drawType = parameters->GetValue<uint32_t>(groupName, "drawType");
		info.kind = ToSafeParticleKind(parameters->GetValue<uint32_t>(groupName, "kind"));
		info.spriteType = ToSafeSpriteType(parameters->GetValue<uint32_t>(groupName, "spriteType"));
		info.ribbonType = static_cast<GpuRibbonType>(ClampU32(parameters->GetValue<uint32_t>(groupName, "ribbonType"), 0u, static_cast<uint32_t>(kRibbonTypeCount - 1)));
		info.billboardFlags = ToSafeBillboardMode(parameters->GetValue<uint32_t>(groupName, "billboardFlags"));
		info.lifeScale = std::max(parameters->GetValue<float>(groupName, "lifeScale"), 0.01f);
		info.speedScale = std::max(parameters->GetValue<float>(groupName, "speedScale"), 0.0f);

		if (info.kind == GpuParticleKind::Mesh)
		{
			info.billboardFlags = BillboardMode::None;
		}
		else if (info.billboardFlags == BillboardMode::None)
		{
			info.billboardFlags = BillboardMode::Camera;
		}
	}

	void GpuParticleManager::UnregisterEmitterParameters(const GpuParticleEmitter& emitter)
	{
		ParameterManager::GetInstance()->UnregisterParameterApplier(
			BuildEmitterParameterGroupName(emitter.GetName()),
			&emitter);
	}

	GpuParticleEmitterAsset GpuParticleManager::BuildAssetFromEmitter(const std::string& name) const
	{
		GpuParticleEmitterAsset asset{};

		auto it = emitters_.find(name);
		if (it == emitters_.end())
		{
			return asset;
		}

		const GpuParticleEmitter* emitter = it->second.get();
		const auto& info = emitter->GetInfo();
		const Vector3 position = emitter->GetPosition();

		asset.name = emitter->GetName();
		asset.textureFilePath = info.textureFilePath;
		asset.position = position;
		asset.radius = info.radius;
		asset.loopCount = info.loopCount;
		asset.loopFrequency = info.loopFrequency;
		asset.drawType = info.drawType;
		asset.kind = info.kind;
		asset.spriteType = info.spriteType;
		asset.ribbonType = info.ribbonType;
		asset.billboardFlags = info.billboardFlags;

		return asset;
	}

	GpuParticleEmitter* GpuParticleManager::CreateEmitterFromAsset(const GpuParticleEmitterAsset& asset, bool overwrite)
	{
		if (asset.name.empty())
		{
			return nullptr;
		}

		auto it = emitters_.find(asset.name);
		if (it != emitters_.end())
		{
			if (!overwrite)
			{
				return nullptr;
			}

			UnregisterEmitterParameters(*it->second);
			emitters_.erase(it);
		}

		GpuParticleEmitter::EmitterInfo info{};
		info.textureFilePath = asset.textureFilePath;
		info.radius = asset.radius;
		info.loopCount = asset.loopCount;
		info.loopFrequency = asset.loopFrequency;
		info.drawType = asset.drawType;
		info.kind = asset.kind;
		info.spriteType = asset.spriteType;
		info.ribbonType = asset.ribbonType;
		info.billboardFlags = asset.billboardFlags;

		GpuParticleEmitter* emitter = CreateEmitter(asset.name, info);
		if (!emitter)
		{
			return nullptr;
		}

		emitter->SetPosition(asset.position);
		return emitter;
	}

	bool GpuParticleManager::SaveEmitterToFile(const std::string& name, const std::string& filePath) const
	{
		const GpuParticleEmitterAsset asset = BuildAssetFromEmitter(name);
		if (asset.name.empty())
		{
			return false;
		}

		return GpuParticleEmitterSerializer::SaveToFile(asset, filePath);
	}

	GpuParticleEmitter* GpuParticleManager::LoadEmitterFromFile(const std::string& filePath, bool overwrite)
	{
		const auto loaded = GpuParticleEmitterSerializer::LoadFromFile(filePath);
		if (!loaded.has_value())
		{
			return nullptr;
		}

		return CreateEmitterFromAsset(loaded.value(), overwrite);
	}

	void GpuParticleManager::LoadEmittersFromDirectory(const std::string& directoryPath, bool overwrite)
	{
		const auto files = GpuParticleEmitterSerializer::FindJsonFiles(directoryPath);
		for (const std::string& filePath : files)
		{
			LoadEmitterFromFile(filePath, overwrite);
		}
	}

	bool GpuParticleManager::SaveAllEmittersToDirectory(const std::string& directoryPath) const
	{
		bool allOk = true;

		for (const auto& [name, emitter] : emitters_)
		{
			(void)emitter;

			const std::string filePath = BuildEmitterJsonPath(directoryPath, name);
			if (!SaveEmitterToFile(name, filePath))
			{
				allOk = false;
			}
		}

		return allOk;
	}

} // namespace Ken4lowEngine
