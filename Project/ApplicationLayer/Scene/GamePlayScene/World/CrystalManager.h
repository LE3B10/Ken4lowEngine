#pragma once

#include "EnemySpawnCrystal.h"

#include <cstddef>
#include <vector>

class CharacterWorld;
class CollisionManager;

/// 複数クリスタルの進行、雑魚敵生成要求、ボス通知を集約する。
class CrystalManager
{
public:
	void Initialize(const std::vector<CrystalSpawnPoint>& spawnPoints, CollisionManager* collisionManager = nullptr);
	void Update(CharacterWorld& characters, float deltaTime);
	void Draw() const;
	void DrawImGui();

	int GetCrystalCount() const { return static_cast<int>(crystals_.size()); }
	int GetAliveCrystalCount() const;
	int GetAliveCrystalSpawnEnemyCount() const;
	bool AreAllCrystalsDestroyed() const;
	bool ShouldSpawnBoss() const { return shouldSpawnBoss_; }
	bool HasBossSpawned() const { return hasBossSpawned_; }
	static constexpr float GetMaxUpdateDeltaTime() { return kMaxUpdateDeltaTime; }

private:
	EnemySpawnCrystal* GetSelectedCrystal();
	const EnemySpawnCrystal* GetSelectedCrystal() const;
	EnemySpawnCrystal* FindNextSpawnableCrystal();
	void NotifyBossSpawnIfNeeded();

private:
	static constexpr float kMaxUpdateDeltaTime = 1.0f / 30.0f;
	std::vector<EnemySpawnCrystal> crystals_;
	CollisionManager* collisionManager_ = nullptr;
	size_t selectedCrystalIndex_ = 0;
	size_t nextSpawnCrystalIndex_ = 0;
	bool enableCrystalEnemySpawn_ = true;
	int maxTotalCrystalSpawnEnemies_ = 9;
	float globalSpawnInterval_ = 5.0f;
	float globalSpawnTimer_ = 0.0f;
	int maxSpawnPerInterval_ = 1;
	bool shouldSpawnBoss_ = false;
	bool hasBossSpawned_ = false;
};
