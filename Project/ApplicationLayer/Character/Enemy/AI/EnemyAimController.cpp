#define NOMINMAX
#include "EnemyAimController.h"

#include <algorithm>
#include <cmath>
#include <random>

using namespace Ken4lowEngine;

namespace
{
	constexpr float kEpsilon = 0.0001f;

	float Clamp(float value, float minValue, float maxValue)
	{
		if (value < minValue) return minValue;
		if (value > maxValue) return maxValue;
		return value;
	}

	Vector3 NormalizeSafe(const Vector3& value)
	{
		const float len = Vector3::Length(value);
		if (len <= kEpsilon)
		{
			return { 0.0f, 0.0f, 1.0f };
		}
		return value * (1.0f / len);
	}

	float NextNormal01()
	{
		thread_local std::mt19937 engine{ std::random_device{}() };
		static thread_local std::normal_distribution<float> dist(0.0f, 1.0f);
		return dist(engine);
	}
}

Vector3 EnemyAimController::BuildAimPoint(const Input& input) const
{
	const Vector3 targetDir = NormalizeSafe(input.targetPosition - input.muzzlePosition);
	Vector3 right{ targetDir.z, 0.0f, -targetDir.x };
	if (Vector3::Length(right) <= kEpsilon)
	{
		right = { 1.0f, 0.0f, 0.0f };
	}
	right = NormalizeSafe(right);
	Vector3 up = NormalizeSafe(Vector3::Cross(right, targetDir));

	const float distanceFactor = Clamp(input.distanceToTarget / 24.0f, 0.0f, 1.2f) * input.distanceSpreadWeight;
	const float speedFactor = Clamp(input.movementSpeed / 4.2f, 0.0f, 1.0f) * input.moveSpreadWeight;
	const float stress = (input.lowHp ? 0.35f : 0.0f) + (input.inHitReaction ? 0.5f : 0.0f);
	const float traitStability = input.traits ? input.traits->aimStability : 0.5f;
	const float stabilityScale = Clamp(1.0f - (traitStability * input.stableBonusWeight), 0.55f, 1.2f);
	const float spread = Clamp(
		input.baseSpreadRad + (distanceFactor + speedFactor + stress * input.stressSpreadWeight) * 0.08f,
		input.baseSpreadRad,
		input.maxSpreadRad) * stabilityScale;

	const float x = NextNormal01();
	const float y = NextNormal01();
	const Vector3 jitter = right * (x * spread) + up * (y * spread);
	const Vector3 finalDir = NormalizeSafe(targetDir + jitter);
	const float travel = std::max(2.0f, input.distanceToTarget);
	return input.muzzlePosition + finalDir * travel;
}