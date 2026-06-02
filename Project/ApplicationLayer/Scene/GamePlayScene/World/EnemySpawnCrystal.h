#pragma once

#include "EnemyType.h"
#include "Object3D.h"
#include "Vector3.h"

#include <memory>
#include <vector>

class CharacterWorld;
class EnemyBase;

namespace K4E = ::Ken4lowEngine;

/// Blender のスポーン設定を将来そのまま受け取るためのクリスタル定義。
struct CrystalSpawnPoint
{
	K4E::Vector3 position{};
	K4E::Vector3 rotation{};
	K4E::Vector3 scale{ 1.0f, 1.0f, 1.0f };
	int hp = 100;
	EnemyType spawnEnemyType = EnemyType::Melee;
	float spawnInterval = 2.0f;
	int maxAliveEnemies = 10;
	float spawnRadius = 3.0f;
	bool enableInfiniteSpawn = true;
	bool spawnBossTrigger = true;
};

/// 破壊されるまで、上限付きで雑魚敵を生成し続ける仮クリスタル。
class EnemySpawnCrystal
{
public:
	void Initialize(const CrystalSpawnPoint& spawnPoint);
	void Update(CharacterWorld& characters, float deltaTime);
	void Draw() const;
	void TakeDamage(int damage);

	bool IsAlive() const { return isAlive; }
	int GetHp() const { return hp; }
	EnemyType GetSpawnEnemyType() const { return spawnEnemyType; }
	float GetSpawnInterval() const { return spawnInterval; }
	float GetSpawnRadius() const { return spawnRadius; }
	int GetMaxAliveEnemies() const { return maxAliveEnemies; }
	int GetTotalSpawnedCount() const { return totalSpawnedCount; }
	int GetAliveSpawnedEnemyCount() const { return aliveSpawnedEnemyCount; }
	bool IsInfiniteSpawnEnabled() const { return enableInfiniteSpawn; }
	bool IsBossSpawnTrigger() const { return spawnBossTrigger_; }
	const K4E::Vector3& GetPosition() const { return position_; }

	void SetInfiniteSpawnEnabled(bool enabled) { enableInfiniteSpawn = enabled; }
	void SetSpawnEnemyType(EnemyType enemyType) { spawnEnemyType = enemyType; }
	void SetSpawnInterval(float interval);
	void SetMaxAliveEnemies(int count);

private:
	void RemoveInactiveSpawnedEnemies(const CharacterWorld& characters);
	void SpawnEnemy(CharacterWorld& characters);

public: // Blender 読み込み対応時にも維持するランタイム設定値。
	bool isAlive = true;
	int hp = 100;
	EnemyType spawnEnemyType = EnemyType::Melee;
	float spawnInterval = 2.0f;
	float spawnTimer = 0.0f;
	float spawnRadius = 3.0f;
	int maxAliveEnemies = 10;
	int totalSpawnedCount = 0;
	int aliveSpawnedEnemyCount = 0;
	bool enableInfiniteSpawn = true;

private:
	K4E::Vector3 position_{};
	bool spawnBossTrigger_ = true;
	std::unique_ptr<K4E::Object3D> debugCube_;
	std::vector<const EnemyBase*> spawnedEnemies_;
};
