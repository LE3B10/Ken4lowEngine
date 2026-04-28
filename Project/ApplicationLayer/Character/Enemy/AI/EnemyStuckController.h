#pragma once

#include <Vector3.h>

class EnemyStuckController
{
public:
	struct Config
	{
		float checkIntervalSec = 0.3f;
		float minProgressDistance = 0.2f;
		float stuckThresholdSec = 0.9f;
		float unstuckDurationSec = 0.7f;
	};

	struct UpdateInput
	{
		Ken4lowEngine::Vector3 selfPos{ 0.0f, 0.0f, 0.0f };
		float dt = 0.0f;
		bool moveCommanded = false;
	};

	struct Output
	{
		bool isStuck = false;
		bool shouldRepath = false;
	};

	void Reset(const Ken4lowEngine::Vector3& startPos);
	[[nodiscard]] Output Update(const UpdateInput& input);
	[[nodiscard]] bool IsUnstucking() const { return unstuckTimer_ > 0.0f; }

private:
	Config config_{};
	Ken4lowEngine::Vector3 probePosition_{ 0.0f, 0.0f, 0.0f };
	float probeTimer_ = 0.0f;
	float stuckAccumSec_ = 0.0f;
	float unstuckTimer_ = 0.0f;
	bool initialized_ = false;
};
