#pragma once

#include "BossMeleeAttackUtility.h"
#include "IBossAttack.h"

#include <cstdint>

namespace Ken4lowEngine
{
	enum class GpuParticleType : uint32_t;
}

/// 時間制御型のボス近接攻撃で共通するライフサイクルとフェーズ遷移を管理する基底クラス。
class BossMeleePhaseAttackBase : public IBossAttack
{
public:
	enum class Phase
	{
		None,
		Windup,
		Hold,
		Active,
		Recovery,
	};

	struct PhaseSettings
	{
		float windupTime = 0.0f;
		float holdTime = 0.0f;
		float activeTime = 0.0f;
		float recoveryTime = 0.0f;
		float cooldownTime = 0.0f;
		float minRange = 0.0f;
		float maxRange = 0.0f;
	};

	struct ImpactSettings
	{
		float damage = 0.0f;
		BossMeleeHitSettings hit{};
		BossImpactParticleSettings particles{};
		const char* emitterName = "";
		Ken4lowEngine::GpuParticleType particleType;
	};

	void Initialize(BossBase* owner) final;
	void Start() final;
	void Update(float deltaTime) final;
	void End() final;
	bool CanStart() const final;
	bool IsFinished() const final { return isFinished_; }
	bool IsActive() const final { return isActive_; }
	void TickCooldown(float deltaTime) final;
	float GetCooldownRemaining() const final { return cooldownRemaining_; }
	float GetMinRange() const final { return settings_.minRange; }
	float GetMaxRange() const final { return settings_.maxRange; }
	void Draw() final {}
	void DrawShadow() final {}
	void DrawImGui() final;

	Phase GetPhase() const { return phase_; }
	float GetPhaseTimer() const { return phaseTimer_; }
	float GetTotalTimer() const { return totalTimer_; }
	bool HasHit() const { return hasHit_; }
	const char* GetPhaseName() const;
	void SetValidRange(float minRange, float maxRange);
	float GetHitRange() const { return impact_.hit.range; }
	float GetHitRadius() const { return impact_.hit.radius; }
	float GetHitForwardOffset() const { return impact_.hit.forwardOffset; }
	float GetHitAngleDeg() const { return impact_.hit.angleDeg; }
	void SetHitParameters(float hitRange, float hitRadius, float hitForwardOffset, float hitAngleDeg);
	void SetImpactParticleParameters(uint32_t spawnCount, float spawnRadius, float lifetimeScale, float initialSpeedScale);

protected:
	BossMeleePhaseAttackBase(const PhaseSettings& phaseSettings, const ImpactSettings& impactSettings);

private:
	void ExecuteHit();
	void ChangePhase(Phase newPhase);
	void UpdateWindup();
	void UpdateHold();
	void UpdateActive();
	void UpdateRecovery();

	BossBase* owner_ = nullptr;
	PhaseSettings settings_{};
	ImpactSettings impact_{};
	Phase phase_ = Phase::None;
	float phaseTimer_ = 0.0f;
	float totalTimer_ = 0.0f;
	float cooldownRemaining_ = 0.0f;
	Ken4lowEngine::Vector3 lockedForward_{ 0.0f, 0.0f, 1.0f };
	bool hasLockedDirection_ = false;
	bool isActive_ = false;
	bool isFinished_ = false;
	bool hasHit_ = false;
};
