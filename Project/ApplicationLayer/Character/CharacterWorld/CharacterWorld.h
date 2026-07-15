#pragma once
#include <array>
#include <functional>
#include <memory>
#include <vector>
#include <string>
#include <unordered_set>

#include <ActorWorld.h>
#include <PhysicsWorld.h>

#include "Player.h"
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
/// キャラクター（Player/Enemy）だけを保持・生成・更新するワールド
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
	void Draw();
	void DrawImGui();
	void DrawPlayerDebugImGui();
	void DrawEnemyDebugImGui();

	/// CharacterのShadow描画は新ActorWorldと旧Enemy互換経路を順番に描画する。
	void DrawShadow()
	{
		if (playerMigrationRuntime_ && playerMigrationRuntime_->IsActive()) playerMigrationRuntime_->DrawShadow();
		else if (player_) player_->DrawShadow();
		for (auto& enemy : enemies_) if (enemy) enemy->DrawShadow();
	}

	/// Light行列の明示同期が必要な旧描画呼び出しも、新ActorWorldの描画状態同期へ渡す。
	void UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection)
	{
		if (playerMigrationRuntime_ && playerMigrationRuntime_->IsActive()) playerMigrationRuntime_->PrepareRenderState();
		else if (player_) player_->UpdateShadowMatrix(lightViewProjection);
		for (auto& enemy : enemies_) if (enemy) enemy->UpdateShadowMatrix(lightViewProjection);
	}

	// P12では新PlayerActorの所有権をCharacterWorld内ActorWorldへ統一し、旧Player参照はP13まで互換Proxyとして残す。
	void SetPlayerRuntimeOverride(IPlayerRuntime* runtime) { playerRuntimeOverride_ = runtime; }
	void SetLegacyPlayerProxyMode(bool enabled) { legacyPlayerProxyMode_ = enabled; }
	bool IsLegacyPlayerProxyMode() const { return legacyPlayerProxyMode_; }
	K4E::PlayerActor* GetMigratedPlayerActor() { return playerMigrationRuntime_ ? playerMigrationRuntime_->GetPlayer() : nullptr; }
	const K4E::PlayerActor* GetMigratedPlayerActor() const { return playerMigrationRuntime_ ? playerMigrationRuntime_->GetPlayer() : nullptr; }

	IPlayerRuntime* GetPlayerRuntime() { return playerRuntimeOverride_ ? playerRuntimeOverride_ : player_.get(); }
	const IPlayerRuntime* GetPlayerRuntime() const { return playerRuntimeOverride_ ? playerRuntimeOverride_ : player_.get(); }

	// P12以降のPlayer/Enemy/Boss移行先となるGamePlayキャラクター用Worldを公開する。
	K4E::ActorWorld& GetActorWorld() { return actorWorld_; }
	const K4E::ActorWorld& GetActorWorld() const { return actorWorld_; }
	K4E::PhysicsWorld& GetPhysicsWorld() { return physicsWorld_; }
	const K4E::PhysicsWorld& GetPhysicsWorld() const { return physicsWorld_; }

	// 旧式の具象Player参照は、新しいRuntime境界へ移行し終えるP13まで互換入口として残す。
	Player* GetPlayer() { return player_.get(); }
	const Player* GetPlayer() const { return player_.get(); }
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
	void EnsurePlayerMigrationRuntime();
	void UpdateActivePlayer(float dt);
	void InjectPlayerDeps(Player& p);
	void InjectEnemyDeps(EnemyBase& e);

private:
	GameContext ctx_{};
	K4E::PhysicsWorld physicsWorld_{}; // ActorWorldより先に構築し、破棄順ではActorWorldを先に終了させる。
	K4E::ActorWorld actorWorld_{};
	std::unique_ptr<Player> player_;
	std::unique_ptr<GamePlayPlayerMigrationRuntime> playerMigrationRuntime_;
	std::vector<std::unique_ptr<EnemyBase>> enemies_;
	EnemyParticleEffectSystem enemyParticleEffectSystem_;
	std::function<void(const K4E::Vector3&)> onEnemyKilled_{};
	std::unordered_set<const EnemyBase*> notifiedKilledEnemies_;
	std::array<int, 2> spawnedEnemyCounts_{};
	IPlayerRuntime* playerRuntimeOverride_ = nullptr;
	bool legacyPlayerProxyMode_ = false;
	bool enablePlayerMigrationRuntime_ = true;
	bool isDebug_ = false;
	EnemyType debugSpawnEnemyType_ = EnemyType::Melee;
};