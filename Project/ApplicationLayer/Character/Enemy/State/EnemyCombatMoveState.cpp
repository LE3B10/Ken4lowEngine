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
	retreatRepathTimer_ = 0.0f;
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

	const float idealMin = enemy.GetIdealRangeMin();
	const float idealMax = enemy.GetIdealRangeMax();
	const float tooClose = enemy.GetTooCloseRange();
	const float tooFar = enemy.GetTooFarRange();
	const float mid = (idealMin + idealMax) * 0.5f;
	const float half = std::max(0.2f, (idealMax - idealMin) * 0.5f);
	const bool lowHp = enemy.IsLowHp();
	const bool inHitReaction = enemy.IsInHitReaction();
	const bool dangerMode = lowHp || inHitReaction;

	float radialBias = Clamp((distToTarget - mid) / half, -1.0f, 1.0f);
	// radialBias > 0: 接近 / <0: 離脱
	float speed = enemy.GetStrafeSpeed();
	bool shouldPathChase = false;
	bool forceRetreat = false;

	if (dangerMode)
	{
		const float reactionScale = inHitReaction ? (1.0f + enemy.GetHitReactionMoveWeight() * 0.45f) : 1.0f;
		const float lowHpRetreatSpeed = enemy.GetRetreatSpeed() * enemy.GetLowHpRetreatSpeedScale() * reactionScale;

		if (distToTarget < enemy.GetLowHpRetreatDistance() || inHitReaction)
		{
			forceRetreat = true;
			speed = lowHpRetreatSpeed;
			radialBias = -1.0f;
		}
		else if (distToTarget > enemy.GetLowHpReturnDistance())
		{
			speed = enemy.GetApproachSpeed() * 0.85f;
			radialBias = 1.0f;
			shouldPathChase = true;
		}
		else
		{
			speed = enemy.GetStrafeSpeed();
			radialBias = Clamp(radialBias, -0.85f, 0.25f);
		}
	}
	else if (distToTarget < tooClose)
	{
		speed = enemy.GetRetreatSpeed();
		radialBias = -1.0f;
	}
	else if (distToTarget > tooFar)
	{
		speed = enemy.GetApproachSpeed();
		radialBias = 1.0f;
		shouldPathChase = true;
	}
	else if (distToTarget < idealMin)
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

		if (dangerMode)
		{
			radialBias = std::min(radialBias, -0.35f);
		}
		else
		{
			radialBias = std::max(radialBias, -0.1f);
		}
		speed = std::max(speed, enemy.GetStrafeSpeed());
		shouldPathChase = true;
	}

	if (forceRetreat)
	{
		retreatRepathTimer_ -= deltaTime;
		if (!hasRetreatTarget_ || retreatRepathTimer_ <= 0.0f)
		{
			Ken4lowEngine::Vector3 cover{};
			hasRetreatTarget_ = enemy.TryFindCoverPosition(targetPos, true, cover);
			if (hasRetreatTarget_)
			{
				retreatTarget_ = cover;
			}
			retreatRepathTimer_ = enemy.GetRetreatRepathInterval();
		}

		if (hasRetreatTarget_)
		{
			enemy.MoveTowardsPath(retreatTarget_, speed, deltaTime);
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
