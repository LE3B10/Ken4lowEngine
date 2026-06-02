#pragma once

#include "Matrix4x4.h"
#include "Vector3.h"

#include <cstdint>
#include <vector>

namespace Ken4lowEngine
{
	enum class ScalableEnemyType : uint8_t
	{
		Melee,
		MidRange,
	};

	enum class ScalableEnemyState : uint8_t
	{
		Idle,
		Moving,
		Dormant,
	};

	/// StressTest 専用の軽量敵データ。通常 Enemy の AI、攻撃、ナビゲーション、描画オブジェクトは所有しない。
	struct ScalableEnemyData
	{
		Vector3 position{};
		Vector3 rotation{};
		Vector3 scale{ 1.0f, 1.0f, 1.0f };
		Vector3 velocity{};
		ScalableEnemyType enemyType = ScalableEnemyType::Melee;
		ScalableEnemyState state = ScalableEnemyState::Idle;
		float hp = 100.0f;
		bool isActive = true;
		uint32_t updateGroupId = 0;
		float distanceToPlayer = 0.0f;
	};

	/// DrawIndexedInstanced へ移行するときに、そのまま GPU 転送元として使う描画単位。
	struct EnemyInstanceData
	{
		Matrix4x4 worldMatrix = Matrix4x4::MakeIdentity();
		uint32_t enemyIndex = 0;
	};

	struct EnemyUpdateLodSettings
	{
		bool useEnemyUpdateLod = true;
		float nearUpdateDistance = 20.0f;
		float midUpdateDistance = 50.0f;
		float farUpdateDistance = 100.0f;
		uint32_t nearUpdateInterval = 1;
		uint32_t midUpdateInterval = 4;
		uint32_t farUpdateInterval = 12;
	};

	struct EnemyScalabilityMetrics
	{
		uint32_t totalEnemyCount = 0;
		uint32_t meleeEnemyCount = 0;
		uint32_t midRangeEnemyCount = 0;
		uint32_t updatedEnemyCountThisFrame = 0;
		uint32_t skippedEnemyCountThisFrame = 0;
		uint32_t drawEnemyCount = 0;
		uint32_t collisionCheckCount = 0;
		float enemyUpdateTimeMs = 0.0f;
		float enemyDrawSubmitTimeMs = 0.0f;
		float enemyCollisionTimeMs = 0.0f;
	};

	/// 大量敵の検証データを通常 Enemy と分離し、更新 LOD・簡易衝突・描画バッチを一括管理する。
	class EnemyScalabilitySystem
	{
	public:
		void Update(float deltaTime, const Vector3& playerPosition);
		void Clear();
		void ApplyStressTestCounts(uint32_t meleeEnemyCount, uint32_t midRangeEnemyCount);
		void DrawDebugInstances() const;

		EnemyUpdateLodSettings& GetUpdateLodSettings() { return updateLodSettings_; }
		const EnemyScalabilityMetrics& GetMetrics() const { return metrics_; }
		const std::vector<ScalableEnemyData>& GetEnemies() const { return enemies_; }
		const std::vector<EnemyInstanceData>& GetMeleeEnemyInstanceData() const { return meleeEnemyInstanceData_; }
		const std::vector<EnemyInstanceData>& GetMidRangeEnemyInstanceData() const { return midRangeEnemyInstanceData_; }

		bool IsSimpleCollisionEnabled() const { return useSimpleCollision_; }
		void SetSimpleCollisionEnabled(bool enabled) { useSimpleCollision_ = enabled; }
		bool IsEnemyDebugDrawEnabled() const { return enemyDebugDraw_; }
		void SetEnemyDebugDrawEnabled(bool enabled) { enemyDebugDraw_ = enabled; }

	private:
		void SpawnEnemy(uint32_t spawnIndex, ScalableEnemyType enemyType);
		uint32_t GetUpdateInterval(const ScalableEnemyData& enemy) const;
		void UpdateEnemy(ScalableEnemyData& enemy, float deltaTime, const Vector3& playerPosition);
		void UpdateSimpleCollisions(const Vector3& playerPosition);
		void RebuildInstanceData();

		std::vector<ScalableEnemyData> enemies_{};
		std::vector<EnemyInstanceData> meleeEnemyInstanceData_{};
		std::vector<EnemyInstanceData> midRangeEnemyInstanceData_{};
		EnemyUpdateLodSettings updateLodSettings_{};
		EnemyScalabilityMetrics metrics_{};
		uint64_t frameCount_ = 0;
		bool useSimpleCollision_ = false;
		bool enemyDebugDraw_ = false;
	};
}
