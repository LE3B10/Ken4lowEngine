#pragma once
#include <array>
#include <functional>
#include <memory>
#include <vector>
#include <string>
#include <unordered_set>

#include <ActorWorld.h>
#include <PhysicsWorld.h>

#include "ApplicationLayer/Character/Player/Migration/GamePlayPlayerMigrationRuntime.h"
#include "EnemyBase.h"
#include "EnemyType.h"
#include "EnemyParticleEffectSystem.h"

class CollisionManager;
class BulletManager;

namespace K4E = ::Ken4lowEngine;

struct GameContext
{
	CollisionManager* collisionManager_ = nullptr;
	BulletManager* bulletManager_ = nullptr;
};

/// -------------------------------------------------------------
/// GamePlayのPlayerActorと旧通常Enemyを保持・更新する段階統合World。
/// -------------------------------------------------------------
class CharacterWorld
{
private:
	struct EnemySpawnRequest
	{
		K4E::Vector3 position = {};
		float yawRad = 0.0f;
		float maxHp = 240.0f;
		EnemyType enemyType = EnemyType::Melee;
	};

public:
	void Initialize(GameContext& ctx);
	void Finalize();
	void Update(float dt);
	void UpdatePlayerOnly(float dt);
	void WarmupStartGameplayVisuals();
	void SetStartGameplayVisualsVisible(bool visible);
	void SetPlayerSpawnPosition(const K4E::Vector3& position);
	void Draw();
	void DrawImGui();
	void DrawPlayerDebugImGui();
	void DrawEnemyDebugImGui();

	void DrawShadow()
	{
		if (playerRuntimeController_ && playerRuntimeController_->IsActive()) playerRuntimeController_->DrawShadow();
		for (auto& enemy : enemies_) if (enemy) enemy->DrawShadow();
	}

	void UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection)
	{
		if (playerRuntimeController_ && playerRuntimeController_->IsActive()) playerRuntimeController_->PrepareRenderState();
		for (auto& enemy : enemies_) if (enemy) enemy->UpdateShadowMatrix(lightViewProjection);
	}

	K4E::PlayerActor* GetPlayer() { return playerRuntimeController_ ? playerRuntimeController_->GetPlayer() : nullptr; }
	const K4E::PlayerActor* GetPlayer() const { return playerRuntimeController_ ? playerRuntimeController_->GetPlayer() : nullptr; }
	IPlayerRuntime* GetPlayerRuntime() { return playerRuntimeController_ ? playerRuntimeController_->GetPlayerRuntime() : nullptr; }
	const IPlayerRuntime* GetPlayerRuntime() const { return playerRuntimeController_ ? playerRuntimeController_->GetPlayerRuntime() : nullptr; }

	K4E::ActorWorld& GetActorWorld() { return actorWorld_; }
	const K4E::ActorWorld& GetActorWorld() const { return actorWorld_; }
	K4E::PhysicsWorld& GetPhysicsWorld() { return physicsWorld_; }
	const K4E::PhysicsWorld& GetPhysicsWorld() const { return physicsWorld_; }

	const std::vector<std::unique_ptr<EnemyBase>>& GetEnemies() const { return enemies_; }
	std::vector<EnemyBase*> GetEnemyRawList() const;
	EnemyBase& SpawnEnemy(const EnemySpawnRequest& request);
	EnemyBase& SpawnEnemyAt(const K4E::Vector3& position, EnemyType enemyType = EnemyType::Melee);
	void ClearEnemies();
	bool RemoveEnemy(EnemyBase* enemy);
	void SetEnemyKilledCallback(std::function<void(const K4E::Vector3&)> callback) { onEnemyKilled_ = std::move(callback); }

	int GetEnemyCount() const { return static_cast<int>(enemies_.size()); }
	int GetAliveNormalEnemyCount() const;

	void SetDebug(bool on) { isDebug_ = on; }
	bool IsDebug() const { return isDebug_; }

private:
	void EnsurePlayerRuntime();
	void UpdateActivePlayer(float dt);
	void InjectEnemyDeps(EnemyBase& e);
	void RegisterPlayerCollisionBridge();
	void UnregisterPlayerCollisionBridge();

private:
	GameContext ctx_{};
	K4E::PhysicsWorld physicsWorld_{};
	K4E::ActorWorld actorWorld_{};
	std::unique_ptr<GamePlayPlayerMigrationRuntime> playerRuntimeController_;
	std::vector<std::unique_ptr<EnemyBase>> enemies_;
	EnemyParticleEffectSystem enemyParticleEffectSystem_;
	std::function<void(const K4E::Vector3&)> onEnemyKilled_{};
	std::unordered_set<const EnemyBase*> notifiedKilledEnemies_;
	std::array<int, 2> spawnedEnemyCounts_{};
	K4E::Vector3 playerSpawnPosition_{ 0.0f, 2.25f, 0.0f };
	bool playerCollisionBridgeRegistered_ = false;
	bool isDebug_ = false;
	EnemyType debugSpawnEnemyType_ = EnemyType::Melee;
};
