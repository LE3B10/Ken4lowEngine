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
	repathCooldownTimer_ = 0.0f;
	unstuckTimer_ = 0.0f;
	jumpRetryTimer_ = 0.0f;
	initialized_ = true;
}

EnemyStuckController::Output EnemyStuckController::Update(const UpdateInput& input)
{
	if (!initialized_)
	{
		Reset(input.selfPos);
	}

	Output output{};
	repathCooldownTimer_ = std::max(0.0f, repathCooldownTimer_ - input.dt);
	unstuckTimer_ = std::max(0.0f, unstuckTimer_ - input.dt);
	jumpRetryTimer_ = std::max(0.0f, jumpRetryTimer_ - input.dt);

	probeTimer_ -= input.dt;
	if (probeTimer_ <= 0.0f)
	{
		probeTimer_ = config_.checkIntervalSec;

		const float moved = LengthXZ(input.selfPos - probePosition_);
		probePosition_ = input.selfPos;

		if (input.moveCommanded && moved < config_.minProgressDistance)
		{
			stuckAccumSec_ += config_.checkIntervalSec;
		}
		else
		{
			stuckAccumSec_ = std::max(0.0f, stuckAccumSec_ - config_.checkIntervalSec * 1.25f);
		}
	}

	const bool stuckDetected = stuckAccumSec_ >= config_.stuckThresholdSec;
	if (stuckDetected)
	{
		if (repathCooldownTimer_ <= 0.0f)
		{
			output.shouldRepath = true;
			repathCooldownTimer_ = config_.repathCooldownSec;
		}
		if (input.grounded && jumpRetryTimer_ <= 0.0f)
		{
			output.shouldRetryJump = true;
			jumpRetryTimer_ = config_.jumpRetryMinIntervalSec;
		}

		unstuckTimer_ = std::max(unstuckTimer_, config_.unstuckDurationSec);
		stuckAccumSec_ = config_.stuckThresholdSec * 0.5f;
	}

	output.isRecovering = (unstuckTimer_ > 0.0f);
	return output;
}
