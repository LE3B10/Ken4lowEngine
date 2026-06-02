#define NOMINMAX
#include "EnemySpawnCrystal.h"

#include "CharacterWorld.h"
#include "EnemyBase.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

using namespace Ken4lowEngine;

namespace
{
	constexpr float kMinimumSpawnInterval = 0.05f;

	float RandomRange(float minValue, float maxValue)
	{
		const float t = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
		return minValue + (maxValue - minValue) * t;
	}
}

void EnemySpawnCrystal::Initialize(const CrystalSpawnPoint& spawnPoint)
{
	position_ = spawnPoint.position;
	isAlive = true;
	hp = std::max(1, spawnPoint.hp);
	spawnEnemyType = spawnPoint.spawnEnemyType;
	spawnInterval = std::max(kMinimumSpawnInterval, spawnPoint.spawnInterval);
	spawnTimer = 0.0f;
	spawnRadius = std::max(0.0f, spawnPoint.spawnRadius);
	maxAliveEnemies = std::max(0, spawnPoint.maxAliveEnemies);
	totalSpawnedCount = 0;
	aliveSpawnedEnemyCount = 0;
	enableInfiniteSpawn = spawnPoint.enableInfiniteSpawn;
	spawnBossTrigger_ = spawnPoint.spawnBossTrigger;
	spawnedEnemies_.clear();

	debugCube_ = std::make_unique<Object3D>();
	debugCube_->Initialize("Test/cube.gltf");
	debugCube_->SetTranslate(spawnPoint.position);
	debugCube_->SetRotate(spawnPoint.rotation);
	debugCube_->SetScale(spawnPoint.scale);
	debugCube_->SetColor({ 0.25f, 0.85f, 1.0f, 1.0f });
	debugCube_->Update();
}

void EnemySpawnCrystal::Update(CharacterWorld& characters, float deltaTime)
{
	RemoveInactiveSpawnedEnemies(characters);
	if (!isAlive || !enableInfiniteSpawn || maxAliveEnemies <= 0)
	{
		return;
	}

	// クリスタルが破壊されるまで一定間隔で敵を出し続ける。
	spawnTimer += std::max(0.0f, deltaTime);
	if (spawnTimer < spawnInterval || aliveSpawnedEnemyCount >= maxAliveEnemies)
	{
		return;
	}

	spawnTimer = 0.0f;
	SpawnEnemy(characters);
}

void EnemySpawnCrystal::Draw() const
{
	if (isAlive && debugCube_)
	{
		debugCube_->Draw();
	}
}

void EnemySpawnCrystal::TakeDamage(int damage)
{
	if (!isAlive || damage <= 0)
	{
		return;
	}

	hp = std::max(0, hp - damage);
	if (hp <= 0)
	{
		isAlive = false;
		spawnTimer = 0.0f;
	}
}

void EnemySpawnCrystal::SetSpawnInterval(float interval)
{
	spawnInterval = std::max(kMinimumSpawnInterval, interval);
}

void EnemySpawnCrystal::SetMaxAliveEnemies(int count)
{
	maxAliveEnemies = std::max(0, count);
}

void EnemySpawnCrystal::RemoveInactiveSpawnedEnemies(const CharacterWorld& characters)
{
	const std::vector<EnemyBase*> livingEnemies = characters.GetEnemyRawList();
	spawnedEnemies_.erase(
		std::remove_if(spawnedEnemies_.begin(), spawnedEnemies_.end(),
			[&livingEnemies](const EnemyBase* spawnedEnemy)
			{
				const auto it = std::find(livingEnemies.begin(), livingEnemies.end(), spawnedEnemy);
				return it == livingEnemies.end() || (*it)->IsDead();
			}),
		spawnedEnemies_.end());
	aliveSpawnedEnemyCount = static_cast<int>(spawnedEnemies_.size());
}

void EnemySpawnCrystal::SpawnEnemy(CharacterWorld& characters)
{
	const float angle = RandomRange(0.0f, 6.28318530718f);
	const float distance = RandomRange(0.0f, spawnRadius);
	const Vector3 spawnPosition{
		position_.x + std::cos(angle) * distance,
		position_.y,
		position_.z + std::sin(angle) * distance
	};

	// CharacterWorld 内部で EnemyFactory を経由し、設定された雑魚敵派生を生成する。
	EnemyBase& enemy = characters.SpawnEnemyAt(spawnPosition, spawnEnemyType);
	spawnedEnemies_.push_back(&enemy);
	aliveSpawnedEnemyCount = static_cast<int>(spawnedEnemies_.size());
	++totalSpawnedCount;
}
