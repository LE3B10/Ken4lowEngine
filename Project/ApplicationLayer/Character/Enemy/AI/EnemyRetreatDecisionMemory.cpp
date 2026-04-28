#include "EnemyRetreatDecisionMemory.h"

#include <algorithm>

void EnemyRetreatDecisionMemory::Reset()
{
	retreating_ = false;
	evalTimer_ = 0.0f;
	retreatHoldTimer_ = 0.0f;
	safeTimer_ = 0.0f;
}

bool EnemyRetreatDecisionMemory::Update(const Input& input)
{
	if (retreatHoldTimer_ > 0.0f)
	{
		retreatHoldTimer_ = std::max(0.0f, retreatHoldTimer_ - input.dt);
	}

	evalTimer_ -= input.dt;
	if (evalTimer_ > 0.0f)
	{
		return retreating_;
	}
	evalTimer_ = std::max(0.05f, input.decisionInterval);

	const bool veryLowHp = input.hpRate <= config_.hpThreshold;
	const bool pressuredByHits = input.inHitReaction && input.consecutiveHits >= 2;
	const bool closeThreat = input.distanceToTarget <= (input.retreatDistance + config_.engageDistanceBias);
	const bool shouldEnterRetreat = veryLowHp && (closeThreat || pressuredByHits);

	if (!retreating_ && shouldEnterRetreat)
	{
		retreating_ = true;
		retreatHoldTimer_ = config_.minRetreatHoldSec;
		safeTimer_ = 0.0f;
		return retreating_;
	}

	if (!retreating_)
	{
		return retreating_;
	}

	const bool hpRecovered = input.hpRate >= config_.hpRecoverThreshold;
	const bool safeDistance = input.distanceToTarget >= input.returnDistance;
	const bool notPressured = !input.inHitReaction && input.consecutiveHits == 0;
	const bool hasShots = input.canShoot;

	if (safeDistance && hasShots && notPressured)
	{
		safeTimer_ += input.decisionInterval;
	}
	else
	{
		safeTimer_ = 0.0f;
	}

	if (retreatHoldTimer_ <= 0.0f && (hpRecovered || safeTimer_ >= config_.safeTimeToReleaseSec))
	{
		retreating_ = false;
		safeTimer_ = 0.0f;
	}

	return retreating_;
}
