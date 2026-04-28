#define NOMINMAX
#include "EnemyCoverController.h"

#include <algorithm>
#include <cmath>
#include <random>

using namespace Ken4lowEngine;

namespace
{
	constexpr float kEpsilon = 0.0001f;

	float LengthXZ(const Vector3& value)
	{
		return std::sqrt(value.x * value.x + value.z * value.z);
	}

	Vector3 NormalizeXZ(const Vector3& value)
	{
		const float len = LengthXZ(value);
		if (len <= kEpsilon)
		{
			return { 0.0f, 0.0f, 0.0f };
		}
		return { value.x / len, 0.0f, value.z / len };
	}

	float RandomRange(float minValue, float maxValue)
	{
		thread_local std::mt19937 engine{ std::random_device{}() };
		std::uniform_real_distribution<float> dist(minValue, maxValue);
		return dist(engine);
	}
}

void EnemyCoverController::Reset()
{
	phaseTimer_ = 0.0f;
	exposing_ = false;
}

EnemyCoverController::Output EnemyCoverController::Update(const UpdateInput& input)
{
	Output output{};
	if (!input.hasCover)
	{
		Reset();
		return output;
	}

	output.active = true;
	output.moveTarget = input.coverPos;

	phaseTimer_ -= input.dt;
	if (phaseTimer_ <= 0.0f)
	{
		exposing_ = !exposing_;
		if (input.dangerMode && exposing_)
		{
			exposing_ = false;
		}

		if (exposing_)
		{
			phaseTimer_ = RandomRange(config_.peekExposeMinSec, config_.peekExposeMaxSec);
		}
		else
		{
			phaseTimer_ = RandomRange(config_.peekHideMinSec, config_.peekHideMaxSec);
		}
	}

	Vector3 toTarget = input.targetPos - input.coverPos;
	toTarget.y = 0.0f;
	const Vector3 fwd = NormalizeXZ(toTarget);
	Vector3 right{ fwd.z, 0.0f, -fwd.x };
	if (LengthXZ(right) <= kEpsilon)
	{
		right = { 1.0f, 0.0f, 0.0f };
	}

	const bool useRight = (input.losRightScore >= input.losLeftScore);
	const float sign = useRight ? 1.0f : -1.0f;
	const Vector3 peekPos = input.coverPos + right * (config_.peekOffset * sign);
	const bool arrivedAtCover = LengthXZ(input.selfPos - input.coverPos) <= config_.coverArriveDistance;
	if (exposing_ && arrivedAtCover)
	{
		output.moveTarget = peekPos;
		output.exposing = true;
		output.shouldShoot = std::max(input.losLeftScore, input.losRightScore) > 0.65f;
	}

	return output;
}