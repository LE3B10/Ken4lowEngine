#define NOMINMAX
#include "EnemySpawnCrystal.h"

#include "CharacterWorld.h"
#include "EnemyBase.h"
#include "Bullet.h"
#include "CollisionTypeIdDef.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

using namespace Ken4lowEngine;

namespace
{
	float RandomRange(float minValue, float maxValue)
	{
		const float t = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
		return minValue + (maxValue - minValue) * t;
	}
}

void EnemySpawnCrystal::Initialize(const CrystalSpawnPoint& spawnPoint)
{
	position_ = spawnPoint.position;
	rotation_ = spawnPoint.rotation;
	scale_ = spawnPoint.scale;
	isAlive = true;
	hp = std::max(1, spawnPoint.hp);
	spawnEnemyType = spawnPoint.spawnEnemyType;
	spawnRadius = std::max(0.0f, spawnPoint.spawnRadius);
	maxAliveEnemies = std::max(0, spawnPoint.maxAliveEnemies);
	totalSpawnedCount = 0;
	aliveSpawnedEnemyCount = 0;
	enableInfiniteSpawn = spawnPoint.enableInfiniteSpawn;
	spawnBossTrigger_ = spawnPoint.spawnBossTrigger;
	spawnedEnemies_.clear();
	hitCount_ = 0;

	SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kCrystal));
	SetOwner(this);
	SetCenterPosition(position_);
	SetOBBHalfSize(scale_);
	SetOrientation(rotation_);

	debugCube_ = std::make_unique<Object3D>();
	debugCube_->Initialize("Test/cube.gltf");
	debugCube_->SetTranslate(position_);
	debugCube_->SetRotate(rotation_);
	debugCube_->SetScale(scale_);
	debugCube_->SetColor({ 0.25f, 0.85f, 1.0f, 1.0f });
	debugCube_->Update();
}

void EnemySpawnCrystal::Update(const CharacterWorld& characters)
{
	RemoveInactiveSpawnedEnemies(characters);

	if (debugCube_)
	{
		// クリスタルはカメラ基準ではなく、ステージ上の固定ワールド座標に配置する。
		debugCube_->SetTranslate(position_);
		debugCube_->SetRotate(rotation_);
		debugCube_->SetScale(scale_);
		debugCube_->Update();
	}
}

void EnemySpawnCrystal::Draw() const
{
	if (isAlive && debugCube_)
	{
		debugCube_->Draw();
	}
}

void EnemySpawnCrystal::ApplyDamage(int damage)
{
	if (!isAlive || damage <= 0)
	{
		return;
	}

	// プレイヤー攻撃でクリスタルを破壊できるよう、被弾回数とHPを一か所で更新する。
	++hitCount_;
	hp = std::max(0, hp - damage);
	if (hp <= 0)
	{
		isAlive = false;
		// 破壊後のColliderが近接攻撃や弾を遮らないよう、判定だけを場外へ退避する。
		SetCenterPosition({ 1.0e9f, 1.0e9f, 1.0e9f });
	}
}


void EnemySpawnCrystal::OnCollisionEnter(K4E::Collider* other)
{
	if (!isAlive || !other || other->GetTypeID() != static_cast<uint32_t>(CollisionTypeIdDef::kBullet))
	{
		return;
	}

	if (auto* bullet = other->GetOwner<Bullet>())
	{
		ApplyDamage(bullet->GetDamage());
	}
}

bool EnemySpawnCrystal::CanSpawnEnemy() const
{
	return isAlive && enableInfiniteSpawn && aliveSpawnedEnemyCount < maxAliveEnemies;
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
	if (!CanSpawnEnemy())
	{
		return;
	}

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
