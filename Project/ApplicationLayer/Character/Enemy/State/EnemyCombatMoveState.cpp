#include "EnemyCombatMoveState.h"
#include "Enemy.h"

using namespace Ken4lowEngine;

void EnemyCombatMoveState::Enter(Enemy& enemy)
{
	enemy.PlayMoveAnimation(enemy.GetApproachSpeed());
}

void EnemyCombatMoveState::Update(Enemy& enemy, float deltaTime)
{
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

	enemy.UpdateStrafeDecision(deltaTime);

	const bool canShoot = enemy.CanShootTargetPublic(targetPos);
	const float tooClose = enemy.GetIdealRangeMin();
	const float tooFar = enemy.GetIdealRangeMax();

	if (distToTarget < tooClose)
	{
		enemy.MoveAwayFrom(targetPos, enemy.GetRetreatSpeed());
		enemy.PlayMoveAnimation(enemy.GetRetreatSpeed());
	}
	else if (distToTarget > tooFar)
	{
		enemy.MoveTowards(targetPos, enemy.GetApproachSpeed());
		enemy.PlayMoveAnimation(enemy.GetApproachSpeed());
	}
	else
	{
		enemy.MoveStrafeAround(targetPos, enemy.GetCurrentStrafeSign(), enemy.GetStrafeSpeed());
		enemy.PlayMoveAnimation(enemy.GetStrafeSpeed());
	}

	if (!canShoot && distToTarget <= enemy.GetAttackRange())
	{
		enemy.MoveStrafeAround(targetPos, enemy.GetCurrentStrafeSign(), enemy.GetStrafeSpeed());
	}

	if (distToTarget <= enemy.GetAttackRange() && canShoot)
	{
		enemy.ChangeStateToShoot();
		return;
	}
}

void EnemyCombatMoveState::Exit(Enemy& enemy)
{
	(void)enemy;
}
