#include "EnemyArchetypeBehaviorFactory.h"

#include "EnemyBehavior_RifleGrunt.h"
#include "EnemyBehavior_SMGFlanker.h"
#include "EnemyBehavior_Sniper.h"
#include "EnemyBehavior_HeavyRifleman.h"
#include "EnemyBehavior_BurstTrooper.h"
#include "EnemyBehavior_ShotgunRusher.h"
#include "EnemyBehavior_Scout.h"
#include "EnemyBehavior_Marksman.h"
#include "EnemyBehavior_Suppressor.h"
#include "EnemyBehavior_EliteFlanker.h"
#include "EnemyBehavior_HeavySniper.h"
#include "EnemyBehavior_Default.h"

std::unique_ptr<EnemyArchetypeBehavior> EnemyArchetypeBehaviorFactory::Create(EnemyArchetype archetype)
{
	switch (archetype)
	{
	case EnemyArchetype::RifleGrunt:
		return std::make_unique<EnemyBehavior_RifleGrunt>();

	case EnemyArchetype::SMGFlanker:
		return std::make_unique<EnemyBehavior_SMGFlanker>();

	case EnemyArchetype::Sniper:
		return std::make_unique<EnemyBehavior_Sniper>();

	case EnemyArchetype::HeavyRifleman:
		return std::make_unique<EnemyBehavior_HeavyRifleman>();

	case EnemyArchetype::BurstTrooper:
		return std::make_unique<EnemyBehavior_BurstTrooper>();

	case EnemyArchetype::ShotgunRusher:
		return std::make_unique<EnemyBehavior_ShotgunRusher>();

	case EnemyArchetype::Scout:
		return std::make_unique<EnemyBehavior_Scout>();

	case EnemyArchetype::Marksman:
		return std::make_unique<EnemyBehavior_Marksman>();

	case EnemyArchetype::Suppressor:
		return std::make_unique<EnemyBehavior_Suppressor>();

	case EnemyArchetype::EliteFlanker:
		return std::make_unique<EnemyBehavior_EliteFlanker>();

	case EnemyArchetype::HeavySniper:
		return std::make_unique<EnemyBehavior_HeavySniper>();

	default:
		return std::make_unique<EnemyBehavior_Default>();
	}
}