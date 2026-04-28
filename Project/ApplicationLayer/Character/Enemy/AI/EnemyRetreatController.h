#pragma once

class EnemyRetreatController
{
public:
	struct Input
	{
		float distanceToTarget = 9999.0f;
		float idealRangeMin = 10.0f;
		float idealRangeMax = 17.5f;
		float tooCloseRange = 7.0f;
		float tooFarRange = 25.0f;
		float lowHpRetreatDistance = 18.0f;
		float lowHpReturnDistance = 28.0f;
		float approachSpeed = 3.2f;
		float retreatSpeed = 3.0f;
		float strafeSpeed = 2.8f;
		float lowHpRetreatSpeedScale = 1.25f;
		float hitReactionMoveWeight = 0.8f;
		bool isLowHp = false;
		bool inHitReaction = false;
		bool canShoot = true;
	};

	struct Plan
	{
		float radialBias = 0.0f;
		float speed = 0.0f;
		bool shouldPathChase = false;
		bool forceRetreat = false;
		bool dangerMode = false;
	};

	[[nodiscard]] Plan EvaluatePlan(const Input& input) const;
};