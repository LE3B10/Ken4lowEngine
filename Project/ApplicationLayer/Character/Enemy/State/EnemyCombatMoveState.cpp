#define NOMINMAX
#include "EnemyCombatMoveState.h"
#include "Enemy.h"

#include <algorithm>

using namespace Ken4lowEngine;

namespace
{
	float Clamp(float v, float lo, float hi)
	{
		if (v < lo) return lo;
		if (v > hi) return hi;
		return v;
	}
}

void EnemyCombatMoveState::Enter(Enemy& enemy)
{
	losRepositionTimer_ = 0.0f;
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
	losRepositionTimer_ -= deltaTime;

	const float idealMin = enemy.GetIdealRangeMin();
	const float idealMax = enemy.GetIdealRangeMax();
	const float mid = (idealMin + idealMax) * 0.5f;
	const float half = std::max(0.2f, (idealMax - idealMin) * 0.5f);

	float radialBias = Clamp((distToTarget - mid) / half, -1.0f, 1.0f);
	// radialBias > 0: 接近 / <0: 離脱
	float speed = enemy.GetStrafeSpeed();
	bool shouldPathChase = false;

	if (distToTarget < idealMin)
	{
		speed = enemy.GetRetreatSpeed();
	}
	else if (distToTarget > idealMax)
	{
		speed = enemy.GetApproachSpeed();
		shouldPathChase = true;
	}

	bool canShoot = enemy.CanShootTargetPublic(targetPos);
	if (!canShoot)
	{
		if (losRepositionTimer_ <= 0.0f)
		{
			const float betterSign = enemy.ChooseBetterStrafeSign(targetPos, enemy.GetLosProbeDistance());
			if (betterSign != enemy.GetCurrentStrafeSign())
			{
				enemy.ForceStrafeSign(betterSign);
			}
			losRepositionTimer_ = enemy.GetLosRepositionEvalSec();
		}

		radialBias = std::max(radialBias, -0.1f);
		speed = std::max(speed, enemy.GetStrafeSpeed());
		shouldPathChase = true;
	}

	if (shouldPathChase)
	{
		enemy.MoveTowardsPath(targetPos, speed, deltaTime);
	}
	else
	{
		enemy.MoveTacticalAround(targetPos, enemy.GetCurrentStrafeSign(), radialBias, speed);
	}
	enemy.PlayMoveAnimation(speed);

	canShoot = enemy.CanShootTargetPublic(targetPos);
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
