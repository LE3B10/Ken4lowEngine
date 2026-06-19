#include "EnemyFactory.h"

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
	default:
		// 不正なenum値でも旧Enemyへ戻さず、通常ゲームの既定であるMeleeEnemyを生成する。
		return std::make_unique<MeleeEnemy>();
	}
}
