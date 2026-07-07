#define NOMINMAX
#include "EnemyScalabilitySystem.h"

#include "MathUtil.h"
#include "Wireframe.h"
#include "LogString.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>

namespace Ken4lowEngine
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		float ToMilliseconds(const Clock::time_point& begin, const Clock::time_point& end)
		{
			return std::chrono::duration<float, std::milli>(end - begin).count();
		}

		float LengthXZ(const Vector3& value)
		{
			// XZ長さの計算本体だけを共通Vector3へ寄せ、LOD判定の値そのものは変えない。
			return Vector3::LengthXZ(value);
		}
	}

	void EnemyScalabilitySystem::ApplyStressTestCounts(uint32_t meleeEnemyCount, uint32_t midRangeEnemyCount, const Vector3& centerPosition)
	{
		enemies_.clear();
		meleeEnemyInstanceData_.clear();
		midRangeEnemyInstanceData_.clear();
		metrics_ = {};
		enemies_.reserve(static_cast<size_t>(meleeEnemyCount) + midRangeEnemyCount);
		meleeEnemyInstanceData_.reserve(meleeEnemyCount);
		midRangeEnemyInstanceData_.reserve(midRangeEnemyCount);
		for (uint32_t index = 0; index < meleeEnemyCount; ++index)
		{
			SpawnEnemy(index, ScalableEnemyType::Melee, centerPosition);
		}
		for (uint32_t index = 0; index < midRangeEnemyCount; ++index)
		{
			SpawnEnemy(meleeEnemyCount + index, ScalableEnemyType::MidRange, centerPosition);
		}
		RebuildInstanceData();
		Log(std::format("[EnemyStressTest] 適用: 近接={}, 中距離={}, 合計={}\n", meleeEnemyCount, midRangeEnemyCount, meleeEnemyCount + midRangeEnemyCount));
	}

	void EnemyScalabilitySystem::Clear()
	{
		enemies_.clear();
		meleeEnemyInstanceData_.clear();
		midRangeEnemyInstanceData_.clear();
		metrics_ = {};
		Log("[EnemyStressTest] クリアしました\n");
	}

	void EnemyScalabilitySystem::SpawnEnemy(uint32_t spawnIndex, ScalableEnemyType enemyType, const Vector3& centerPosition)
	{
		constexpr uint32_t gridColumns = 20;
		constexpr float gridSpacing = 3.0f;
		const uint32_t row = spawnIndex / gridColumns;
		const uint32_t column = spawnIndex % gridColumns;
		const float x = (static_cast<float>(column) - (static_cast<float>(gridColumns) - 1.0f) * 0.5f) * gridSpacing;
		const float z = (static_cast<float>(row) + 1.0f) * gridSpacing;

		ScalableEnemyData enemy{};
		// StressTest の増加を目視確認しやすくするため、敵をターゲット周辺の地表グリッドへ配置する。
		enemy.position = { centerPosition.x + x, centerPosition.y, centerPosition.z + z };
		enemy.rotation = { 0.0f, 3.14159265f, 0.0f };
		enemy.scale = enemyType == ScalableEnemyType::Melee ? Vector3{ 0.75f, 0.75f, 0.75f } : Vector3{ 0.9f, 0.9f, 0.9f };
		enemy.enemyType = enemyType;
		enemy.state = ScalableEnemyState::Moving;
		enemy.hp = enemyType == ScalableEnemyType::Melee ? 100.0f : 80.0f;
		enemy.updateGroupId = spawnIndex;
		// プレイヤー基準点とのXZ距離だけを共通MathUtilへ寄せ、LOD用の判定値は変えない。
		enemy.distanceToPlayer = MathUtil::DistanceXZ(centerPosition, enemy.position);
		enemies_.push_back(enemy);
	}

	void EnemyScalabilitySystem::Update(float deltaTime, const Vector3& playerPosition)
	{
		const Clock::time_point updateBegin = Clock::now();
		++frameCount_;
		metrics_.updatedEnemyCountThisFrame = 0;
		metrics_.nearUpdatedEnemyCount = 0;
		metrics_.midUpdatedEnemyCount = 0;
		metrics_.farUpdatedEnemyCount = 0;
		metrics_.skippedEnemyCountThisFrame = 0;
		metrics_.intervalSkippedEnemyCount = 0;
		metrics_.outOfRangeSkippedEnemyCount = 0;
		metrics_.inactiveSkippedEnemyCount = 0;

		for (ScalableEnemyData& enemy : enemies_)
		{
			if (!enemy.isActive)
			{
				++metrics_.skippedEnemyCountThisFrame;
				++metrics_.inactiveSkippedEnemyCount;
				continue;
			}

			// プレイヤーとのXZ距離だけを共通MathUtilへ寄せ、更新頻度の判定条件は維持する。
			enemy.distanceToPlayer = MathUtil::DistanceXZ(playerPosition, enemy.position);
			const uint32_t updateInterval = GetUpdateInterval(enemy);
			// 大量敵の負荷を抑えるため、距離と updateGroupId に応じて更新頻度と更新フレームを分散する。
			if (updateInterval == 0)
			{
				enemy.velocity = {};
				enemy.state = ScalableEnemyState::Dormant;
				++metrics_.skippedEnemyCountThisFrame;
				++metrics_.outOfRangeSkippedEnemyCount;
				continue;
			}
			if ((enemy.updateGroupId + frameCount_) % updateInterval != 0)
			{
				++metrics_.skippedEnemyCountThisFrame;
				++metrics_.intervalSkippedEnemyCount;
				continue;
			}

			UpdateEnemy(enemy, deltaTime * static_cast<float>(updateInterval), playerPosition);
			++metrics_.updatedEnemyCountThisFrame;
			if (enemy.distanceToPlayer <= updateLodSettings_.nearUpdateDistance) ++metrics_.nearUpdatedEnemyCount;
			else if (enemy.distanceToPlayer <= updateLodSettings_.midUpdateDistance) ++metrics_.midUpdatedEnemyCount;
			else ++metrics_.farUpdatedEnemyCount;
		}
		metrics_.enemyUpdateTimeMs = ToMilliseconds(updateBegin, Clock::now());

		const Clock::time_point collisionBegin = Clock::now();
		UpdateSimpleCollisions(playerPosition);
		metrics_.enemyCollisionTimeMs = ToMilliseconds(collisionBegin, Clock::now());

		const Clock::time_point drawSubmitBegin = Clock::now();
		RebuildInstanceData();
		metrics_.enemyDrawSubmitTimeMs = ToMilliseconds(drawSubmitBegin, Clock::now());
	}

	uint32_t EnemyScalabilitySystem::GetUpdateInterval(const ScalableEnemyData& enemy) const
	{
		if (!updateLodSettings_.useEnemyUpdateLod)
		{
			return 1;
		}
		if (enemy.distanceToPlayer <= updateLodSettings_.nearUpdateDistance)
		{
			return std::max(updateLodSettings_.nearUpdateInterval, 1u);
		}
		if (enemy.distanceToPlayer <= updateLodSettings_.midUpdateDistance)
		{
			return std::max(updateLodSettings_.midUpdateInterval, 1u);
		}
		if (enemy.distanceToPlayer <= updateLodSettings_.farUpdateDistance)
		{
			return std::max(updateLodSettings_.farUpdateInterval, 1u);
		}
		return 0; // 遠すぎる敵は停止し、描画バッチからも除外する。
	}

	void EnemyScalabilitySystem::UpdateEnemy(ScalableEnemyData& enemy, float deltaTime, const Vector3& playerPosition)
	{
		Vector3 direction = playerPosition - enemy.position;
		direction.y = 0.0f;
		const float distance = LengthXZ(direction);
		if (distance <= 0.001f)
		{
			enemy.velocity = {};
			enemy.state = ScalableEnemyState::Idle;
			return;
		}

		const float inverseDistance = 1.0f / distance;
		direction.x *= inverseDistance;
		direction.z *= inverseDistance;
		const float speed = enemy.enemyType == ScalableEnemyType::Melee ? 1.5f : 0.8f;
		enemy.velocity = direction * speed;
		enemy.position += enemy.velocity * deltaTime;
		enemy.rotation.y = std::atan2(direction.x, direction.z);
		enemy.state = ScalableEnemyState::Moving;
	}

	void EnemyScalabilitySystem::UpdateSimpleCollisions(const Vector3& playerPosition)
	{
		metrics_.collisionCheckCount = 0;
		if (!useSimpleCollision_)
		{
			return;
		}

		// 判定をこの関数へ隔離し、次段階で Spatial Hash / Uniform Grid の候補抽出へ置き換えやすくする。
		constexpr float playerCollisionRadius = 1.0f;
		constexpr float enemyCollisionRadius = 0.6f;
		const float collisionDistance = playerCollisionRadius + enemyCollisionRadius;
		const float collisionDistanceSquared = collisionDistance * collisionDistance;
		for (ScalableEnemyData& enemy : enemies_)
		{
			if (!enemy.isActive || enemy.distanceToPlayer > updateLodSettings_.nearUpdateDistance)
			{
				continue;
			}
			++metrics_.collisionCheckCount;
			Vector3 offset = enemy.position - playerPosition;
			offset.y = 0.0f;
			const float distanceSquared = offset.x * offset.x + offset.z * offset.z;
			if (distanceSquared > 0.0001f && distanceSquared < collisionDistanceSquared)
			{
				const float scale = collisionDistance / std::sqrt(distanceSquared);
				enemy.position.x = playerPosition.x + offset.x * scale;
				enemy.position.z = playerPosition.z + offset.z * scale;
			}
		}
	}

	void EnemyScalabilitySystem::RebuildInstanceData()
	{
		meleeEnemyInstanceData_.clear();
		midRangeEnemyInstanceData_.clear();
		metrics_.meleeEnemyCount = 0;
		metrics_.midRangeEnemyCount = 0;
		metrics_.visibleEnemyCount = 0;
		metrics_.culledEnemyCount = 0;
		metrics_.nearEnemyCount = 0;
		metrics_.midEnemyCount = 0;
		metrics_.farEnemyCount = 0;
		metrics_.outOfRangeEnemyCount = 0;

		// Update LODの効果を確認しやすくするため、距離帯ごとの敵数と更新数を表示する。
		for (uint32_t index = 0; index < enemies_.size(); ++index)
		{
			const ScalableEnemyData& enemy = enemies_[index];
			if (!enemy.isActive)
			{
				++metrics_.culledEnemyCount;
				continue;
			}
			if (enemy.enemyType == ScalableEnemyType::Melee) ++metrics_.meleeEnemyCount;
			else ++metrics_.midRangeEnemyCount;

			if (enemy.distanceToPlayer <= updateLodSettings_.nearUpdateDistance) ++metrics_.nearEnemyCount;
			else if (enemy.distanceToPlayer <= updateLodSettings_.midUpdateDistance) ++metrics_.midEnemyCount;
			else if (enemy.distanceToPlayer <= updateLodSettings_.farUpdateDistance) ++metrics_.farEnemyCount;
			else
			{
				++metrics_.outOfRangeEnemyCount;
				++metrics_.culledEnemyCount;
				continue;
			}

			++metrics_.visibleEnemyCount;
			EnemyInstanceData instance{};
			instance.worldMatrix = Matrix4x4::MakeAffineMatrix(enemy.scale, enemy.rotation, enemy.position);
			instance.enemyIndex = index;
			auto& instances = enemy.enemyType == ScalableEnemyType::Melee ? meleeEnemyInstanceData_ : midRangeEnemyInstanceData_;
			instances.push_back(instance);
		}
		metrics_.totalEnemyCount = static_cast<uint32_t>(enemies_.size());
		metrics_.drawEnemyCount = enemyDebugDraw_ ? metrics_.visibleEnemyCount : 0;
	}

	void EnemyScalabilitySystem::DrawDebugInstances() const
	{
		if (!enemyDebugDraw_)
		{
			return;
		}
		for (const ScalableEnemyData& enemy : enemies_)
		{
			if (!enemy.isActive || enemy.distanceToPlayer > updateLodSettings_.farUpdateDistance)
			{
				continue;
			}
			const Vector4 color = enemy.enemyType == ScalableEnemyType::Melee
				? Vector4{ 1.0f, 0.25f, 0.15f, 0.8f }
				: Vector4{ 0.25f, 0.55f, 1.0f, 0.8f };
			if (enemy.enemyType == ScalableEnemyType::Melee)
			{
				Wireframe::GetInstance()->DrawSphere(enemy.position, 0.6f, color);
			}
			else
			{
				const Vector3 halfExtent{ 0.65f, 0.65f, 0.65f };
				Wireframe::GetInstance()->DrawAABB({ enemy.position - halfExtent, enemy.position + halfExtent }, color);
			}
			// 一部の敵には番号代わりの目印を付け、グリッド内で個体を追跡しやすくする。
			if (enemy.updateGroupId % 10 == 0)
			{
				Wireframe::GetInstance()->DrawLine(enemy.position, enemy.position + Vector3{ 0.0f, 2.0f, 0.0f }, { 1.0f, 1.0f, 0.2f, 1.0f });
			}

		}
	}
}
