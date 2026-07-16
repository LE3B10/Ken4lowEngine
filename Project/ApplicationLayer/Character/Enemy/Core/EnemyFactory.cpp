#include "EnemyFactory.h"

#include "../Actor/EnemyActor.h"

std::unique_ptr<EnemyBase> EnemyFactory::Create(EnemyType enemyType)
{
	switch (enemyType)
	{
	case EnemyType::Melee:
		return std::make_unique<Ken4lowEngine::EnemyActor>(EnemyType::Melee);
	case EnemyType::MidRange:
		return std::make_unique<Ken4lowEngine::EnemyActor>(EnemyType::MidRange); // 中距離敵も距離AI・爆弾・自爆Componentを持つ同じActor境界へ統一する。
	default:
		return std::make_unique<Ken4lowEngine::EnemyActor>(EnemyType::Melee);
	}
}
