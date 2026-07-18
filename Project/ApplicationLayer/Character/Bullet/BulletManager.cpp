#include "BulletManager.h"
#include "Bullet.h"
#include "CollisionManager.h"
#include "Engine/Physics/Core/PhysicsWorld.h"
#include "BulletEnemyCollisionSoA.h"
#include "GpuParticleManager.h"

#include <algorithm>
#include <cmath>
#include <string>

using namespace Ken4lowEngine;

namespace
{
	constexpr uint32_t kMuzzleSparkMeshId = 1000u;
	constexpr uint32_t kMuzzleSparkBurstCount = 8u;
	constexpr const char* kMuzzleSparkEmitterName = "MuzzleSparkMesh";
	constexpr const char* kMuzzleSparkMeshModelPath = "Sample/cube.gltf";

	constexpr uint32_t kBulletTracerMeshId = 1001u;
	constexpr uint32_t kBulletTracerBurstCountPerPoint = 4u;
	constexpr int kBulletTracerMinPointCount = 2;
	constexpr int kBulletTracerMaxPointCount = 64;
	constexpr uint32_t kBulletTracerEmitterBankCount = 4u;
	constexpr float kBulletTracerStartOffset = 0.08f;
	constexpr float kBulletTracerPointSpacing = 0.75f;
	constexpr float kBulletTracerLifeScale = 1.25f;
	constexpr const char* kBulletTracerEmitterBaseName = "BulletTracerMesh_";
	constexpr const char* kBulletTracerMeshModelPath = "Sample/cube.gltf";

	float Length(const Vector3& value)
	{
		return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
	}

	std::string MakeMeshTexturePath(uint32_t meshId)
	{
		return "Mesh:" + std::to_string(meshId);
	}

	bool EnsureMeshAssetRegistered(GpuParticleManager* particle, uint32_t meshId, const char* modelPath)
	{
		if (!particle) return false;
		if (particle->FindMeshAsset(meshId)) return true;
		return particle->LoadMeshAssetsFromAssimp(meshId, modelPath, true);
	}

	GpuParticleEmitter* GetOrCreateMeshEmitter(
		GpuParticleManager* particle,
		const char* name,
		uint32_t meshId,
		GpuParticleType particleType,
		const char* meshModelPath)
	{
		if (!EnsureMeshAssetRegistered(particle, meshId, meshModelPath)) return nullptr;
		if (GpuParticleEmitter* emitter = particle->GetEmitter(name)) return emitter;

		GpuParticleEmitter::EmitterInfo info{};
		info.textureFilePath = MakeMeshTexturePath(meshId);
		info.radius = 0.0f;
		info.loopCount = 0;
		info.loopFrequency = 0.0f;
		info.drawType = 0;
		info.kind = GpuParticleKind::Mesh;
		info.spriteType = particleType;
		info.billboardFlags = BillboardMode::None;
		if (particleType == GpuParticleType::BulletTracer)
		{
			info.lifeScale = kBulletTracerLifeScale;
		}
		return particle->CreateEmitter(name, info);
	}

	void EmitMuzzleSparkMesh(GpuParticleManager* particle, const Vector3& muzzlePosition)
	{
		GpuParticleEmitter* emitter = GetOrCreateMeshEmitter(
			particle,
			kMuzzleSparkEmitterName,
			kMuzzleSparkMeshId,
			GpuParticleType::Spark,
			kMuzzleSparkMeshModelPath);
		if (!emitter) return;

		emitter->SetPosition(muzzlePosition);
		emitter->RequestEmit(kMuzzleSparkBurstCount);
	}

	void EmitBulletTracerMesh(GpuParticleManager* particle, const Vector3& muzzlePosition, const Vector3& tracerEndPosition)
	{
		if (!particle) return;

		const Vector3 tracerVector = tracerEndPosition - muzzlePosition;
		const float tracerLength = Length(tracerVector);
		if (tracerLength <= 1.0e-4f) return;

		const Vector3 forward = tracerVector * (1.0f / tracerLength);
		const float firstDistance = std::min(kBulletTracerStartOffset, tracerLength);
		const float drawableLength = std::max(0.0f, tracerLength - firstDistance);
		const int pointCount = std::clamp(
			static_cast<int>(std::ceil(drawableLength / kBulletTracerPointSpacing)) + 1,
			kBulletTracerMinPointCount,
			kBulletTracerMaxPointCount);

		static uint32_t tracerEmitterBank = 0u;
		const uint32_t currentBank = tracerEmitterBank;
		tracerEmitterBank = (tracerEmitterBank + 1u) % kBulletTracerEmitterBankCount;

		// 実際の着弾点までを等間隔で補間し、固定距離で弾道が途中終了しないようにする。
		for (int i = 0; i < pointCount; ++i)
		{
			const float ratio = pointCount > 1
				? static_cast<float>(i) / static_cast<float>(pointCount - 1)
				: 1.0f;
			const float distance = firstDistance + drawableLength * ratio;
			const Vector3 tracerPosition = muzzlePosition + forward * distance;
			const std::string emitterName = std::string(kBulletTracerEmitterBaseName) +
				std::to_string(currentBank) + "_" + std::to_string(i);

			GpuParticleEmitter* emitter = GetOrCreateMeshEmitter(
				particle,
				emitterName.c_str(),
				kBulletTracerMeshId,
				GpuParticleType::BulletTracer,
				kBulletTracerMeshModelPath);
			if (!emitter) continue;

			emitter->SetPosition(tracerPosition);
			emitter->RequestEmit(kBulletTracerBurstCountPerPoint);
		}
	}

	void EmitPlayerWeaponFireEffects(const Vector3& muzzlePosition, const Vector3& tracerEndPosition)
	{
		EffectSystem::GetInstance()->Play("MuzzleFlash", muzzlePosition);

		GpuParticleManager* particle = GpuParticleManager::GetInstance();
		if (!particle) return;
		EmitMuzzleSparkMesh(particle, muzzlePosition);
		EmitBulletTracerMesh(particle, muzzlePosition, tracerEndPosition);
	}
}

void BulletManager::Initialize(CollisionManager* collisionManager)
{
	collisionManager_ = collisionManager;
	shotEffectTransformResolver_ = {};
	bullets_.clear();
}

Bullet* BulletManager::Spawn(const Vector3& startPos,
	const Vector3& dirNormalized,
	float speed,
	int damage,
	float lifeTimeSec,
	const Ken4lowEngine::Vector3& shooterPosition,
	uint32_t shooterColliderId,
	uint32_t typeId,
	const WeaponParams& weaponParams
)
{
	auto b = std::make_unique<Bullet>();
	b->Initialize(startPos, dirNormalized * speed, damage, lifeTimeSec, shooterPosition, shooterColliderId, typeId);
	b->SetModelDrawEnabled(weaponParams.drawProjectileModel);
	b->SetCollisionManager(collisionManager_);
	b->SetWorldImpactCallback(worldImpactCallback_);
	b->ConfigureSplashDamage(weaponParams);
	b->SetWeaponMetadata(weaponParams);
	b->SetUsePhysicsTrigger(usePhysicsTriggerForNormalBullets_);
	if (b->UsesPhysicsTrigger())
	{
		// 通常弾のColliderをPhysicsWorld Trigger判定へ登録する。Legacy側には残すが命中処理はBullet側でスキップする。
		b->SetCollisionLayer(playerBulletLayer_);
		if (physicsWorld_)
		{
			physicsWorld_->RegisterCollider(b.get());
		}
	}

	if (collisionManager_) collisionManager_->AddCollider(b.get());

	Bullet* raw = b.get();
	bullets_.push_back(std::move(b));

	if (typeId == static_cast<uint32_t>(CollisionTypeIdDef::kBullet))
	{
		const Vector3 fallbackForward = Vector3::Normalize(dirNormalized);
		const float maxTracerDistance = std::max(0.0f, speed) * std::max(0.0f, lifeTimeSec);
		Vector3 tracerEndPosition = startPos + fallbackForward * maxTracerDistance;

		if (collisionManager_ && maxTracerDistance > 0.0f)
		{
			RaycastQuery tracerQuery{};
			tracerQuery.origin = startPos;
			tracerQuery.direction = fallbackForward;
			tracerQuery.maxDistance = maxTracerDistance;
			tracerQuery.traceChannel = ETraceChannel::Weapon;

			RaycastHit tracerHit{};
			if (collisionManager_->RaycastSingle(tracerQuery, tracerHit) && tracerHit.hit)
			{
				tracerEndPosition = tracerHit.point;
			}
		}

		Vector3 effectPosition = startPos + fallbackForward * 0.35f;
		Vector3 effectDirection = fallbackForward;
		if (shotEffectTransformResolver_)
		{
			Vector3 resolvedPosition{};
			Vector3 resolvedDirection{};
			if (shotEffectTransformResolver_(resolvedPosition, resolvedDirection))
			{
				effectPosition = resolvedPosition;
				effectDirection = Vector3::Normalize(resolvedDirection); // ViewModelの銃口座標を使い、カメラ中心の顔位置からVFXを出さない。
			}
		}
		if (maxTracerDistance <= 0.0f)
		{
			tracerEndPosition = effectPosition + effectDirection;
		}
		EmitPlayerWeaponFireEffects(effectPosition, tracerEndPosition);
	}
	return raw;
}

void BulletManager::SetWorldImpactCallback(std::function<void(const Vector3&, const Vector3&)> callback)
{
	worldImpactCallback_ = std::move(callback);
	// すでに飛翔中の弾にも新しい通知先を反映し、Scene再接続時の古い参照を残さない。
	for (auto& bullet : bullets_)
	{
		if (bullet)
		{
			bullet->SetWorldImpactCallback(worldImpactCallback_);
		}
	}
}

void BulletManager::Update(float dt)
{
	for (auto& b : bullets_) b->Update(dt);

	// 寿命切れ/衝突済みの弾はCollider解除後に管理対象から外し、Update/Draw負荷を残さない。
	const auto removeBegin = std::remove_if(bullets_.begin(), bullets_.end(), [this](const std::unique_ptr<Bullet>& bullet)
		{
			if (!bullet || !bullet->IsRemovable())
			{
				return false;
			}

			if (bullet->UsesPhysicsTrigger() && bullet->HasPhysicsHit())
			{
				++physicsTriggerHitCount_;
			}
			if (physicsWorld_ && bullet->UsesPhysicsTrigger())
			{
				// 破棄済みBullet Collider参照を防ぐため、管理対象から外す前にPhysicsWorld登録を解除する。
				physicsWorld_->UnregisterCollider(bullet.get());
			}
			if (collisionManager_) collisionManager_->RemoveCollider(bullet.get());
			return true;
		});

	bullets_.erase(removeBegin, bullets_.end());
}

size_t BulletManager::GetActiveCount() const
{
	return static_cast<size_t>(std::count_if(bullets_.begin(), bullets_.end(), [](const std::unique_ptr<Bullet>& bullet)
		{
			return bullet && !bullet->IsDead() && !bullet->IsRemovable();
		}));
}

size_t BulletManager::GetPhysicsTriggerBulletCount() const
{
	return static_cast<size_t>(std::count_if(bullets_.begin(), bullets_.end(), [](const std::unique_ptr<Bullet>& bullet)
		{
			return bullet && bullet->UsesPhysicsTrigger() && !bullet->IsRemovable();
		}));
}

void BulletManager::AppendCollisionSoABullets(BulletEnemyCollisionSoA& collisionSoA) const
{
	for (const auto& bullet : bullets_)
	{
		if (!bullet || bullet->IsDead() || bullet->IsRemovable())
		{
			continue;
		}

		// 描画や寿命管理はBullet側に残し、判定に必要な位置・半径・ダメージだけをSoAへ転送する。
		collisionSoA.AddBullet(
			bullet->GetCenterPosition(),
			bullet->GetCollisionRadius(),
			bullet->GetDamage(),
			true);
	}
}

void BulletManager::SetPhysicsTriggerWorld(PhysicsWorld* physicsWorld, uint32_t playerBulletLayer)
{
	physicsWorld_ = physicsWorld;
	playerBulletLayer_ = playerBulletLayer;
	RefreshPhysicsTriggerRegistrations();
}

void BulletManager::SetUsePhysicsTriggerForNormalBullets(bool enabled)
{
	usePhysicsTriggerForNormalBullets_ = enabled;
	RefreshPhysicsTriggerRegistrations();
}

void BulletManager::RefreshPhysicsTriggerRegistrations()
{
	for (auto& bullet : bullets_)
	{
		if (!bullet)
		{
			continue;
		}

		const bool wasUsingPhysics = bullet->UsesPhysicsTrigger();
		if (wasUsingPhysics && physicsWorld_)
		{
			physicsWorld_->UnregisterCollider(bullet.get());
		}

		bullet->SetUsePhysicsTrigger(usePhysicsTriggerForNormalBullets_);
		if (bullet->UsesPhysicsTrigger())
		{
			bullet->SetCollisionLayer(playerBulletLayer_);
			if (physicsWorld_ && !bullet->IsRemovable())
			{
				physicsWorld_->RegisterCollider(bullet.get());
			}
		}
	}
}

void BulletManager::Draw()
{
	for (auto& b : bullets_) b->Draw();
}

void BulletManager::DrawImGui()
{
	for (auto& b : bullets_) b->DrawImGui();
}

void BulletManager::Clear()
{
	if (collisionManager_)
	{
		for (auto& b : bullets_) collisionManager_->RemoveCollider(b.get());
	}
	if (physicsWorld_)
	{
		for (auto& b : bullets_)
		{
			if (b && b->UsesPhysicsTrigger())
			{
				// 破棄済みBullet Collider参照を防ぐため、Scene終了/一括Clear時にPhysicsWorld登録を解除する。
				physicsWorld_->UnregisterCollider(b.get());
			}
		}
	}
	bullets_.clear();
}
