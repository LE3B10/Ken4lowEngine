#include "EnemyShootState.h"
#include "Enemy.h"

using namespace Ken4lowEngine;

void EnemyShootState::Enter(Enemy& enemy)
{
	stayTimer_ = 0.0f;
	reevalTimer_ = 0.0f;
	enemy.StopMove();
	enemy.PlayShootAnimation();
}

void EnemyShootState::Update(Enemy& enemy, float deltaTime)
{
	if (!enemy.HasTarget())
	{
		enemy.ChangeStateToIdle();
		return;
	}

	stayTimer_ += deltaTime;
	reevalTimer_ -= deltaTime;

	const Vector3 targetPos = enemy.GetTargetPosition();
	const float distToTarget = enemy.GetDistanceToTarget();
	const bool canSee = enemy.CanSeeTargetPublic(targetPos, distToTarget);

	if (canSee)
	{
		enemy.RememberLastSeenTarget(targetPos);
	}
	else if (enemy.HasLostTarget())
	{
		enemy.ChangeStateToSearch();
		return;
	}

	if (reevalTimer_ <= 0.0f)
	{
		reevalTimer_ = enemy.GetShootRepositionEvalSec();

		const bool canShootNow = enemy.CanShootTargetPublic(targetPos);
		if (distToTarget > enemy.GetAttackRange() || !canShootNow)
		{
			enemy.ChangeStateToCombatMove();
			return;
		}

		if (stayTimer_ > enemy.GetShootMaxStaySec())
		{
			enemy.ChangeStateToCombatMove();
			return;
		}
	}

	// 射撃中も微移動して棒立ち感を減らす
	enemy.UpdateStrafeDecision(deltaTime);
	enemy.MoveTacticalAround(targetPos, enemy.GetCurrentStrafeSign(), 0.0f, enemy.GetShootMicroStrafeSpeed());
	enemy.FaceTo(targetPos);
	enemy.PlayShootAnimation();
	enemy.FireAt(targetPos);
}

void EnemyShootState::Exit(Enemy& enemy)
{
	(void)enemy;
}
