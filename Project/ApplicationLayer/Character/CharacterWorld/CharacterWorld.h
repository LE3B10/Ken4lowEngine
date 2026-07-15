#pragma once
#include <array>
#include <functional>
#include <memory>
#include <vector>
#include <string>
#include <unordered_set>

#include "Player.h"
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

	/// CharacterのShadow描画は各ActorのComponent DrawShadow経路へ統一する。
	void DrawShadow()
	{
		if (player_) player_->DrawShadow();
		for (auto& enemy : enemies_) if (enemy) enemy->DrawShadow();
	}

	/// Light行列の明示同期が必要な旧描画呼び出しも、各Characterの共通表示経路へ渡す。
	void UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection)
	{
		if (player_) player_->UpdateShadowMatrix(lightViewProjection);
		for (auto& enemy : enemies_) if (enemy) enemy->UpdateShadowMatrix(lightViewProjection);
	}

	// 新しいGamePlay依存は、可能な範囲から巨大な具象Playerではなく最小Runtime境界を参照する。
	IPlayerRuntime* GetPlayerRuntime() { return player_.get(); }
	const IPlayerRuntime* GetPlayerRuntime() const { return player_.get(); }

	// 旧式の具象Player参照は、新しいRuntime境界へ移行し終えるまで互換入口として残す。
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
	void InjectPlayerDeps(Player& p);
	void InjectEnemyDeps(EnemyBase& e);

private:
	GameContext ctx_{};
	std::unique_ptr<Player> player_;
	std::vector<std::unique_ptr<EnemyBase>> enemies_;
	EnemyParticleEffectSystem enemyParticleEffectSystem_;
	std::function<void(const K4E::Vector3&)> onEnemyKilled_{};
	std::unordered_set<const EnemyBase*> notifiedKilledEnemies_;
	std::array<int, 2> spawnedEnemyCounts_{};
	bool isDebug_ = false;
	EnemyType debugSpawnEnemyType_ = EnemyType::Melee;
};
