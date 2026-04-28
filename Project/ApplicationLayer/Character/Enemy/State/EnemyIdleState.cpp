#include "EnemyIdleState.h"
#include "Enemy.h"

using namespace Ken4lowEngine;

void EnemyIdleState::Enter(Enemy& enemy)
{
	enemy.UpdateWander(0.0f);
}

void EnemyIdleState::Update(Enemy& enemy, float deltaTime)
{
	enemy.UpdateWander(deltaTime);

	if (!enemy.HasTarget()) return;

	const Vector3 targetPos = enemy.GetTargetPosition();
	const float distToTarget = enemy.GetDistanceToTarget();

	if (enemy.CanSeeTargetPublic(targetPos, distToTarget))
	{
		enemy.RememberLastSeenTarget(targetPos);
		enemy.ChangeStateToCombatMove();
		enemy.FaceTo(targetPos);
		return;
	}
}

void EnemyIdleState::Exit(Enemy& enemy)
{
	(void)enemy;
}
