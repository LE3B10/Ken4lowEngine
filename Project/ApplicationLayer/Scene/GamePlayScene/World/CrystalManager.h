#pragma once

#include "EnemySpawnCrystal.h"

#include <cstddef>
#include <vector>

#include "Matrix4x4.h"

class CharacterWorld;
class CollisionManager;

/// 複数クリスタルの進行、雑魚敵生成要求、ボス通知を集約する。
class CrystalManager
{
public:
	void Initialize(const std::vector<CrystalSpawnPoint>& spawnPoints, CollisionManager* collisionManager = nullptr, const std::vector<K4E::AABB>* floorAABBs = nullptr, const std::vector<K4E::AABB>* obstacleAABBs = nullptr);
	void Update(CharacterWorld& characters, float deltaTime);
	void Draw() const;
	void DrawHpBars(const K4E::Matrix4x4& viewMatrix, const K4E::Matrix4x4& projMatrix, float screenWidth, float screenHeight) const;
	void DrawImGui();

	int GetCrystalCount() const { return static_cast<int>(crystals_.size()); }
	int GetAliveCrystalCount() const;
	int GetAliveCrystalSpawnEnemyCount() const;
	bool AreAllCrystalsDestroyed() const;
	bool IsCrystalEnemySpawnEnabled() const { return enableCrystalEnemySpawn_; }
	void SetProgressDebugStatus(int aliveNormalEnemyCount, bool bossSpawnConditionMet, bool bossSpawned, const K4E::Vector3& bossSpawnPosition);
	static constexpr float GetMaxUpdateDeltaTime() { return kMaxUpdateDeltaTime; }

private:
	EnemySpawnCrystal* GetSelectedCrystal();
	const EnemySpawnCrystal* GetSelectedCrystal() const;
	EnemySpawnCrystal* FindNextSpawnableCrystal();

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
	int debugAliveNormalEnemyCount_ = 0;
	bool debugBossSpawnConditionMet_ = false;
	bool debugBossSpawned_ = false;
	K4E::Vector3 debugBossSpawnPosition_{};
};
