#pragma once

#include <Vector3.h>

class EnemyCoverController
{
public:
	struct Config
	{
		float peekOffset = 1.55f;
		float peekExposeMinSec = 0.3f;
		float peekExposeMaxSec = 0.8f;
		float peekHideMinSec = 0.35f;
		float peekHideMaxSec = 0.95f;
		float coverArriveDistance = 1.1f;
	};

	struct UpdateInput
	{
		Ken4lowEngine::Vector3 selfPos{ 0.0f, 0.0f, 0.0f };
		Ken4lowEngine::Vector3 targetPos{ 0.0f, 0.0f, 0.0f };
		Ken4lowEngine::Vector3 coverPos{ 0.0f, 0.0f, 0.0f };
		float dt = 0.0f;
		float losLeftScore = 0.0f;
		float losRightScore = 0.0f;
		bool dangerMode = false;
		bool hasCover = false;
	};

	struct Output
	{
		bool active = false;
		bool exposing = false;
		bool shouldShoot = false;
		Ken4lowEngine::Vector3 moveTarget{ 0.0f, 0.0f, 0.0f };
	};

	void Reset();
	void SetConfig(const Config& config) { config_ = config; }
	[[nodiscard]] Output Update(const UpdateInput& input);

private:
	Config config_{};
	float phaseTimer_ = 0.0f;
	bool exposing_ = false;
};