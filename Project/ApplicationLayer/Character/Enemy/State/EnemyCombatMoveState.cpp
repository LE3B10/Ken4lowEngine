#define NOMINMAX
#include "EnemyCombatMoveState.h"
#include "Enemy.h"

#include <algorithm>

using namespace Ken4lowEngine;

void EnemyCombatMoveState::Enter(Enemy& enemy)
{
	losRepositionTimer_ = 0.0f;
	retreatRepathTimer_ = 0.0f;
	coverStayTimer_ = 0.0f;
	hasRetreatTarget_ = false;
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
	retreatRepathTimer_ -= deltaTime;
	coverStayTimer_ -= deltaTime;

	bool canShoot = enemy.CanShootTargetPublic(targetPos);
	const auto retreatPlan = enemy.EvaluateRetreatPlan(distToTarget, canShoot);
	const bool dangerMode = retreatPlan.dangerMode;
	const bool inHitReaction = enemy.IsInHitReaction();

	float radialBias = retreatPlan.radialBias;
	float speed = retreatPlan.speed;
	bool shouldPathChase = retreatPlan.shouldPathChase;
	bool forceRetreat = retreatPlan.forceRetreat;

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

		radialBias = dangerMode ? std::min(radialBias, -0.4f) : std::max(radialBias, -0.1f);
		speed = std::max(speed, enemy.GetStrafeSpeed());
		shouldPathChase = true;
	}

	const bool shouldPrioritizeCover = forceRetreat || (dangerMode && coverStayTimer_ <= 0.0f);
	if (shouldPrioritizeCover)
	{
		if (!hasRetreatTarget_ || retreatRepathTimer_ <= 0.0f)
		{
			Ken4lowEngine::Vector3 cover{};
			hasRetreatTarget_ = enemy.TryFindCoverPosition(targetPos, true, cover);
			if (hasRetreatTarget_)
			{
				retreatTarget_ = cover;
			}

			retreatRepathTimer_ = dangerMode ? enemy.GetRetreatDecisionInterval() : enemy.GetCoverRepathInterval();
		}

		if (hasRetreatTarget_)
		{
			enemy.MoveTowardsPath(retreatTarget_, speed, deltaTime);
			coverStayTimer_ = enemy.GetCoverStayTime();
		}
		else
		{
			enemy.MoveAwayFrom(targetPos, speed);
		}
	}
	else if (shouldPathChase)
	{
		enemy.MoveTowardsPath(targetPos, speed, deltaTime);
	}
	else
	{
		enemy.MoveTacticalAround(targetPos, enemy.GetCurrentStrafeSign(), radialBias, speed);
	}
	enemy.PlayMoveAnimation(speed);

	const float shootRange = dangerMode ? enemy.GetLowHpShootRange() : enemy.GetFireRange();
	if (!inHitReaction && distToTarget <= shootRange && canShoot)
	{
		enemy.ChangeStateToShoot();
		return;
	}
}

void EnemyCombatMoveState::Exit(Enemy& enemy)
{
	(void)enemy;
}
