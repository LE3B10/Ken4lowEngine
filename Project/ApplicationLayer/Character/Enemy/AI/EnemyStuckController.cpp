#include "EnemyStuckController.h"

#include <algorithm>
#include <cmath>

using namespace Ken4lowEngine;

namespace
{
	float LengthXZ(const Vector3& value)
	{
		return std::sqrt(value.x * value.x + value.z * value.z);
	}
}

void EnemyStuckController::Reset(const Vector3& startPos)
{
	probePosition_ = startPos;
	probeTimer_ = config_.checkIntervalSec;
	stuckAccumSec_ = 0.0f;
	unstuckTimer_ = 0.0f;
	jumpRetryCooldownSec_ = 0.0f;
	consecutiveStuckEvents_ = 0;
	initialized_ = true;
}

EnemyStuckController::Output EnemyStuckController::Update(const UpdateInput& input)
{
	if (!initialized_)
	{
		Reset(input.selfPos);
	}

	Output output{};
	if (unstuckTimer_ > 0.0f)
	{
		unstuckTimer_ = std::max(0.0f, unstuckTimer_ - input.dt);
	}
	if (jumpRetryCooldownSec_ > 0.0f)
	{
		jumpRetryCooldownSec_ = std::max(0.0f, jumpRetryCooldownSec_ - input.dt);
	}

	probeTimer_ -= input.dt;
	if (probeTimer_ > 0.0f)
	{
		output.isStuck = (unstuckTimer_ > 0.0f);
		return output;
	}

	probeTimer_ = config_.checkIntervalSec;

	const float moved = LengthXZ(input.selfPos - probePosition_);
	probePosition_ = input.selfPos;

	if (input.moveCommanded && moved < config_.minProgressDistance)
	{
		stuckAccumSec_ += config_.checkIntervalSec;
	}
	else
	{
		stuckAccumSec_ = std::max(0.0f, stuckAccumSec_ - config_.checkIntervalSec * 1.5f);
		if (moved >= config_.minProgressDistance * 1.2f)
		{
			consecutiveStuckEvents_ = 0;
		}
	}

	if (stuckAccumSec_ >= config_.stuckThresholdSec)
	{
		output.shouldRepath = true;
		stuckAccumSec_ = 0.0f;
		unstuckTimer_ = config_.unstuckDurationSec;
		++consecutiveStuckEvents_;
		if (jumpRetryCooldownSec_ <= 0.0f || consecutiveStuckEvents_ >= 2)
		{
			output.shouldRetryJump = true;
			jumpRetryCooldownSec_ = 0.45f;
		}
	}

	output.isStuck = (unstuckTimer_ > 0.0f);
	return output;
}
