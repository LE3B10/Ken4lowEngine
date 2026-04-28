#pragma once

#include <Vector3.h>

class EnemyStuckController
{
public:
	struct Config
	{
		float checkIntervalSec = 0.28f;
		float minProgressDistance = 0.18f;
		float stuckThresholdSec = 0.84f;
		float repathCooldownSec = 0.75f;
		float unstuckDurationSec = 0.7f;
		float jumpRetryMinIntervalSec = 0.28f;
	};

	struct UpdateInput
	{
		Ken4lowEngine::Vector3 selfPos{ 0.0f, 0.0f, 0.0f };
		float dt = 0.0f;
		bool moveCommanded = false;
		bool grounded = false;
	};

	struct Output
	{
		bool isRecovering = false;
		bool shouldRepath = false;
		bool shouldRetryJump = false;
	};

	void Reset(const Ken4lowEngine::Vector3& startPos);
	[[nodiscard]] Output Update(const UpdateInput& input);

private:
	Config config_{};
	Ken4lowEngine::Vector3 probePosition_{ 0.0f, 0.0f, 0.0f };
	float probeTimer_ = 0.0f;
	float stuckAccumSec_ = 0.0f;
	float repathCooldownTimer_ = 0.0f;
	float unstuckTimer_ = 0.0f;
	float jumpRetryTimer_ = 0.0f;
	bool initialized_ = false;
};
