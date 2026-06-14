#include "BulletManager.h"
#include "Bullet.h"
#include "CollisionManager.h"
#include "Engine/Physics/Core/PhysicsWorld.h"

#include <algorithm>

using namespace Ken4lowEngine;

void BulletManager::Initialize(CollisionManager* collisionManager)
{
	collisionManager_ = collisionManager;
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
	float splashRadius,
	int splashDamage,
	bool splashCanDamageSelf,
	bool drawModel,
	int32_t weaponID,
	EWeaponCategory weaponCategory,
	EDeathKnockbackType deathType,
	float deathPower,
	float deathUpPower,
	float deathExplosionRadius,
	float deathImpulseScale
)
{
	auto b = std::make_unique<Bullet>();
	b->Initialize(startPos, dirNormalized * speed, damage, lifeTimeSec, shooterPosition, shooterColliderId, typeId);
	b->SetModelDrawEnabled(drawModel);
	b->SetCollisionManager(collisionManager_);
	b->ConfigureSplashDamage(splashRadius, splashDamage, splashCanDamageSelf);
	b->SetWeaponMetadata(weaponID, weaponCategory, deathType, deathPower, deathUpPower, deathExplosionRadius, deathImpulseScale);
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
	return raw;
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
