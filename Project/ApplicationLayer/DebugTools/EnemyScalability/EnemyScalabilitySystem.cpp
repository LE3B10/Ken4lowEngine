#define NOMINMAX
#include "EnemyScalabilitySystem.h"

#include "Wireframe.h"

#include <algorithm>
#include <chrono>
#include <cmath>

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
			return std::sqrt(value.x * value.x + value.z * value.z);
		}
	}

	void EnemyScalabilitySystem::ApplyStressTestCounts(uint32_t meleeEnemyCount, uint32_t midRangeEnemyCount)
	{
		Clear();
		enemies_.reserve(static_cast<size_t>(meleeEnemyCount) + midRangeEnemyCount);
		meleeEnemyInstanceData_.reserve(meleeEnemyCount);
		midRangeEnemyInstanceData_.reserve(midRangeEnemyCount);
		for (uint32_t index = 0; index < meleeEnemyCount; ++index)
		{
			SpawnEnemy(index, ScalableEnemyType::Melee);
		}
		for (uint32_t index = 0; index < midRangeEnemyCount; ++index)
		{
			SpawnEnemy(meleeEnemyCount + index, ScalableEnemyType::MidRange);
		}
		RebuildInstanceData();
	}

	void EnemyScalabilitySystem::Clear()
	{
		enemies_.clear();
		meleeEnemyInstanceData_.clear();
		midRangeEnemyInstanceData_.clear();
		metrics_ = {};
	}

	void EnemyScalabilitySystem::SpawnEnemy(uint32_t spawnIndex, ScalableEnemyType enemyType)
	{
		constexpr float goldenAngle = 2.39996323f;
		const float angle = goldenAngle * static_cast<float>(spawnIndex);
		const float radius = 8.0f + std::sqrt(static_cast<float>(spawnIndex)) * 2.25f;

		ScalableEnemyData enemy{};
		enemy.position = { std::cos(angle) * radius, 1.0f, 24.0f + std::sin(angle) * radius };
		enemy.rotation = { 0.0f, angle + 3.14159265f, 0.0f };
		enemy.scale = enemyType == ScalableEnemyType::Melee ? Vector3{ 0.75f, 0.75f, 0.75f } : Vector3{ 0.9f, 0.9f, 0.9f };
		enemy.enemyType = enemyType;
		enemy.state = ScalableEnemyState::Moving;
		enemy.hp = enemyType == ScalableEnemyType::Melee ? 100.0f : 80.0f;
		enemy.updateGroupId = spawnIndex;
		enemies_.push_back(enemy);
	}

	void EnemyScalabilitySystem::Update(float deltaTime, const Vector3& playerPosition)
	{
		const Clock::time_point updateBegin = Clock::now();
		++frameCount_;
		metrics_.updatedEnemyCountThisFrame = 0;
		metrics_.skippedEnemyCountThisFrame = 0;

		for (ScalableEnemyData& enemy : enemies_)
		{
			if (!enemy.isActive)
			{
				++metrics_.skippedEnemyCountThisFrame;
				continue;
			}

			const Vector3 playerOffset = playerPosition - enemy.position;
			enemy.distanceToPlayer = LengthXZ(playerOffset);
			const uint32_t updateInterval = GetUpdateInterval(enemy);
			// 大量敵の負荷を抑えるため、距離と updateGroupId に応じて更新頻度と更新フレームを分散する。
			if (updateInterval == 0)
			{
				enemy.velocity = {};
				enemy.state = ScalableEnemyState::Dormant;
				++metrics_.skippedEnemyCountThisFrame;
				continue;
			}
			if ((enemy.updateGroupId + frameCount_) % updateInterval != 0)
			{
				++metrics_.skippedEnemyCountThisFrame;
				continue;
			}

			UpdateEnemy(enemy, deltaTime * static_cast<float>(updateInterval), playerPosition);
			++metrics_.updatedEnemyCountThisFrame;
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

		for (uint32_t index = 0; index < enemies_.size(); ++index)
		{
			const ScalableEnemyData& enemy = enemies_[index];
			if (!enemy.isActive)
			{
				continue;
			}
			if (enemy.enemyType == ScalableEnemyType::Melee)
			{
				++metrics_.meleeEnemyCount;
			}
			else
			{
				++metrics_.midRangeEnemyCount;
			}
			if (enemy.distanceToPlayer > updateLodSettings_.farUpdateDistance)
			{
				continue;
			}

			EnemyInstanceData instance{};
			instance.worldMatrix = Matrix4x4::MakeAffineMatrix(enemy.scale, enemy.rotation, enemy.position);
			instance.enemyIndex = index;
			auto& instances = enemy.enemyType == ScalableEnemyType::Melee ? meleeEnemyInstanceData_ : midRangeEnemyInstanceData_;
			instances.push_back(instance);
		}
		metrics_.totalEnemyCount = static_cast<uint32_t>(enemies_.size());
		metrics_.drawEnemyCount = static_cast<uint32_t>(meleeEnemyInstanceData_.size() + midRangeEnemyInstanceData_.size());
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
			Wireframe::GetInstance()->DrawSphere(enemy.position, 0.6f, color);
		}
	}
}
