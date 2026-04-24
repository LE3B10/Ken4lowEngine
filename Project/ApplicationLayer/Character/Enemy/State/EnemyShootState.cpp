#include "EnemyShootState.h"
#include "Enemy.h"

using namespace Ken4lowEngine;

void EnemyShootState::Enter(Enemy& enemy)
{
	enemy.StopMove();
	enemy.PlayShootAnimation();
}

void EnemyShootState::Update(Enemy& enemy, float deltaTime)
{
	(void)deltaTime;

	if (!enemy.HasTarget())
	{
		enemy.ChangeStateToIdle();
		return;
	}

	const Vector3 targetPos = enemy.GetTargetPosition();
	const float distToTarget = enemy.GetDistanceToTarget();

	if (enemy.CanSeeTargetPublic(targetPos, distToTarget))
	{
		enemy.RememberLastSeenTarget(targetPos);
	}
	else if (enemy.HasLostTarget())
	{
		enemy.ChangeStateToSearch();
		return;
	}

	if (distToTarget > enemy.GetAttackRange() || !enemy.CanShootTargetPublic(targetPos))
	{
		enemy.ChangeStateToCombatMove();
		return;
	}

	enemy.StopMove();
	enemy.FaceTo(targetPos);
	enemy.PlayShootAnimation();
	enemy.FireAt(targetPos);
}

void EnemyShootState::Exit(Enemy& enemy)
{
	(void)enemy;
}
