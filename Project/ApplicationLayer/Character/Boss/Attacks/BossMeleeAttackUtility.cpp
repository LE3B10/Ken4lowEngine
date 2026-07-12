#define NOMINMAX
#include "BossMeleeAttackUtility.h"

#include "BossBase.h"

#include <algorithm>
#include <cmath>

bool BossMeleeAttackUtility::IsTargetInRange(const BossBase* owner, float minRange, float maxRange)
{
	if (!owner) { return false; }
	const float distance = owner->GetDistanceToTargetXZ();
	return distance >= minRange && distance <= maxRange;
}

bool BossMeleeAttackUtility::LockDirection(BossBase* owner, K4E::Vector3& lockedForward)
{
	if (!owner) { return false; }
	lockedForward = owner->GetDirectionToTargetXZOrForward(owner->GetCenterPosition());
	owner->FaceDirectionXZImmediate(lockedForward);
	return true;
}

bool BossMeleeAttackUtility::TryCalculateHitCenter(
	const BossBase& owner,
	const K4E::Vector3& forward,
	const BossMeleeHitSettings& inputSettings,
	K4E::Vector3& hitCenter)
{
	BossMeleeHitSettings settings = inputSettings;
	NormalizeHitSettings(settings);
	settings.range = std::max(settings.forwardOffset, settings.range);

	const K4E::Vector3 bossCenter = owner.GetCenterPosition();
	hitCenter = bossCenter;
	hitCenter.x += forward.x * settings.forwardOffset;
	hitCenter.y += settings.heightOffset;
	hitCenter.z += forward.z * settings.forwardOffset;

	const K4E::Vector3 targetCenter = owner.GetTargetPosition();
	const float toTargetX = targetCenter.x - bossCenter.x;
	const float toTargetZ = targetCenter.z - bossCenter.z;
	const float horizontalDistance = std::sqrt(toTargetX * toTargetX + toTargetZ * toTargetZ);
	const float forwardDistance = toTargetX * forward.x + toTargetZ * forward.z;
	const float closestForwardDistance = std::clamp(forwardDistance, settings.forwardOffset, settings.range);

	K4E::Vector3 closestPoint = bossCenter;
	closestPoint.x += forward.x * closestForwardDistance;
	closestPoint.y = hitCenter.y;
	closestPoint.z += forward.z * closestForwardDistance;
	const K4E::Vector3 offset = targetCenter - closestPoint;
	const float sumRadius = settings.radius + settings.targetRadius;
	const bool insideCapsule = K4E::Vector3::Dot(offset, offset) <= sumRadius * sumRadius;

	constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
	const float directionDot = horizontalDistance > 0.0001f ? forwardDistance / horizontalDistance : 1.0f;
	const float angleCos = std::cos(settings.angleDeg * 0.5f * kDegToRad);
	const bool insideAngle = settings.angleDeg >= 360.0f || directionDot >= angleCos;
	const bool insideRange = horizontalDistance <= settings.range + settings.targetRadius;
	const bool insideHeight = std::abs(targetCenter.y - hitCenter.y) <= sumRadius;
	return insideCapsule || (insideRange && insideAngle && insideHeight);
}

void BossMeleeAttackUtility::NormalizeValidRange(float& minRange, float& maxRange)
{
	minRange = std::max(0.0f, minRange);
	maxRange = std::max(minRange, maxRange);
}

void BossMeleeAttackUtility::NormalizeHitSettings(BossMeleeHitSettings& settings)
{
	settings.range = std::max(0.0f, settings.range);
	settings.radius = std::max(0.0f, settings.radius);
	settings.forwardOffset = std::max(0.0f, settings.forwardOffset);
	settings.angleDeg = std::clamp(settings.angleDeg, 0.0f, 360.0f);
	settings.targetRadius = std::max(0.0f, settings.targetRadius);
}

void BossMeleeAttackUtility::NormalizeParticleSettings(BossImpactParticleSettings& settings)
{
	settings.spawnRadius = std::max(0.0f, settings.spawnRadius);
	settings.lifetimeScale = std::max(0.01f, settings.lifetimeScale);
	settings.initialSpeedScale = std::max(0.0f, settings.initialSpeedScale);
}
