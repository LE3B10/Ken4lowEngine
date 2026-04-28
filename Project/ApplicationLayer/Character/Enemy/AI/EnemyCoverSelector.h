#pragma once

#include <Vector3.h>

class CollisionManager;

class EnemyCoverSelector
{
public:
	struct Config
	{
		float eyeHeight = 1.2f;
		float targetEyeHeight = 1.2f;
		float viewRange = 30.0f;
		bool useLOS = true;
		float coverSearchRadius = 10.0f;
		int coverSampleCount = 14;
		float retreatDistanceScoreWeight = 0.65f;
	};

	[[nodiscard]] bool TryFindCoverPosition(
		const Config& config,
		CollisionManager* collisionManager,
		const Ken4lowEngine::Vector3& selfPos,
		const Ken4lowEngine::Vector3& targetPos,
		bool preferRetreat,
		Ken4lowEngine::Vector3& outPosition) const;
};