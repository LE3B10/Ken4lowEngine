#pragma once
#include "IBossAttack.h"

#include <Vector3.h>
#include <cstdint>

namespace K4E = Ken4lowEngine;
class BossBase;

/// ---------------------------------------------------------------
///                 遠距離用の直進突進攻撃
/// ---------------------------------------------------------------
class BossChargeAttack : public IBossAttack
{
public:
	enum class Phase
	{
		None,
		Windup,
		Charging,
		Recovery
	};

	~BossChargeAttack() override = default;

	void Initialize(BossBase* owner) override;
	void Start() override;
	void Update(float deltaTime) override;
	void End() override;

	bool CanStart() const override;
	bool IsFinished() const override { return isFinished_; }
	bool IsActive() const override { return isActive_; }

	void TickCooldown(float deltaTime) override;
	float GetCooldownRemaining() const override { return cooldownRemaining_; }

	const char* GetName() const override { return "ChargeAttack"; }
	int GetPriority() const override { return priority_; }
	float GetMinRange() const override { return minRange_; }
	float GetMaxRange() const override { return maxRange_; }

	Phase GetPhase() const { return phase_; }
	float GetPhaseTimer() const { return phaseTimer_; }
	float GetTraveledDistance() const { return traveledDistance_; }
	float GetChargeSpeed() const { return chargeSpeed_; }
	float GetChargeDistance() const { return chargeDistance_; }
	float GetDamage() const { return damage_; }

	void SetValidRange(float minRange, float maxRange);
	void SetChargeParameters(float speed, float distance, float damage, float startupSec, float recoverySec, float cooldownSec);
	void SetImpactParticleParameters(uint32_t spawnCount, float spawnRadius, float lifetimeScale, float initialSpeedScale);

	void Draw() override;
	void DrawShadow() override {}
	void DrawImGui() override;

private:
	void UpdateWindup(float deltaTime);
	void UpdateCharging(float deltaTime);
	void UpdateRecovery(float deltaTime);
	void ChangePhase(Phase newPhase);
	void LockChargeDirection();
	void TryHitPlayer();
	bool IsTargetInValidRange() const;
	const char* GetPhaseName() const;

private:
	BossBase* owner_ = nullptr;

	bool isActive_ = false;
	bool isFinished_ = false;
	bool hasHit_ = false;
	bool hasWindupEffect_ = false;
	bool hasChargeReleaseEffect_ = false;
	bool hasRecoveryImpactEffect_ = false;
	Phase phase_ = Phase::None;
	float phaseTimer_ = 0.0f;
	float totalTimer_ = 0.0f;

	K4E::Vector3 lockedForward_{ 0.0f, 0.0f, 1.0f };
	K4E::Vector3 startPosition_{};
	bool hasLockedDirection_ = false;
	float traveledDistance_ = 0.0f;

	float minRange_ = 10.0f;
	float maxRange_ = 20.0f;

	float chargeSpeed_ = 18.0f;
	float chargeDistance_ = 12.0f;
	float damage_ = 20.0f;
	float startupTime_ = 0.6f;
	float recoveryTime_ = 1.0f;
	float hitRadius_ = 1.4f;
	float targetRadius_ = 0.65f;

	uint32_t particleSpawnCount_ = 72;
	float particleSpawnRadius_ = 0.75f;
	float particleLifetimeScale_ = 1.0f;
	float particleInitialSpeedScale_ = 1.0f;

	float cooldownSec_ = 8.0f;
	float cooldownRemaining_ = 0.0f;
	int priority_ = 85;
};
