#include "EnemyFactory.h"

#include "../Actor/EnemyActor.h"
#include "MidRangeEnemy.h"

std::unique_ptr<EnemyBase> EnemyFactory::Create(EnemyType enemyType)
{
	switch (enemyType)
	{
	case EnemyType::Melee:
		return std::make_unique<Ken4lowEngine::EnemyActor>(); // 本番の近接敵はAI・攻撃Componentを持つEnemyActorへ統一する。
	case EnemyType::MidRange:
		return std::make_unique<MidRangeEnemy>();
	default:
		return std::make_unique<Ken4lowEngine::EnemyActor>();
	}
}
