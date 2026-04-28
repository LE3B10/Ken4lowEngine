#include "EnemyEvadeController.h"

#include <algorithm>

EnemyEvadeController::Plan EnemyEvadeController::Evaluate(const Input& input) const
{
	Plan plan{};
	if (!input.inHitReaction)
	{
		return plan;
	}

	const float panic = std::clamp((static_cast<float>(input.consecutiveHits) - 1.0f) * 0.25f, 0.0f, 0.8f);
	const float survivalNeed = std::clamp(input.evadeWeight + panic + (input.lowHp ? 0.35f : 0.0f), 0.0f, 1.8f);
	const float coverNeed = std::clamp(input.coverBias * (0.6f + input.coverPreference * 0.9f) + panic, 0.0f, 1.8f);
	const float attackNeed = std::clamp(input.aggression - (input.lowHp ? 0.25f : 0.0f), 0.0f, 1.0f);

	if (coverNeed > 0.65f && (!input.canShoot || survivalNeed > attackNeed))
	{
		plan.mode = Mode::ToCover;
		plan.radialBias = -1.0f;
		plan.speedScale = 1.2f + panic;
		return plan;
	}

	if (survivalNeed > 0.55f)
	{
		plan.mode = Mode::Retreat;
		plan.radialBias = -0.9f;
		plan.speedScale = 1.1f + panic;
		return plan;
	}

	plan.mode = Mode::Strafe;
	plan.radialBias = -0.1f;
	plan.speedScale = 1.0f;
	return plan;
}