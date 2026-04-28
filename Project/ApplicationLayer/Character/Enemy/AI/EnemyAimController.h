#pragma once

#include <Vector3.h>

#include "EnemyTraitProfile.h"

class EnemyAimController
{
public:
	struct Input
	{
		Ken4lowEngine::Vector3 muzzlePosition{ 0.0f, 0.0f, 0.0f };
		Ken4lowEngine::Vector3 targetPosition{ 0.0f, 0.0f, 0.0f };
		float distanceToTarget = 0.0f;
		float movementSpeed = 0.0f;
		bool lowHp = false;
		bool inHitReaction = false;
		float baseSpreadRad = 0.01f;
		float maxSpreadRad = 0.2f;
		float distanceSpreadWeight = 1.0f;
		float moveSpreadWeight = 1.0f;
		float stressSpreadWeight = 1.0f;
		float stableBonusWeight = 0.5f;
		const EnemyTraitProfile* traits = nullptr;
	};

	[[nodiscard]] Ken4lowEngine::Vector3 BuildAimPoint(const Input& input) const;
};