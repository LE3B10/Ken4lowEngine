#include "EnemyTacticalDebugPointBuilder.h"

#include <algorithm>
#include <cmath>

using namespace Ken4lowEngine;

namespace
{
	constexpr float kEpsilon = 0.0001f;

	float LengthXZ(const Vector3& v)
	{
		return std::sqrt(v.x * v.x + v.z * v.z);
	}

	Vector3 NormalizeXZ(const Vector3& v)
	{
		const float len = LengthXZ(v);
		if (len <= kEpsilon) return { 0.0f, 0.0f, 0.0f };
		return { v.x / len, 0.0f, v.z / len };
	}

	float EvaluateRangeScore(const Vector3& candidate, const Vector3& target, float idealRange, float minRange, float maxRange)
	{
		Vector3 toTarget = target - candidate;
		toTarget.y = 0.0f;
		const float distance = LengthXZ(toTarget);
		const float safeIdeal = std::max(idealRange, kEpsilon);
		const float rangeError = std::abs(distance - safeIdeal) / safeIdeal;
		float score = 1.0f - std::clamp(rangeError, 0.0f, 1.0f);

		if (distance < minRange)
		{
			score -= std::clamp((minRange - distance) / std::max(minRange, kEpsilon), 0.0f, 1.0f) * 0.45f;
		}
		if (distance > maxRange)
		{
			score -= std::clamp((distance - maxRange) / std::max(maxRange, kEpsilon), 0.0f, 1.0f) * 0.25f;
		}

		return std::clamp(score, 0.0f, 1.0f);
	}

	void AddPoint(std::vector<EnemyTacticalDebugPoint>& points, const Vector3& position, float score, bool valid)
	{
		EnemyTacticalDebugPoint point{};
		point.position = position;
		point.score = score;
		point.valid = valid;
		points.push_back(point);
	}
}

std::vector<EnemyTacticalDebugPoint> EnemyTacticalDebugPointBuilder::Build(const Input& input, const Config& config)
{
	std::vector<EnemyTacticalDebugPoint> points;
	points.reserve(8);

	Vector3 toTarget = input.targetPosition - input.enemyPosition;
	toTarget.y = 0.0f;
	const Vector3 forward = NormalizeXZ(toTarget);
	if (LengthXZ(forward) <= kEpsilon)
	{
		return points;
	}

	const Vector3 right{ forward.z, 0.0f, -forward.x };
	const Vector3 back = forward * -1.0f;
	const float strafe = std::max(config.strafeDistance, 0.1f);
	const float wideStrafe = strafe * 1.75f;
	const float retreat = std::max(config.retreatDistance, 0.1f);
	const float approach = std::max(config.approachDistance, 0.1f);

	const Vector3 bases[] = {
		input.enemyPosition - right * strafe,
		input.enemyPosition + right * strafe,
		input.enemyPosition - right * wideStrafe,
		input.enemyPosition + right * wideStrafe,
		input.enemyPosition + back * retreat,
		input.enemyPosition + (back - right) * (retreat * 0.75f),
		input.enemyPosition + (back + right) * (retreat * 0.75f),
		input.enemyPosition + forward * approach,
	};

	for (const Vector3& candidate : bases)
	{
		const float score = EvaluateRangeScore(candidate, input.targetPosition, input.idealCombatRange, input.minCombatRange, input.maxCombatRange);
		Vector3 candidateToTarget = input.targetPosition - candidate;
		candidateToTarget.y = 0.0f;
		const float candidateRange = LengthXZ(candidateToTarget);
		// 今回は移動先決定に使わず、距離条件だけで候補点の有効/無効を可視化します。
		const bool valid =
			std::isfinite(score) &&
			candidateRange >= input.minCombatRange &&
			candidateRange <= input.maxCombatRange;
		AddPoint(points, candidate, score, valid);
	}

	auto best = std::max_element(points.begin(), points.end(), [](const EnemyTacticalDebugPoint& lhs, const EnemyTacticalDebugPoint& rhs)
	{
		if (lhs.valid != rhs.valid) return !lhs.valid && rhs.valid;
		return lhs.score < rhs.score;
	});
	if (best != points.end() && best->valid)
	{
		best->selected = true;
	}

	return points;
}