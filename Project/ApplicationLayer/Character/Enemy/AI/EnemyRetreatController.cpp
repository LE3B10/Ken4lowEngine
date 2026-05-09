#include "EnemyRetreatController.h"

#include <algorithm>

namespace
{
	float Clamp(float value, float minVal, float maxVal)
	{
		if (value < minVal) return minVal;
		if (value > maxVal) return maxVal;
		return value;
	}
}

EnemyRetreatController::Plan EnemyRetreatController::EvaluatePlan(const Input& input) const
{
	Plan plan{};
	plan.dangerMode = input.isLowHp || input.inHitReaction;

	const float mid = (input.idealRangeMin + input.idealRangeMax) * 0.5f;
	const float half = std::max(0.2f, (input.idealRangeMax - input.idealRangeMin) * 0.5f);
	plan.radialBias = Clamp((input.distanceToTarget - mid) / half, -1.0f, 1.0f);
	plan.speed = input.strafeSpeed;

	if (plan.dangerMode)
	{
		const float reactionScale = input.inHitReaction ? (1.0f + input.hitReactionMoveWeight * 0.55f) : 1.0f;
		const float lowHpRetreatSpeed = input.retreatSpeed * input.lowHpRetreatSpeedScale * reactionScale;

		if (input.distanceToTarget < input.lowHpRetreatDistance || input.inHitReaction)
		{
			plan.forceRetreat = true;
			plan.speed = lowHpRetreatSpeed;
			plan.radialBias = -1.0f;
		}
		else if (input.distanceToTarget > input.lowHpReturnDistance)
		{
			plan.speed = input.approachSpeed * 0.9f;
			plan.radialBias = 1.0f;
			plan.shouldPathChase = true;
		}
		else
		{
			plan.speed = input.strafeSpeed;
			plan.radialBias = Clamp(plan.radialBias, -0.9f, 0.2f);
		}
	}
	else if (input.distanceToTarget < input.tooCloseRange)
	{
		plan.speed = input.retreatSpeed;
		plan.radialBias = -1.0f;
	}
	else if (input.distanceToTarget > input.tooFarRange)
	{
		plan.speed = input.approachSpeed;
		plan.radialBias = 1.0f;
		plan.shouldPathChase = true;
	}
	else if (input.distanceToTarget < input.idealRangeMin)
	{
		plan.speed = input.retreatSpeed;
	}
	else if (input.distanceToTarget > input.idealRangeMax)
	{
		plan.speed = input.approachSpeed;
		plan.shouldPathChase = true;
	}

	if (!input.canShoot)
	{
		if (input.distanceToTarget < input.idealRangeMin)
		{
			plan.radialBias = std::min(plan.radialBias, -0.85f);
			plan.shouldPathChase = false;
		}
		else if (plan.dangerMode)
		{
			plan.radialBias = std::min(plan.radialBias, -0.45f);
			plan.shouldPathChase = true;
		}
		else
		{
			plan.radialBias = std::max(plan.radialBias, -0.1f);
			plan.shouldPathChase = true;
		}
		plan.speed = std::max(plan.speed, input.strafeSpeed);
	}

	return plan;
}