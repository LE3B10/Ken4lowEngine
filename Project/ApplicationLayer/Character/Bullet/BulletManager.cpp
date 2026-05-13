#include "BulletManager.h"
#include "Bullet.h"
#include "CollisionManager.h"

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
	bool drawModel
)
{
	auto b = std::make_unique<Bullet>();
	b->Initialize(startPos, dirNormalized * speed, damage, lifeTimeSec, shooterPosition, shooterColliderId, typeId);
	b->SetModelDrawEnabled(drawModel);
	b->SetCollisionManager(collisionManager_);
	b->ConfigureSplashDamage(splashRadius, splashDamage, splashCanDamageSelf);

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
	bullets_.clear();
}