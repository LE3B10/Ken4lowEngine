#define NOMINMAX
#include "PlayerCombatComponent.h"

#include <string>

#include "BulletManager.h"
#include "Camera.h"
#include "CollisionManager.h"
#include "GpuParticleManager.h"
#include "PlayerViewComponent.h"
#include "PlayerWeaponComponent.h"
#include "PlayerWeaponVisualComponent.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
	constexpr uint32_t kMuzzleSparkMeshId = 1000u;
	constexpr uint32_t kMuzzleSparkBurstCount = 8u;
	constexpr const char* kMuzzleSparkEmitterName = "MuzzleSparkMesh";
	constexpr const char* kMuzzleSparkMeshModelPath = "Test/cube.gltf";

	constexpr uint32_t kBulletTracerMeshId = 1001u;
	constexpr uint32_t kBulletTracerBurstCountPerPoint = 10u;
	constexpr int kBulletTracerPointCount = 18;
	constexpr float kBulletTracerStartOffset = 0.22f;
	constexpr float kBulletTracerStepDistance = 0.16f;
	constexpr const char* kBulletTracerEmitterBaseName = "BulletTracerMesh_";
	constexpr const char* kBulletTracerMeshModelPath = "Test/cube.gltf";

	std::string MakeMeshTexturePath(uint32_t meshId)
	{
		return "Mesh:" + std::to_string(meshId);
	}

	bool EnsureMeshAssetRegistered(K4E::GpuParticleManager* particle, uint32_t meshId, const char* modelPath)
	{
		if (!particle)
		{
			return false;
		}

		if (particle->FindMeshAsset(meshId))
		{
			return true;
		}

		return particle->LoadMeshAssetsFromAssimp(meshId, modelPath, true);
	}

	K4E::GpuParticleEmitter* GetOrCreateMeshEmitter(
		K4E::GpuParticleManager* particle,
		const char* name,
		uint32_t meshId,
		K4E::GpuParticleType particleType,
		const char* meshModelPath)
	{
		if (!EnsureMeshAssetRegistered(particle, meshId, meshModelPath))
		{
			return nullptr;
		}

		auto* emitter = particle->GetEmitter(name);
		if (emitter)
		{
			return emitter;
		}

		K4E::GpuParticleEmitter::EmitterInfo info{};
		info.textureFilePath = MakeMeshTexturePath(meshId);
		info.radius = 0.0f;
		info.loopCount = 0;
		info.loopFrequency = 0.0f;

		// drawType は粒子タイプのフィルタに使われるため、MeshIdは入れない。
		// MeshIdは textureFilePath = "Mesh:<id>" で GpuParticleRenderer に渡す。
		info.drawType = 0;
		info.kind = K4E::GpuParticleKind::Mesh;
		info.spriteType = particleType;
		info.billboardFlags = K4E::BillboardMode::None;

		return particle->CreateEmitter(name, info);
	}

	void EmitMuzzleSparkMesh(K4E::GpuParticleManager* particle, const K4E::Vector3& muzzlePos)
	{
		auto* emitter = GetOrCreateMeshEmitter(
			particle,
			kMuzzleSparkEmitterName,
			kMuzzleSparkMeshId,
			K4E::GpuParticleType::Spark,
			kMuzzleSparkMeshModelPath);

		if (!emitter)
		{
			return;
		}

		emitter->SetPosition(muzzlePos);
		emitter->RequestEmit(kMuzzleSparkBurstCount);
	}

	void EmitBulletTracerMesh(
		K4E::GpuParticleManager* particle,
		const K4E::Vector3& muzzlePos,
		const K4E::Vector3& fireForward)
	{
		if (!particle)
		{
			return;
		}

		const K4E::Vector3 forward = K4E::Vector3::Normalize(fireForward);

		// 同じエミッターへ連続で位置を変えても最後の位置にまとまるため、
		// 点列ごとに別エミッターを使い、弾道方向へ伸びて見えるようにする。
		for (int i = 0; i < kBulletTracerPointCount; ++i)
		{
			const std::string emitterName =
				std::string(kBulletTracerEmitterBaseName) + std::to_string(i);

			auto* emitter = GetOrCreateMeshEmitter(
				particle,
				emitterName.c_str(),
				kBulletTracerMeshId,
				K4E::GpuParticleType::BulletTracer,
				kBulletTracerMeshModelPath);

			if (!emitter)
			{
				continue;
			}

			const float distance =
				kBulletTracerStartOffset + kBulletTracerStepDistance * static_cast<float>(i);
			const K4E::Vector3 tracerPos = muzzlePos + forward * distance;

			emitter->SetPosition(tracerPos);
			emitter->RequestEmit(kBulletTracerBurstCountPerPoint);
		}
	}
}

void PlayerCombatComponent::BindDependencies(PlayerWeaponComponent* weapon, PlayerViewComponent* view)
{
	weapon_ = weapon;
	view_ = view;
}

void PlayerCombatComponent::SetAudioCallbacks(std::function<void()>* onFire, std::function<void()>* onReload)
{
	onFireSE_ = onFire;
	onReloadSE_ = onReload;
}

void PlayerCombatComponent::Tick(float deltaTime)
{
	if (shotDebugTimer_ > 0.0f)
	{
		shotDebugTimer_ -= deltaTime;
		if (shotDebugTimer_ < 0.0f)
		{
			shotDebugTimer_ = 0.0f;
		}
	}
}

void PlayerCombatComponent::FireOnce(
	const InputSnapshot& input,
	BulletManager* bulletManager,
	CollisionManager* collisionManager,
	const std::function<void(const K4E::Segment&)>& setDebugSegment)
{
	if (!weapon_ || !view_)
	{
		return;
	}

	// CombatFSM から呼ばれる発射処理
	auto* shootCam = view_->GetShootCamera();
	if (!bulletManager || !shootCam)
	{
		return;
	}

	const bool fired = weapon_->TryFire(input, shootCam, bulletManager, collisionManager);
	if (!fired)
	{
		return;
	}

	// 発射した瞬間のSE
	if (onFireSE_ && *onFireSE_)
	{
		(*onFireSE_)();
	}

	// 発射した瞬間のVFX
	if (auto* cam = view_->GetCamera())
	{
		K4E::Vector3 fireForward = K4E::Vector3::Normalize(cam->GetForward());
		K4E::Vector3 muzzlePos = cam->GetTranslate() + fireForward * 1.0f;

		// できるだけ武器モデル側の銃口位置を使う。
		// まだ武器が作られていない場合だけ、従来のカメラ前方にフォールバックする。
		if (weaponVisual_)
		{
			K4E::Vector3 visualMuzzle{};
			if (weaponVisual_->TryGetMuzzleWorldPosition(visualMuzzle))
			{
				muzzlePos = visualMuzzle;
			}

			K4E::Vector3 visualForward{};
			if (weaponVisual_->TryGetMuzzleForward(visualForward))
			{
				fireForward = visualForward;
			}
		}

		auto* particle = K4E::GpuParticleManager::GetInstance();
		if (particle)
		{
			// Spriteの発光表現はそのまま残す。
			particle->EmitBurst(
				"MuzzleFlash",
				K4E::GpuParticleType::MuzzleFlash,
				muzzlePos,
				8);

			// 火花はMesh Particleとして別に発生させる。
			EmitMuzzleSparkMesh(particle, muzzlePos);

			// 弾道Meshは重い/目立つため、ヘビー武器だけ表示する。
			if (weapon_->IsHeavyCategory())
			{
				EmitBulletTracerMesh(particle, muzzlePos, fireForward);
			}
		}
	}

	// カメラリコイル
	const bool ads = input.aimHeld;
	const float vDeg = ads ? recoilPitchDegAds_ : recoilPitchDegHip_;
	const float hDeg = ads ? recoilYawDegAds_ : recoilYawDegHip_;
	view_->AddRecoil(vDeg, hDeg);

	// デバッグ用ショットレイ
	if (setDebugSegment)
	{
		if (auto* cam = view_->GetCamera())
		{
			K4E::Segment seg{};
			seg.origin = cam->GetTranslate();
			seg.diff = cam->GetForward() * hitscanRange_;
			setDebugSegment(seg);
			shotDebugTimer_ = 0.05f;
		}
	}
}

void PlayerCombatComponent::DrawImGui()
{
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("Recoil Tuning", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Pre-shot camera kick (deg)");
		ImGui::DragFloat("Hip Pitch", &recoilPitchDegHip_, 0.01f, 0.0f, 10.0f, "%.2f");
		ImGui::DragFloat("Hip Yaw", &recoilYawDegHip_, 0.10f, 0.0f, 20.0f, "%.2f");
		ImGui::Separator();
		ImGui::DragFloat("ADS Pitch", &recoilPitchDegAds_, 0.01f, 0.0f, 10.0f, "%.2f");
		ImGui::DragFloat("ADS Yaw", &recoilYawDegAds_, 0.01f, 0.0f, 10.0f, "%.2f");
		ImGui::Separator();
		ImGui::DragFloat("Hitscan Range", &hitscanRange_, 1.0f, 1.0f, 5000.0f, "%.1f");
		ImGui::Text("ShotDebugTimer: %.3f", shotDebugTimer_);
	}
#endif
}
