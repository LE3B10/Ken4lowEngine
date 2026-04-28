#pragma once

class EnemyEvadeController
{
public:
	enum class Mode
	{
		None,
		Strafe,
		Retreat,
		ToCover,
	};

	struct Input
	{
		bool inHitReaction = false;
		bool lowHp = false;
		bool canShoot = true;
		int consecutiveHits = 0;
		float evadeWeight = 0.7f;
		float coverBias = 0.55f;
		float aggression = 0.5f;
		float coverPreference = 0.5f;
	};

	struct Plan
	{
		Mode mode = Mode::None;
		float radialBias = 0.0f;
		float speedScale = 1.0f;
	};

	[[nodiscard]] Plan Evaluate(const Input& input) const;
};