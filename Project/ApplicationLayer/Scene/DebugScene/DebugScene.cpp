#define NOMINMAX
#include "DebugScene.h"
#include <DirectXCommon.h>
#include <Input.h>
#include <SpriteManager.h>
#include "CameraManager.h"
#include "Wireframe.h"
#include <GameTimer.h>
#ifdef _DEBUG
#include <DebugCamera.h>
#endif // _DEBUG

#ifdef USE_IMGUI
#include <ImGuiManager.h>
#endif // USE_IMGUI
#include <GpuParticleManager.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

using namespace Ken4lowEngine;

namespace
{
	void DebugLog(const std::string& message)
	{
		std::string line = message + "\n";
		OutputDebugStringA(line.c_str());
	}

	GpuParticleType ToDebugParticleType(int index)
	{
		switch (index)
		{
		case 0: return GpuParticleType::Default;
		case 1: return GpuParticleType::Debris;
		case 2: return GpuParticleType::Spark;
		case 3: return GpuParticleType::Shockwave;
		case 4: return GpuParticleType::Smoke;
		case 5: return GpuParticleType::Heal;
		case 6: return GpuParticleType::ArmorBreak;
		case 7: return GpuParticleType::VoxelFragment;
		default: return GpuParticleType::VoxelFragment;
		}
	}

	GpuParticleEmitter* PrepareDebugMeshParticleEmitter(
		GpuParticleManager* manager,
		const std::string& emitterName,
		uint32_t meshId,
		GpuParticleType type,
		const Vector3& position,
		float radius)
	{
		if (!manager)
		{
			return nullptr;
		}

		if (GpuParticleEmitter* existing = manager->GetEmitter(emitterName))
		{
			auto& info = existing->GetInfoMutable();
			info.kind = GpuParticleKind::Mesh;
			info.spriteType = type;
			info.drawType = static_cast<uint32_t>(type);
			info.billboardFlags = BillboardMode::None;
			info.textureFilePath = "Mesh:" + std::to_string(meshId);
			info.radius = radius;
			info.loopCount = 0;
			info.loopFrequency = 0.0f;
			existing->SetPosition(position);
			return existing;
		}

		GpuParticleEmitter::EmitterInfo info{};
		info.kind = GpuParticleKind::Mesh;
		info.spriteType = type;
		info.drawType = static_cast<uint32_t>(type);
		info.billboardFlags = BillboardMode::None;
		info.textureFilePath = "Mesh:" + std::to_string(meshId);
		info.radius = radius;
		info.loopCount = 0;
		info.loopFrequency = 0.0f;

		GpuParticleEmitter* created = manager->CreateEmitter(emitterName, info);
		if (created)
		{
			created->SetPosition(position);
		}
		return created;
	}
}

void DebugScene::Initialize()
{
#ifdef _DEBUG
	DebugCamera::GetInstance()->Initialize();
#endif // _DEBUG

	dxCommon_ = DirectXCommon::GetInstance();
	input_ = Input::GetInstance();

	collisionManager_ = std::make_unique<CollisionManager>();
	collisionManager_->Initialize();

	debugBoss_ = std::make_unique<GuardianBoss>();
	debugBoss_->Initialize();
	collisionManager_->AddCollider(debugBoss_.get());

	debugBoss_->SetPosition({ 0.0f, 2.25f, 30.0f });
	debugBoss_->SetYaw(3.141592f);

	BuildDebugVoxelDisintegration();
}

void DebugScene::Update()
{
#ifdef _DEBUG
	UpdateDebug();
#endif // _DEBUG

	float deltaTime = K4E::GameTimer::GetInstance()->GetDeltaTime();

	if (debugBoss_)
	{
		debugBoss_->SetTargetPosition({});
		debugBoss_->Update(deltaTime);
	}

	for (auto& block : debugVoxelBlocks_)
	{
		if (block.object)
		{
			block.object->Update();
		}
	}

	UpdateDebugBossHitTest();
	UpdateDebugParticleTest();
	UpdateDebugVoxelDisintegration(deltaTime);

	collisionManager_->Update();
	collisionManager_->CheckAllCollisions();
}

void DebugScene::Draw3DObjects()
{
	if (debugBoss_)
	{
		debugBoss_->Draw();
	}

	for (auto& block : debugVoxelBlocks_)
	{
		if (block.visible && block.object)
		{
			block.object->Draw();
		}
	}

#ifdef _DEBUG
	Wireframe::GetInstance()->DrawGrid(100.0f, 50.0f, { 0.25f, 0.25f, 0.25f, 1.0f });
	collisionManager_->Draw();
#endif // _DEBUG
}

void DebugScene::DrawShadowObjects()
{
	if (debugBoss_)
	{
		debugBoss_->DrawShadow();
	}

	for (auto& block : debugVoxelBlocks_)
	{
		if (block.visible && block.object)
		{
			block.object->DrawShadow();
		}
	}
}

void DebugScene::Draw2DSprites()
{
	SpriteManager::GetInstance()->SetRenderSetting_Background();
	SpriteManager::GetInstance()->SetRenderSetting_UI();
}

void DebugScene::Finalize()
{
	input_->SetLockCursor(false);
	input_->SetCursorVisible(true);

	debugVoxelBlocks_.clear();
	debugBoss_.reset();
	collisionManager_.reset();

	input_ = nullptr;
	dxCommon_ = nullptr;
}

void DebugScene::DrawImGui()
{
#ifdef USE_IMGUI

	if (debugBoss_)
	{
		debugBoss_->DrawImGui();
	}

	ImGui::Begin("Debug Boss Hit Test");

	ImGui::Checkbox("Enable Hit Test", &debugBossHitTestEnabled_);
	ImGui::DragFloat("Hit Radius", &debugHitRadius_, 0.01f, 0.1f, 5.0f);
	ImGui::DragFloat("Base Damage", &debugBaseDamage_, 0.1f, 1.0f, 999.0f);

	ImGui::Separator();
	ImGui::Text("Press H to test hit.");
	ImGui::TextWrapped("%s", debugHitLog_.c_str());

	ImGui::End();

	GpuParticleManager::GetInstance()->DrawImGui();

	{
		static char meshModelPath[256] = "Test/cube.gltf";
		static int meshId = 1000;
		static int particleTypeIndex = 7;
		static int spriteCount = 16;
		static int meshCount = 48;
		static float radius = 0.35f;
		static float position[3] = { 0.0f, 2.5f, 18.0f };
		static bool meshLoaded = false;
		static bool spawnSideBySide = true;

		const char* particleTypeNames[] = {
			"Default",
			"Debris",
			"Spark",
			"Shockwave",
			"Smoke",
			"Heal",
			"ArmorBreak",
			"VoxelFragment"
		};

		GpuParticleManager* gpuParticleManager = GpuParticleManager::GetInstance();
		const GpuParticleType selectedType = ToDebugParticleType(particleTypeIndex);

		ImGui::Begin("GPU Particle Sprite / Mesh Test");

		ImGui::InputText("Mesh Model Path", meshModelPath, IM_ARRAYSIZE(meshModelPath));
		ImGui::InputInt("MeshId", &meshId);
		if (meshId < 0)
		{
			meshId = 0;
		}

		if (ImGui::Button("Load Mesh Asset"))
		{
			meshLoaded = gpuParticleManager->LoadMeshAssetsFromAssimp(static_cast<uint32_t>(meshId), meshModelPath, true);
			debugParticleLog_ = meshLoaded
				? "MeshParticle Debug: LoadMeshAssetsFromAssimp succeeded."
				: "MeshParticle Debug: LoadMeshAssetsFromAssimp failed.";
			DebugLog(debugParticleLog_);
		}

		ImGui::SameLine();
		ImGui::Text("Loaded: %s", meshLoaded ? "true" : "false");
		ImGui::Text("Registered MeshAssets: %zu", gpuParticleManager->GetMeshAssets().size());

		if (const auto* meshAsset = gpuParticleManager->FindMeshAsset(static_cast<uint32_t>(meshId)))
		{
			ImGui::Text("MeshId %d found. IndexCount: %u", meshId, meshAsset->indexCount);
			ImGui::Text("Mesh Texture: %s", meshAsset->textureFilePath.c_str());
		}
		else
		{
			ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "MeshId %d is not registered yet.", meshId);
		}

		ImGui::Separator();
		ImGui::Combo("Particle Type", &particleTypeIndex, particleTypeNames, IM_ARRAYSIZE(particleTypeNames));
		ImGui::DragInt("Sprite Count", &spriteCount, 1.0f, 0, 1000);
		ImGui::DragInt("Mesh Count", &meshCount, 1.0f, 0, 1000);
		ImGui::DragFloat("Emitter Radius", &radius, 0.01f, 0.0f, 30.0f);
		ImGui::DragFloat3("Center Position", position, 0.05f);
		ImGui::Checkbox("Side By Side", &spawnSideBySide);

		Vector3 center{ position[0], position[1], position[2] };
		Vector3 spritePos = center;
		Vector3 meshPos = center;
		if (spawnSideBySide)
		{
			spritePos.x -= 2.0f;
			meshPos.x += 2.0f;
		}

		if (ImGui::Button("Spawn Sprite"))
		{
			if (auto* spriteEmitter = gpuParticleManager->EmitBurst(
				"DebugScene_SpriteParticle",
				selectedType,
				spritePos,
				static_cast<uint32_t>(std::max(spriteCount, 0))))
			{
				spriteEmitter->GetInfoMutable().radius = radius;
				spriteEmitter->SetPosition(spritePos);
			}

			debugParticleLog_ = "Spawn: DebugScene Sprite Particle";
			DebugLog(debugParticleLog_);
		}

		ImGui::SameLine();
		if (ImGui::Button("Spawn Mesh"))
		{
			if (!gpuParticleManager->FindMeshAsset(static_cast<uint32_t>(meshId)))
			{
				meshLoaded = gpuParticleManager->LoadMeshAssetsFromAssimp(static_cast<uint32_t>(meshId), meshModelPath, true);
			}

			if (auto* meshEmitter = PrepareDebugMeshParticleEmitter(
				gpuParticleManager,
				"DebugScene_MeshParticle",
				static_cast<uint32_t>(meshId),
				selectedType,
				meshPos,
				radius))
			{
				meshEmitter->RequestEmit(static_cast<uint32_t>(std::max(meshCount, 0)));
				debugParticleLog_ = "Spawn: DebugScene Mesh Particle";
			}
			else
			{
				debugParticleLog_ = "Spawn failed: DebugScene Mesh Particle";
			}
			DebugLog(debugParticleLog_);
		}

		ImGui::SameLine();
		if (ImGui::Button("Spawn Both"))
		{
			if (auto* spriteEmitter = gpuParticleManager->EmitBurst(
				"DebugScene_SpriteParticle",
				selectedType,
				spritePos,
				static_cast<uint32_t>(std::max(spriteCount, 0))))
			{
				spriteEmitter->GetInfoMutable().radius = radius;
				spriteEmitter->SetPosition(spritePos);
			}

			if (!gpuParticleManager->FindMeshAsset(static_cast<uint32_t>(meshId)))
			{
				meshLoaded = gpuParticleManager->LoadMeshAssetsFromAssimp(static_cast<uint32_t>(meshId), meshModelPath, true);
			}

			if (auto* meshEmitter = PrepareDebugMeshParticleEmitter(
				gpuParticleManager,
				"DebugScene_MeshParticle",
				static_cast<uint32_t>(meshId),
				selectedType,
				meshPos,
				radius))
			{
				meshEmitter->RequestEmit(static_cast<uint32_t>(std::max(meshCount, 0)));
			}

			debugParticleLog_ = "Spawn: DebugScene Sprite + Mesh Particles";
			DebugLog(debugParticleLog_);
		}

		ImGui::SeparatorText("Voxel Disintegration Test");

		static char voxelModelPath[256] = "Test/cube.gltf";
		ImGui::InputText("Voxel Model", voxelModelPath, IM_ARRAYSIZE(voxelModelPath));
		ImGui::DragFloat3("Voxel Center", &debugVoxelCenter_.x, 0.05f);
		ImGui::DragInt("Grid X", &debugVoxelGridX_, 1.0f, 1, 8);
		ImGui::DragInt("Grid Y", &debugVoxelGridY_, 1.0f, 1, 8);
		ImGui::DragInt("Grid Z", &debugVoxelGridZ_, 1.0f, 1, 8);
		ImGui::DragFloat("Voxel Block Scale", &debugVoxelBlockScale_, 0.01f, 0.05f, 2.0f);
		ImGui::DragFloat("Voxel Spacing", &debugVoxelSpacing_, 0.01f, 0.05f, 2.0f);
		ImGui::DragFloat("Voxel Duration", &debugVoxelDisintegrationDuration_, 0.01f, 0.20f, 5.00f);
		ImGui::DragFloat("Voxel Particle Radius", &debugVoxelParticleRadius_, 0.01f, 0.05f, 3.0f);

		int voxelParticleCount = static_cast<int>(debugVoxelParticleCount_);
		if (ImGui::DragInt("Voxel Particle Count", &voxelParticleCount, 1.0f, 1, 64))
		{
			debugVoxelParticleCount_ = static_cast<uint32_t>(std::max(voxelParticleCount, 1));
		}

		if (ImGui::Button("Build Voxel Object"))
		{
			debugVoxelModelPath_ = voxelModelPath;
			BuildDebugVoxelDisintegration();
		}

		ImGui::SameLine();
		if (ImGui::Button("Start Voxel Disintegration"))
		{
			debugVoxelModelPath_ = voxelModelPath;
			StartDebugVoxelDisintegration();
		}

		ImGui::Text("Voxel Active: %s  Blocks: %zu  Time: %.2f / %.2f",
			debugVoxelDisintegrationActive_ ? "true" : "false",
			debugVoxelBlocks_.size(),
			debugVoxelDisintegrationTimer_,
			debugVoxelDisintegrationDuration_);

		ImGui::Separator();
		ImGui::Text("Sprite position: %.2f, %.2f, %.2f", spritePos.x, spritePos.y, spritePos.z);
		ImGui::Text("Mesh position:   %.2f, %.2f, %.2f", meshPos.x, meshPos.y, meshPos.z);
		ImGui::TextWrapped("%s", debugParticleLog_.c_str());

		ImGui::End();
	}

#endif // USE_IMGUI
}

void DebugScene::UpdateDebug()
{
	if (input_->TriggerKey(DIK_F12))
	{
		CameraManager::GetInstance()->SetUseDebugCamera(!CameraManager::GetInstance()->IsUsingDebugCamera());
		Wireframe::GetInstance()->SetDebugCamera(!Wireframe::GetInstance()->GetDebugCamera());
		GpuParticleManager::GetInstance()->SetDebugCameraEnabled(!isDebugCamera_);
		isDebugCamera_ = !isDebugCamera_;
	}
}

const char* DebugScene::ToString(BossHitPart part) const
{
	switch (part)
	{
	case BossHitPart::Head:     return "Head";
	case BossHitPart::Body:     return "Body";
	case BossHitPart::LeftArm:  return "LeftArm";
	case BossHitPart::RightArm: return "RightArm";
	case BossHitPart::LeftLeg:  return "LeftLeg";
	case BossHitPart::RightLeg: return "RightLeg";
	default:                    return "None";
	}
}

void DebugScene::UpdateDebugBossHitTest()
{
	if (!debugBossHitTestEnabled_)
	{
		return;
	}

	if (!debugBoss_)
	{
		debugHitLog_ = "Boss or Player is null.";
		DebugLog(debugHitLog_);
		return;
	}

	if (!input_->TriggerKey(DIK_H))
	{
		return;
	}

	Vector3 attackCenter = debugBoss_->GetCenterPosition();
	attackCenter.y += 1.0f;

	const BossHitResult hitResult =
		debugBoss_->CheckDebugHitSphere(attackCenter, debugHitRadius_);

	if (hitResult.isHit)
	{
		debugBoss_->ApplyDebugHitResult(hitResult, debugBaseDamage_);

		Vector3 effectPos = attackCenter;
		effectPos.y += 0.15f;

		GpuParticleManager::GetInstance()->EmitBurst(
			"Debug_HitSpark_OnHit",
			GpuParticleType::Spark,
			effectPos,
			18);

		StartDebugVoxelDisintegration();

		debugHitLog_ =
			std::string("HIT  Part: ") + ToString(hitResult.part) +
			"  Damage: " + std::to_string(debugBaseDamage_ * hitResult.damageMultiplier) +
			"  HP: " + std::to_string(debugBoss_->GetHP()) +
			" / " + std::to_string(debugBoss_->GetMaxHP());
	}
	else
	{
		debugHitLog_ = "MISS";
	}

	DebugLog(debugHitLog_);
}

void DebugScene::UpdateDebugParticleTest()
{
	if (!debugBoss_)
	{
		return;
	}

	GpuParticleManager* gpuParticleManager = GpuParticleManager::GetInstance();
	if (!gpuParticleManager)
	{
		debugParticleLog_ = "GpuParticleManager is null.";
		return;
	}

	const Vector3 bossCenter = debugBoss_->GetCenterPosition();

	if (input_->TriggerKey(DIK_1))
	{
		Vector3 pos = bossCenter;
		pos.y += 1.0f;

		gpuParticleManager->EmitBurst(
			"Debug_HitSpark",
			GpuParticleType::Spark,
			pos,
			20);

		debugParticleLog_ = "Spawn: HitSpark";
		DebugLog(debugParticleLog_);
	}

	if (input_->TriggerKey(DIK_2))
	{
		Vector3 pos = bossCenter;
		pos.y += 1.5f;

		gpuParticleManager->EmitBurst(
			"Debug_Heal",
			GpuParticleType::Heal,
			pos,
			24);

		debugParticleLog_ = "Spawn: Heal_Effect";
		DebugLog(debugParticleLog_);
	}

	if (input_->TriggerKey(DIK_3))
	{
		Vector3 pos = bossCenter;

		gpuParticleManager->EmitBurst(
			"Debug_BossAppear",
			GpuParticleType::Default,
			pos,
			48);

		debugParticleLog_ = "Spawn: Boss_Appear_Dust";
		DebugLog(debugParticleLog_);
	}
}

void DebugScene::BuildDebugVoxelDisintegration()
{
	debugVoxelDisintegrationActive_ = false;
	debugVoxelDisintegrationTimer_ = 0.0f;
	debugVoxelBlocks_.clear();

	debugVoxelGridX_ = std::clamp(debugVoxelGridX_, 1, 8);
	debugVoxelGridY_ = std::clamp(debugVoxelGridY_, 1, 8);
	debugVoxelGridZ_ = std::clamp(debugVoxelGridZ_, 1, 8);

	const Vector3 half{
		(debugVoxelGridX_ - 1) * debugVoxelSpacing_ * 0.5f,
		(debugVoxelGridY_ - 1) * debugVoxelSpacing_ * 0.5f,
		(debugVoxelGridZ_ - 1) * debugVoxelSpacing_ * 0.5f
	};

	const Vector3 impactCenter{
		debugVoxelCenter_.x,
		debugVoxelCenter_.y + debugVoxelSpacing_ * 0.45f,
		debugVoxelCenter_.z - debugVoxelSpacing_ * 0.35f
	};

	float maxDistance = 0.001f;

	for (int z = 0; z < debugVoxelGridZ_; ++z)
	{
		for (int y = 0; y < debugVoxelGridY_; ++y)
		{
			for (int x = 0; x < debugVoxelGridX_; ++x)
			{
				Vector3 pos{
					debugVoxelCenter_.x + x * debugVoxelSpacing_ - half.x,
					debugVoxelCenter_.y + y * debugVoxelSpacing_ - half.y,
					debugVoxelCenter_.z + z * debugVoxelSpacing_ - half.z
				};

				const float dx = pos.x - impactCenter.x;
				const float dy = pos.y - impactCenter.y;
				const float dz = pos.z - impactCenter.z;
				const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
				maxDistance = std::max(maxDistance, distance);
			}
		}
	}

	for (int z = 0; z < debugVoxelGridZ_; ++z)
	{
		for (int y = 0; y < debugVoxelGridY_; ++y)
		{
			for (int x = 0; x < debugVoxelGridX_; ++x)
			{
				Vector3 pos{
					debugVoxelCenter_.x + x * debugVoxelSpacing_ - half.x,
					debugVoxelCenter_.y + y * debugVoxelSpacing_ - half.y,
					debugVoxelCenter_.z + z * debugVoxelSpacing_ - half.z
				};

				const float dx = pos.x - impactCenter.x;
				const float dy = pos.y - impactCenter.y;
				const float dz = pos.z - impactCenter.z;
				const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
				const float normalized = distance / maxDistance;
				const float jitter = static_cast<float>((x * 13 + y * 7 + z * 5) % 11) / 11.0f;

				DebugVoxelBlock block{};
				block.object = std::make_unique<K4E::Object3D>();
				block.object->Initialize(debugVoxelModelPath_);
				block.object->SetTranslate(pos);
				block.object->SetScale({ debugVoxelBlockScale_, debugVoxelBlockScale_, debugVoxelBlockScale_ });
				block.object->SetDissolveThreshold(1.0f);
				block.object->SetDissolveEdgeThickness(0.0f);
				block.object->Update();

				block.position = pos;
				block.visible = true;
				block.breakTime = normalized * debugVoxelDisintegrationDuration_ * 0.85f + jitter * 0.12f;

				debugVoxelBlocks_.push_back(std::move(block));
			}
		}
	}

	debugParticleLog_ = "Build: Voxel Disintegration object.";
	DebugLog(debugParticleLog_);
}

void DebugScene::StartDebugVoxelDisintegration()
{
	if (debugVoxelBlocks_.empty())
	{
		BuildDebugVoxelDisintegration();
	}

	GpuParticleManager* gpuParticleManager = GpuParticleManager::GetInstance();
	if (gpuParticleManager && !gpuParticleManager->FindMeshAsset(debugVoxelMeshId_))
	{
		gpuParticleManager->LoadMeshAssetsFromAssimp(debugVoxelMeshId_, debugVoxelModelPath_, true);
	}

	for (auto& block : debugVoxelBlocks_)
	{
		block.visible = true;
	}

	debugVoxelDisintegrationTimer_ = 0.0f;
	debugVoxelDisintegrationActive_ = true;

	debugParticleLog_ = "Start: Voxel Disintegration.";
	DebugLog(debugParticleLog_);
}

void DebugScene::UpdateDebugVoxelDisintegration(float deltaTime)
{
	if (!debugVoxelDisintegrationActive_)
	{
		return;
	}

	debugVoxelDisintegrationTimer_ += deltaTime;

	for (auto& block : debugVoxelBlocks_)
	{
		if (!block.visible || debugVoxelDisintegrationTimer_ < block.breakTime)
		{
			continue;
		}

		// ブロックを非表示にした地点からVoxel専用破片を出して、実際に分解されているように見せる。
		block.visible = false;
		EmitDebugVoxelBreakParticle(block.position, debugVoxelParticleCount_);
	}

	if (debugVoxelDisintegrationTimer_ >= debugVoxelDisintegrationDuration_)
	{
		debugVoxelDisintegrationActive_ = false;
		debugParticleLog_ = "Finish: Voxel Disintegration.";
		DebugLog(debugParticleLog_);
	}
}

void DebugScene::EmitDebugVoxelBreakParticle(const Vector3& position, uint32_t count)
{
	GpuParticleManager* gpuParticleManager = GpuParticleManager::GetInstance();
	if (!gpuParticleManager)
	{
		return;
	}

	if (!gpuParticleManager->FindMeshAsset(debugVoxelMeshId_))
	{
		gpuParticleManager->LoadMeshAssetsFromAssimp(debugVoxelMeshId_, debugVoxelModelPath_, true);
	}

	if (auto* meshEmitter = PrepareDebugMeshParticleEmitter(
		gpuParticleManager,
		"DebugScene_VoxelDisintegrationMesh",
		debugVoxelMeshId_,
		GpuParticleType::VoxelFragment,
		position,
		debugVoxelParticleRadius_))
	{
		meshEmitter->RequestEmit(count);
	}

	if (debugVoxelDisintegrationTimer_ < debugVoxelDisintegrationDuration_ * 0.25f)
	{
		if (auto* flashEmitter = gpuParticleManager->EmitBurst(
			"DebugScene_VoxelDisintegrationFlash",
			GpuParticleType::MuzzleFlash,
			position,
			2))
		{
			flashEmitter->GetInfoMutable().radius = debugVoxelParticleRadius_ * 0.35f;
			flashEmitter->SetPosition(position);
		}
	}
}
