#include "EnemyCombatMoveState.h"
#include "Enemy.h"

using namespace Ken4lowEngine;

void EnemyCombatMoveState::Enter(Enemy& enemy)
{
	(void)enemy;
}

void EnemyCombatMoveState::Update(Enemy& enemy, float deltaTime)
{
	(void)deltaTime;

	// ターゲットがいない場合は待機状態に遷移
	if (!enemy.HasTarget())
	{
		enemy.ChangeStateToIdle();
		return;
	}

	const Vector3 targetPos = enemy.GetTargetPosition();
	const float distToTarget = enemy.GetDistanceToTarget();

	if (!enemy.CanSeeTargetPublic(targetPos, distToTarget))
	{
		enemy.ChangeStateToSearch();
		return;
	}

	enemy.RememberLastSeenTarget(targetPos);
	enemy.MoveTowards(targetPos);

	if (distToTarget <= enemy.GetAttackRange() && enemy.CanShootTargetPublic(targetPos))
	{
		enemy.ChangeStateToShoot();
		return;
	}
}

void EnemyCombatMoveState::Exit(Enemy& enemy)
{
	(void)enemy;
}
