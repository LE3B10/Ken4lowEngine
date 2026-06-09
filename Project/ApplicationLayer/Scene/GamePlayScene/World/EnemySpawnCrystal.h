#pragma once

#include "EnemyType.h"
#include "Collider.h"
#include "Object3D.h"
#include "Vector3.h"
#include "AABB.h"

#include <memory>
#include <string>
#include <vector>

class CharacterWorld;
class EnemyBase;

namespace K4E = ::Ken4lowEngine;

/// Blender のスポーン設定を将来そのまま受け取るためのクリスタル定義。
struct CrystalSpawnPoint
{
	std::string crystalName = "Crystal_01";
	bool isActive = true;
	K4E::Vector3 position{};
	K4E::Vector3 rotation{};
	K4E::Vector3 scale{ 1.0f, 1.0f, 1.0f };
	int hp = 1000;
	int maxHp = 1000;
	EnemyType spawnEnemyType = EnemyType::Melee;
	float spawnInterval = 5.0f;
	float initialDelay = 0.0f;
	int maxSpawnCount = 0; // 0以下なら破壊まで無限スポーン。
	int maxAliveEnemies = 9;
	float spawnRadius = 3.0f;
	std::string spawnPattern = "Interval";
	bool enableInfiniteSpawn = true;
	bool spawnBossTrigger = true;
};

/// -------------------------------------------------------------
/// 破壊されるまで、上限付きで雑魚敵を生成し続けるクリスタル。
///
/// クリスタル本体のTransform、Collider、敵スポーン基準座標を同じ
/// position_から同期し、「クリスタル＝スポナー」として扱う。
/// -------------------------------------------------------------
class EnemySpawnCrystal : public K4E::Collider
{
public:
	void Initialize(const CrystalSpawnPoint& spawnPoint, const std::vector<K4E::AABB>* floorAABBs = nullptr, const std::vector<K4E::AABB>* obstacleAABBs = nullptr);
	void Update(const CharacterWorld& characters);
	void Draw() const;
	void ApplyDamage(int damage);
	void TakeDamage(int damage) { ApplyDamage(damage); }
	void OnCollisionEnter(K4E::Collider* other) override;
	bool CanSpawnEnemy() const;
	void SpawnEnemy(CharacterWorld& characters);
	void ApplySpawnerSettings(const CrystalSpawnPoint& spawnPoint, const std::vector<K4E::AABB>* floorAABBs = nullptr, const std::vector<K4E::AABB>* obstacleAABBs = nullptr);
	void ApplyInitialHpSettings(const CrystalSpawnPoint& spawnPoint);
	void AdvanceSpawnTimer(float deltaTime);
	bool IsSpawnReady() const;
	void ConsumeSpawnTimer();
	void ResetSpawnRuntime();

	bool IsAlive() const { return isActive_ && isAlive; }
	bool IsDestroyed() const { return !IsAlive(); }
	bool IsActive() const { return isActive_; }
	const std::string& GetCrystalName() const { return crystalName_; }
	bool IsColliderEnabled() const { return IsAlive(); }
	int GetHitCount() const { return hitCount_; }
	int GetHp() const { return hp; }
	int GetMaxHp() const { return maxHp; }
	float GetHpRate() const { return maxHp > 0 ? static_cast<float>(hp) / static_cast<float>(maxHp) : 0.0f; }
	EnemyType GetSpawnEnemyType() const { return spawnEnemyType; }
	float GetSpawnInterval() const { return spawnInterval_; }
	float GetInitialDelay() const { return initialDelay_; }
	int GetMaxSpawnCount() const { return maxSpawnCount_; }
	float GetSpawnRadius() const { return spawnRadius; }
	int GetMaxAliveEnemies() const { return maxAliveEnemies; }
	int GetTotalSpawnedCount() const { return totalSpawnedCount; }
	int GetAliveSpawnedEnemyCount() const { return aliveSpawnedEnemyCount; }
	const std::string& GetSpawnPattern() const { return spawnPattern_; }
	bool IsInfiniteSpawnEnabled() const { return enableInfiniteSpawn; }
	bool IsBossSpawnTrigger() const { return spawnBossTrigger_; }
	const K4E::Vector3& GetPosition() const { return position_; }
	const K4E::Vector3& GetRotation() const { return rotation_; }
	const K4E::Vector3& GetScale() const { return scale_; }
	float GetSpawnTimer() const { return spawnTimer_; }
	bool IsInitialDelayElapsed() const { return initialDelayElapsed_; }
	static constexpr bool IsGroundSnapEnabled() { return true; }
	static float GetSpawnYOffset() { return s_spawnYOffset_; }
	static void SetSpawnYOffset(float offset);

	void SetInfiniteSpawnEnabled(bool enabled) { enableInfiniteSpawn = enabled; }
	void SetSpawnEnemyType(EnemyType enemyType) { spawnEnemyType = enemyType; }
	void SetMaxAliveEnemies(int count);

public: // Blender 読み込み対応時にも維持するランタイム設定値。
	bool isAlive = true;
	int hp = 1000;
	int maxHp = 1000;
	EnemyType spawnEnemyType = EnemyType::Melee;
	float spawnRadius = 3.0f;
	int maxAliveEnemies = 9;
	int totalSpawnedCount = 0;
	int aliveSpawnedEnemyCount = 0;
	bool enableInfiniteSpawn = true;

private:
	void RemoveInactiveSpawnedEnemies(const CharacterWorld& characters);
	void SyncTransformToRuntime(const std::vector<K4E::AABB>* floorAABBs, const std::vector<K4E::AABB>* obstacleAABBs);

private:
	std::string crystalName_ = "Crystal_01";
	bool isActive_ = true;
	K4E::Vector3 position_{};
	K4E::Vector3 rotation_{};
	K4E::Vector3 scale_{ 1.0f, 1.0f, 1.0f };
	bool spawnBossTrigger_ = true;
	float spawnInterval_ = 5.0f;
	float initialDelay_ = 0.0f;
	int maxSpawnCount_ = 0;
	std::string spawnPattern_ = "Interval";
	float spawnTimer_ = 0.0f;
	bool initialDelayElapsed_ = false;
	int hitCount_ = 0;
	std::unique_ptr<K4E::Object3D> debugCube_;
	std::vector<const EnemyBase*> spawnedEnemies_;
	static float s_spawnYOffset_;
};
