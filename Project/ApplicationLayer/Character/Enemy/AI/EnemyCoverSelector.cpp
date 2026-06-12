#define NOMINMAX
#include "EnemyCoverSelector.h"

#include <CollisionManager.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <random>

using namespace Ken4lowEngine;

namespace
{
	constexpr float kPi = std::numbers::pi_v<float>;

	float Clamp(float value, float minVal, float maxVal)
	{
		if (value < minVal) return minVal;
		if (value > maxVal) return maxVal;
		return value;
	}

	float LengthXZ(const Vector3& v)
	{
		return std::sqrt(v.x * v.x + v.z * v.z);
	}

	float RandomRange(float minValue, float maxValue)
	{
		thread_local std::mt19937 engine{ std::random_device{}() };
		std::uniform_real_distribution<float> dist(minValue, maxValue);
		return dist(engine);
	}

	bool HasLineOfSight(CollisionManager* collisionManager, bool useLOS, const Vector3& fromPos, const Vector3& toPos)
	{
		if (!collisionManager || !useLOS) return true;

		const Vector3 diff = toPos - fromPos;
		const float distance = Vector3::Length(diff);
		if (distance <= 0.0001f) return true;

		RaycastQuery query{};
		query.origin = fromPos;
		query.direction = Vector3::NormalizeSafe(diff, { 0.0f, 0.0f, 1.0f });
		query.maxDistance = distance;
		query.traceChannel = ETraceChannel::Visibility;

		RaycastHit hit{};
		const bool blocked = collisionManager->RaycastSingle(query, hit);
		return !blocked;
	}

	float EvaluateLineOfSightScore(const EnemyCoverSelector::Config& config, CollisionManager* collisionManager, const Vector3& samplePos, const Vector3& targetPos)
	{
		Vector3 sampleEye = samplePos;
		sampleEye.y += config.eyeHeight;

		Vector3 targetEye = targetPos;
		targetEye.y += config.targetEyeHeight;

		if (HasLineOfSight(collisionManager, config.useLOS, sampleEye, targetEye))
		{
			return 1.0f;
		}

		const Vector3 toTarget = targetPos - samplePos;
		const float dist = LengthXZ(toTarget);
		const float normalize = std::max(0.1f, config.viewRange);
		return 0.15f + Clamp(1.0f - (dist / normalize), 0.0f, 0.35f);
	}
}

bool EnemyCoverSelector::TryFindCoverPosition(
	const Config& config,
	CollisionManager* collisionManager,
	const Vector3& selfPos,
	const Vector3& targetPos,
	bool preferRetreat,
	Vector3& outPosition) const
{
	float bestScore = -std::numeric_limits<float>::infinity();
	Vector3 bestPosition = selfPos;
	bool found = false;

	for (int i = 0; i < config.coverSampleCount; ++i)
	{
		const float t = static_cast<float>(i) / static_cast<float>(std::max(1, config.coverSampleCount));
		const float angle = (kPi * 2.0f * t) + RandomRange(-0.2f, 0.2f);
		const float radius = RandomRange(config.coverSearchRadius * 0.35f, config.coverSearchRadius);
		const Vector3 candidate{
			selfPos.x + std::cos(angle) * radius,
			selfPos.y,
			selfPos.z + std::sin(angle) * radius
		};

		Vector3 candidateEye = candidate;
		candidateEye.y += config.eyeHeight;
		Vector3 targetEye = targetPos;
		targetEye.y += config.targetEyeHeight;

		const bool blockedFromTarget = !HasLineOfSight(collisionManager, config.useLOS, targetEye, candidateEye);
		const float losScore = EvaluateLineOfSightScore(config, collisionManager, candidate, targetPos);

		Vector3 toTargetCurrent = targetPos - selfPos;
		toTargetCurrent.y = 0.0f;
		Vector3 toTargetCandidate = targetPos - candidate;
		toTargetCandidate.y = 0.0f;

		const float currentDist = std::max(0.1f, LengthXZ(toTargetCurrent));
		const float candidateDist = LengthXZ(toTargetCandidate);
		const float retreatDelta = Clamp((candidateDist - currentDist) / currentDist, -1.0f, 1.0f);

		float score = blockedFromTarget ? 2.2f : 0.0f;
		score += (1.0f - losScore) * 1.5f;
		if (preferRetreat)
		{
			score += retreatDelta * config.retreatDistanceScoreWeight;
		}
		else
		{
			score += std::fabs(retreatDelta) * 0.2f;
		}

		if (score > bestScore)
		{
			bestScore = score;
			bestPosition = candidate;
			found = (score > 0.75f);
		}
	}

	if (!found)
	{
		return false;
	}

	outPosition = bestPosition;
	return true;
}
