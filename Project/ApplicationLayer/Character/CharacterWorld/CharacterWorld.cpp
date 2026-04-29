#define NOMINMAX
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
	constexpr float kSpawnPaddingXZ = 0.35f;
	constexpr float kSpawnLiftY = 0.05f;
	constexpr float kSpawnSpreadStep = 1.5f;

	Vector3 ClampToAABB(const Vector3& p, const AABB& aabb, float paddingXZ = 0.0f)
	{
		const float minX = aabb.min.x + paddingXZ;
		const float maxX = aabb.max.x - paddingXZ;
		const float minZ = aabb.min.z + paddingXZ;
		const float maxZ = aabb.max.z - paddingXZ;
		return {
			std::clamp(p.x, std::min(minX, maxX), std::max(minX, maxX)),
			std::clamp(p.y, aabb.min.y, aabb.max.y),
			std::clamp(p.z, std::min(minZ, maxZ), std::max(minZ, maxZ))
		};
	}

	Vector3 BuildSpawnSpreadOffset(size_t spawnSerial)
	{
		if (spawnSerial == 0) { return {0.0f, 0.0f, 0.0f}; }
		const int layer = static_cast<int>((spawnSerial - 1) / 8) + 1;
		const int dir = static_cast<int>((spawnSerial - 1) % 8);
		const float d = static_cast<float>(layer) * kSpawnSpreadStep;
		switch (dir)
		{
		case 0: return { d,0,0};
		case 1: return {-d,0,0};
		case 2: return {0,0,d};
		case 3: return {0,0,-d};
		case 4: return { d,0,d};
		case 5: return {-d,0,d};
		case 6: return { d,0,-d};
		default:return {-d,0,-d};
		}
	}

	Vector3 StabilizeSpawnPosition(const Vector3& requested, Enemy& enemy, size_t spawnSerial, bool* outInside)
	{
		const auto* worldAabbs = enemy.GetResolvedWorldAABBs();
		if (!worldAabbs || worldAabbs->empty())
		{
			if (outInside) { *outInside = false; }
			return requested;
		}
		const Vector3 spreadRequested = requested + BuildSpawnSpreadOffset(spawnSerial);
		float bestDistSq = std::numeric_limits<float>::max();
		Vector3 bestPos = spreadRequested;
		bool insideAny = false;
		for (const auto& aabb : *worldAabbs)
		{
			const bool inside = (spreadRequested.x >= aabb.min.x && spreadRequested.x <= aabb.max.x &&
				spreadRequested.y >= aabb.min.y && spreadRequested.y <= aabb.max.y &&
				spreadRequested.z >= aabb.min.z && spreadRequested.z <= aabb.max.z);
			insideAny = insideAny || inside;
			Vector3 clamped = ClampToAABB(spreadRequested, aabb, kSpawnPaddingXZ);
			clamped.y = aabb.max.y + kSpawnLiftY;
			const Vector3 diff = spreadRequested - clamped;
			const float distSq = Vector3::Dot(diff, diff);
			if (distSq < bestDistSq)
			{
				bestDistSq = distSq;
				bestPos = clamped;
			}
		}
		if (outInside) { *outInside = insideAny; }
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
	bool wasInsideStage = false;
	const size_t spawnSerial = enemies_.size();
	const Vector3 stabilizedSpawn = StabilizeSpawnPosition(request.position, *e, spawnSerial, &wasInsideStage);
	e->SetPosition(stabilizedSpawn);
	e->SetVelocity({ 0.0f, 0.0f, 0.0f });

#ifdef _DEBUG
	std::ostringstream oss;
	oss << "[Spawn] requested=(" << request.position.x << "," << request.position.y << "," << request.position.z
		<< ") stabilized=(" << stabilizedSpawn.x << "," << stabilizedSpawn.y << "," << stabilizedSpawn.z
		<< ") insideStage=" << (wasInsideStage ? 1 : 0)
		<< " waveSpawnSerial=" << spawnSerial
		<< "\n";
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
