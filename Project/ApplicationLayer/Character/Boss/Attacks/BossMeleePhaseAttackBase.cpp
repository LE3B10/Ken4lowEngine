#define NOMINMAX
#include "BossMeleePhaseAttackBase.h"

#include "BossAttackEffects.h"
#include "BossBase.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

BossMeleePhaseAttackBase::BossMeleePhaseAttackBase(
	const PhaseSettings& phaseSettings,
	const ImpactSettings& impactSettings)
	: settings_(phaseSettings), impact_(impactSettings)
{
	BossMeleeAttackUtility::NormalizeValidRange(settings_.minRange, settings_.maxRange);
	settings_.windupTime = std::max(0.0f, settings_.windupTime);
	settings_.holdTime = std::max(0.0f, settings_.holdTime);
	settings_.activeTime = std::max(0.0f, settings_.activeTime);
	settings_.recoveryTime = std::max(0.0f, settings_.recoveryTime);
	settings_.cooldownTime = std::max(0.0f, settings_.cooldownTime);
	BossMeleeAttackUtility::NormalizeHitSettings(impact_.hit);
	BossMeleeAttackUtility::NormalizeParticleSettings(impact_.particles);
	impact_.damage = std::max(0.0f, impact_.damage);
}

void BossMeleePhaseAttackBase::Initialize(BossBase* owner)
{
	owner_ = owner;
	phase_ = Phase::None;
	phaseTimer_ = 0.0f;
	totalTimer_ = 0.0f;
	cooldownRemaining_ = 0.0f;
	hasLockedDirection_ = false;
	isActive_ = false;
	isFinished_ = false;
	hasHit_ = false;
}

void BossMeleePhaseAttackBase::Start()
{
	if (!CanStart()) { return; }

	isActive_ = true;
	isFinished_ = false;
	hasHit_ = false;
	totalTimer_ = 0.0f;
	hasLockedDirection_ = BossMeleeAttackUtility::LockDirection(owner_, lockedForward_);
	ChangePhase(Phase::Windup);
}

void BossMeleePhaseAttackBase::Update(float deltaTime)
{
	if (!isActive_) { return; }

	totalTimer_ += deltaTime;
	phaseTimer_ += deltaTime;
	switch (phase_)
	{
	case Phase::Windup: UpdateWindup(); break;
	case Phase::Hold: UpdateHold(); break;
	case Phase::Active: UpdateActive(); break;
	case Phase::Recovery: UpdateRecovery(); break;
	case Phase::None:
	default: break;
	}
}

void BossMeleePhaseAttackBase::End()
{
	if (!isActive_) { return; }

	isActive_ = false;
	isFinished_ = true;
	cooldownRemaining_ = settings_.cooldownTime;
	ChangePhase(Phase::None);
}

bool BossMeleePhaseAttackBase::CanStart() const
{
	return owner_ && !isActive_ && cooldownRemaining_ <= 0.0f && !owner_->IsDead() &&
		BossMeleeAttackUtility::IsTargetInRange(owner_, settings_.minRange, settings_.maxRange);
}

void BossMeleePhaseAttackBase::TickCooldown(float deltaTime)
{
	cooldownRemaining_ = std::max(0.0f, cooldownRemaining_ - std::max(0.0f, deltaTime));
}

void BossMeleePhaseAttackBase::SetValidRange(float minRange, float maxRange)
{
	settings_.minRange = minRange;
	settings_.maxRange = maxRange;
	BossMeleeAttackUtility::NormalizeValidRange(settings_.minRange, settings_.maxRange);
}

void BossMeleePhaseAttackBase::SetHitParameters(float hitRange, float hitRadius, float hitForwardOffset, float hitAngleDeg)
{
	impact_.hit.range = hitRange;
	impact_.hit.radius = hitRadius;
	impact_.hit.forwardOffset = hitForwardOffset;
	impact_.hit.angleDeg = hitAngleDeg;
	BossMeleeAttackUtility::NormalizeHitSettings(impact_.hit);
}

void BossMeleePhaseAttackBase::SetImpactParticleParameters(
	uint32_t spawnCount,
	float spawnRadius,
	float lifetimeScale,
	float initialSpeedScale)
{
	impact_.particles = { spawnCount, spawnRadius, lifetimeScale, initialSpeedScale };
	BossMeleeAttackUtility::NormalizeParticleSettings(impact_.particles);
}

void BossMeleePhaseAttackBase::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Separator();
	ImGui::Text("[%s]", GetName());
	ImGui::Text("Active            : %s", isActive_ ? "true" : "false");
	ImGui::Text("Finished          : %s", isFinished_ ? "true" : "false");
	ImGui::Text("HasHit            : %s", hasHit_ ? "true" : "false");
	ImGui::Text("Phase             : %s", GetPhaseName());
	ImGui::Text("PhaseTimer        : %.2f", phaseTimer_);
	ImGui::Text("TotalTimer        : %.2f", totalTimer_);
	ImGui::Text("CooldownRemaining : %.2f", cooldownRemaining_);
	ImGui::Text("Range             : %.2f - %.2f", settings_.minRange, settings_.maxRange);
	ImGui::Text("HitRange          : %.2f", impact_.hit.range);
	ImGui::Text("HitRadius         : %.2f", impact_.hit.radius);
	ImGui::Text("HitForwardOffset  : %.2f", impact_.hit.forwardOffset);
	ImGui::Text("HitAngleDeg       : %.2f", impact_.hit.angleDeg);
	ImGui::Text("Damage            : %.2f", impact_.damage);
#endif
}

void BossMeleePhaseAttackBase::ExecuteHit()
{
	if (!owner_) { return; }
	const Ken4lowEngine::Vector3 forward = hasLockedDirection_
		? lockedForward_
		: Ken4lowEngine::Vector3{ std::sin(owner_->GetYaw()), 0.0f, std::cos(owner_->GetYaw()) };
	Ken4lowEngine::Vector3 hitCenter{};
	if (!BossMeleeAttackUtility::TryCalculateHitCenter(*owner_, forward, impact_.hit, hitCenter)) { return; }
	if (!owner_->ApplyDamageToTargetPlayer(impact_.damage, &hitCenter)) { return; }

	// 攻撃固有の差をImpactSettingsへ集約し、命中時の演出生成まで共通経路で処理する。
	BossAttackEffects::EmitGuardianHitEffect(
		impact_.emitterName,
		impact_.particleType,
		hitCenter,
		impact_.particles.spawnCount,
		impact_.particles.spawnRadius,
		impact_.particles.lifetimeScale,
		impact_.particles.initialSpeedScale);
}

const char* BossMeleePhaseAttackBase::GetPhaseName() const
{
	switch (phase_)
	{
	case Phase::Windup: return "Windup";
	case Phase::Hold: return "Hold";
	case Phase::Active: return "Active";
	case Phase::Recovery: return "Recovery";
	case Phase::None:
	default: return "None";
	}
}

void BossMeleePhaseAttackBase::ChangePhase(Phase newPhase)
{
	phase_ = newPhase;
	phaseTimer_ = 0.0f;
}

void BossMeleePhaseAttackBase::UpdateWindup()
{
	if (phaseTimer_ < settings_.windupTime) { return; }
	ChangePhase(settings_.holdTime > 0.0f ? Phase::Hold : Phase::Active);
}

void BossMeleePhaseAttackBase::UpdateHold()
{
	if (phaseTimer_ >= settings_.holdTime) { ChangePhase(Phase::Active); }
}

void BossMeleePhaseAttackBase::UpdateActive()
{
	if (!hasHit_)
	{
		ExecuteHit();
		hasHit_ = true;
	}
	if (phaseTimer_ >= settings_.activeTime) { ChangePhase(Phase::Recovery); }
}

void BossMeleePhaseAttackBase::UpdateRecovery()
{
	if (phaseTimer_ >= settings_.recoveryTime) { End(); }
}
