#pragma once

class EnemyRetreatDecisionMemory
{
public:
	struct Config
	{
		float hpThreshold = 0.5f;
		float hpRecoverThreshold = 0.62f;
		float engageDistanceBias = 2.0f;
		float minRetreatHoldSec = 1.1f;
		float safeTimeToReleaseSec = 1.4f;
	};

	struct Input
	{
		float dt = 0.0f;
		float hpRate = 1.0f;
		float distanceToTarget = 9999.0f;
		float retreatDistance = 18.0f;
		float returnDistance = 28.0f;
		float decisionInterval = 0.2f;
		bool inHitReaction = false;
		bool canShoot = true;
		int consecutiveHits = 0;
	};

	void Reset();
	void SetConfig(const Config& config) { config_ = config; }
	[[nodiscard]] bool Update(const Input& input);
	[[nodiscard]] bool IsRetreating() const { return retreating_; }

private:
	Config config_{};
	bool retreating_ = false;
	float evalTimer_ = 0.0f;
	float retreatHoldTimer_ = 0.0f;
	float safeTimer_ = 0.0f;
};
