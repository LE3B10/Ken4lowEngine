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

#ifdef USE_IMGUI
#include "imgui.h"
#endif

using namespace Ken4lowEngine;

namespace
{
	constexpr float kSpawnGroundProbeStartYOffset = 10.0f;
	constexpr float kSpawnGroundProbeEndYOffset = 120.0f;
	constexpr float kSpawnLiftY = 0.05f;

	bool ResolveSpawnYFromWorldAabb(const std::vector<AABB>* worldAabbs, const Vector3& requestedSpawnPos, float* outResolvedY, float* outHitY)
	{
		if (!worldAabbs || worldAabbs->empty() || !outResolvedY) { return false; }

		const float rayX = requestedSpawnPos.x;
		const float rayZ = requestedSpawnPos.z;
		const float rayStartY = requestedSpawnPos.y + kSpawnGroundProbeStartYOffset;
		const float rayEndY = requestedSpawnPos.y - kSpawnGroundProbeEndYOffset;

		float bestHitY = -std::numeric_limits<float>::infinity();
		bool hit = false;
		for (const auto& aabb : *worldAabbs)
		{
			const bool insideXZ = (rayX >= aabb.min.x && rayX <= aabb.max.x && rayZ >= aabb.min.z && rayZ <= aabb.max.z);
			if (!insideXZ) { continue; }

			const float topY = aabb.max.y;
			if (topY <= rayStartY && topY >= rayEndY && topY > bestHitY)
			{
				bestHitY = topY;
				hit = true;
			}
		}

		if (!hit) { return false; }

		if (outHitY) { *outHitY = bestHitY; }
		*outResolvedY = bestHitY + kSpawnLiftY;
		return true;
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

CharacterWorld::EnemySpawnResult CharacterWorld::SpawnEnemy(const EnemySpawnRequest& request)
{
	EnemySpawnResult result{};
	result.requestedPosition = request.position;
	result.correctedPosition = request.position;
	result.spawnRequestId = request.spawnRequestId;

	auto e = std::make_unique<Enemy>();
	InjectEnemyDeps(*e);
	e->Initialize();

	const auto* worldAabbs = e->GetResolvedWorldAABBs();
	const bool insideStage = worldAabbs && std::any_of(worldAabbs->begin(), worldAabbs->end(), [&](const AABB& aabb)
		{
			return request.position.x >= aabb.min.x && request.position.x <= aabb.max.x &&
				request.position.y >= aabb.min.y && request.position.y <= aabb.max.y &&
				request.position.z >= aabb.min.z && request.position.z <= aabb.max.z;
		});
	result.insideStage = insideStage;

	float resolvedY = request.position.y;
	float hitY = 0.0f;
	const bool groundHit = ResolveSpawnYFromWorldAabb(worldAabbs, request.position, &resolvedY, &hitY);
	result.groundHit = groundHit;
	result.hitY = hitY;
	if (!groundHit)
	{
#ifdef _DEBUG
		std::ostringstream oss;
		oss << "[SpawnRejected] reqId=" << request.spawnRequestId
			<< " requested=(" << request.position.x << "," << request.position.y << "," << request.position.z << ")"
			<< " reason=no_ground_hit insideStage=" << (insideStage ? 1 : 0)
			<< "\n";
		std::cout << oss.str();
#endif
		return result;
	}

	result.correctedPosition.y = resolvedY;
	result.spawnAccepted = true;
	e->SetPosition(result.correctedPosition);
	e->SetVelocity({ 0.0f, 0.0f, 0.0f });

#ifdef _DEBUG
	std::ostringstream oss;
	oss << "[SpawnAccepted] reqId=" << request.spawnRequestId
		<< " requested=(" << request.position.x << "," << request.position.y << "," << request.position.z << ")"
		<< " resolved=(" << result.correctedPosition.x << "," << result.correctedPosition.y << "," << result.correctedPosition.z << ")"
		<< " deltaXZ=(" << (result.correctedPosition.x - request.position.x) << "," << (result.correctedPosition.z - request.position.z) << ")"
		<< " deltaY=" << (result.correctedPosition.y - request.position.y)
		<< " groundHit=1 hitY=" << hitY
		<< " insideStage=" << (insideStage ? 1 : 0)
		<< "\n";
	std::cout << oss.str();
#endif

	if (ctx_.collisionManager_)
	{
		ctx_.collisionManager_->AddCollider(e.get());
	}

	enemies_.push_back(std::move(e));
	result.enemyId = static_cast<int>(enemies_.size()) - 1;
	return result;
}

CharacterWorld::EnemySpawnResult CharacterWorld::SpawnEnemyAt(const K4E::Vector3& position, int spawnRequestId)
{
	EnemySpawnRequest request{};
	request.position = position;
	request.spawnRequestId = spawnRequestId;
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
