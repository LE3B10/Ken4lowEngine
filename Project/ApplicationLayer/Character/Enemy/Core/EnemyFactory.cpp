#include "EnemyFactory.h"

#include "Enemy.h"
#include "MeleeEnemy.h"
#include "MidRangeEnemy.h"

std::unique_ptr<EnemyBase> EnemyFactory::Create(EnemyType enemyType)
{
	switch (enemyType)
	{
	case EnemyType::Melee:
		return std::make_unique<MeleeEnemy>();
	case EnemyType::MidRange:
		return std::make_unique<MidRangeEnemy>();
	case EnemyType::Legacy:
	default:
		return std::make_unique<Enemy>();
	}
}
