#include "BulletManager.h"
#include "Bullet.h"
#include "CollisionManager.h"

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
	float lifeTimeSec)
{
	auto b = std::make_unique<Bullet>();
	b->Initialize(startPos, dirNormalized * speed, damage, lifeTimeSec);

	if (collisionManager_) collisionManager_->AddCollider(b.get());

	Bullet* raw = b.get();
	bullets_.push_back(std::move(b));
	return raw;
}

void BulletManager::Update(float dt)
{
	for (auto& b : bullets_) b->Update(dt);

	for (size_t i = 0; i < bullets_.size(); )
	{
		if (bullets_[i]->IsRemovable())
		{
			if (collisionManager_) collisionManager_->RemoveCollider(bullets_[i].get());
			bullets_.erase(bullets_.begin() + i);
		}
		else
		{
			++i;
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
	bullets_.clear();
}