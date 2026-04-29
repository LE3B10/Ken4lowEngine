#include "CharacterWorld.h"
#include "CollisionManager.h"
#include "BulletManager.h"
#include <algorithm>

#include "WorldCollisionResolver.h"
#include "AudioManager.h"
#include <iostream>
#include <limits>
#include <sstream>

using namespace Ken4lowEngine;

namespace
{
	Vector3 ClampToAABB(const Vector3& p, const AABB& aabb)
	{
		return {
			std::clamp(p.x, aabb.min.x, aabb.max.x),
			std::clamp(p.y, aabb.min.y, aabb.max.y),
			std::clamp(p.z, aabb.min.z, aabb.max.z)
		};
	}

	Vector3 StabilizeSpawnPosition(const Vector3& requested, Enemy& enemy)
	{
		const auto* worldAabbs = enemy.GetResolvedWorldAABBs();
		if (!worldAabbs || worldAabbs->empty())
		{
			return requested;
		}

		float bestDistSq = std::numeric_limits<float>::max();
		Vector3 bestPos = requested;
		for (const auto& aabb : *worldAabbs)
		{
			const Vector3 clamped = ClampToAABB(requested, aabb);
			const Vector3 diff = requested - clamped;
			const float distSq = Vector3::Dot(diff, diff);
			if (distSq < bestDistSq)
			{
				bestDistSq = distSq;
				bestPos = clamped;
			}
		}

		return bestPos;
	}
}

void CharacterWorld::Initialize(GameContext& ctx)
{
	ctx_ = ctx;

	// 敵の被弾エフェクトシステムを初期化
	enemyParticleEffectSystem_.Initialize();

	// --- Player ---
	player_ = std::make_unique<Player>();
	InjectPlayerDeps(*player_);
	player_->Initialize();

	// 実際の開始位置は Scene 側で決める
	player_->SetSpawnOffset({ 0.0f, 0.0f, 0.0f });

	if (ctx_.collisionManager_)
	{
		ctx_.collisionManager_->AddCollider(player_.get());
	}

	enemies_.clear();
}

void CharacterWorld::Finalize()
{
	ClearEnemies();

	if (ctx_.collisionManager_ && player_)
	{
		ctx_.collisionManager_->RemoveCollider(player_.get());
	}
	player_.reset();
	ctx_ = GameContext{};
}

void CharacterWorld::InjectPlayerDeps(Player& p)
{
	p.SetCollisionManager(ctx_.collisionManager_);
	p.SetBulletManager(ctx_.bulletManager_);

	p.SetOnHitSECallback([]()
		{
			Ken4lowEngine::AudioManager::GetInstance()->PlaySE("enemy_hit.mp3", 0.2f);
		});

	p.SetOnFireSECallback([]()
		{
			Ken4lowEngine::AudioManager::GetInstance()->PlaySE("player_fire.mp3", 0.1f);
		});

	p.SetOnReloadSECallback([]()
		{
			Ken4lowEngine::AudioManager::GetInstance()->PlaySE("enemy_reload.mp3", 0.2f);
		});

	p.SetOnDeathSECallback([]()
		{
			Ken4lowEngine::AudioManager::GetInstance()->PlaySE("enemy_death.mp3", 0.2f);
		});
}

void CharacterWorld::InjectEnemyDeps(Enemy& e)
{
	e.SetCollisionManager(ctx_.collisionManager_);
	e.SetBulletManager(ctx_.bulletManager_);

	e.SetParticleEffectSystem(&enemyParticleEffectSystem_);

	if (player_)
	{
		e.SetTarget(player_.get());
	}
}

std::vector<EnemyBase*> CharacterWorld::GetEnemyRawList() const
{
	std::vector<EnemyBase*> result;
	result.reserve(enemies_.size());

	for (const auto& enemy : enemies_)
	{
		result.push_back(enemy.get());
	}

	return result;
}

Enemy& CharacterWorld::SpawnEnemy(const EnemySpawnRequest& request)
{
	auto e = std::make_unique<Enemy>();
	InjectEnemyDeps(*e);
	e->Initialize();
	const Vector3 stabilizedSpawn = StabilizeSpawnPosition(request.position, *e);
	e->SetPosition(stabilizedSpawn);

#ifdef _DEBUG
	std::ostringstream oss;
	oss << "[Spawn] requested=(" << request.position.x << "," << request.position.y << "," << request.position.z
		<< ") stabilized=(" << stabilizedSpawn.x << "," << stabilizedSpawn.y << "," << stabilizedSpawn.z << ")\n";
	std::cout << oss.str();
#endif

	if (ctx_.collisionManager_)
	{
		ctx_.collisionManager_->AddCollider(e.get());
	}

	enemies_.push_back(std::move(e));
	return *enemies_.back();
}

Enemy& CharacterWorld::SpawnEnemyAt(const K4E::Vector3& position)
{
	EnemySpawnRequest request{};
	request.position = position;
	return SpawnEnemy(request);
}

void CharacterWorld::ClearEnemies()
{
	if (ctx_.collisionManager_)
	{
		for (auto& e : enemies_)
		{
			ctx_.collisionManager_->RemoveCollider(e.get());
		}
	}
	enemies_.clear();
}

void CharacterWorld::Update(float dt)
{
	if (player_) player_->Update(dt);

	for (auto& e : enemies_)
	{
		e->Update(dt);
	}

	if (ctx_.collisionManager_)
	{
		enemies_.erase(
			std::remove_if(enemies_.begin(), enemies_.end(),
				[&](const std::unique_ptr<Enemy>& e)
				{
					if (e && e->IsRemovable())
					{
						ctx_.collisionManager_->RemoveCollider(e.get());
						return true;
					}
					return false;
				}),
			enemies_.end());
	}
}

void CharacterWorld::Draw()
{
	if (player_) player_->Draw();
	for (auto& e : enemies_) e->Draw();
}

void CharacterWorld::DrawImGui()
{
#ifdef USE_IMGUI
	// --------------------------------------------------------
	// 1) Player の ImGui
	// --------------------------------------------------------
	if (player_) { player_->DrawImGui(); }

	// --------------------------------------------------------
	// 2) Enemy個体ごとの ImGui
	// - ここでは各敵の Object3D / Transform などだけ出す
	// - EnemyTuningEditor は出さない
	// --------------------------------------------------------
	for (auto& e : enemies_) { e->DrawImGui(); }

	// --------------------------------------------------------
	// 3) EnemyTuningEditor は 1回だけ描画する
	// - 保存 / 削除 / Reload 後に全Enemyへ再反映する
	// --------------------------------------------------------
#endif
}

void CharacterWorld::DrawShadow()
{
	if (player_) { player_->DrawShadow(); }
	for (auto& e : enemies_)
	{
		e->DrawShadow();
	}
}

void CharacterWorld::UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection)
{
	if (player_) { player_->UpdateShadowMatrix(lightViewProjection); }

	for (auto& e : enemies_)
	{
		e->UpdateShadowMatrix(lightViewProjection);
	}
}
