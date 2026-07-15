#define NOMINMAX
#include "MeleeAttackController.h"

#include "../Core/MeleeEnemy.h"
#include "ApplicationLayer/Character/Player/IPlayerRuntime.h"
#include "Actor.h"
#include "MathUtil.h"

#include <algorithm>
#include <cmath>

namespace K4E = ::Ken4lowEngine;

namespace
{
	constexpr float kEpsilon = 0.0001f;
	float LengthXZ(const K4E::Vector3& v) { return K4E::Vector3::LengthXZ(v); }
	K4E::Vector3 NormalizeXZ(const K4E::Vector3& v)
	{
		const float len = LengthXZ(v);
		if (len < kEpsilon) return { 0.0f, 0.0f, 1.0f };
		return { v.x / len, 0.0f, v.z / len };
	}
}

void MeleeAttackController::Initialize()
{
	patterns_.clear();
	MeleeAttackPattern scratch{};
	scratch.type = MeleeAttackType::Scratch;
	scratch.name = "Scratch";
	scratch.recoveryTime = 0.35f;
	scratch.cooldown = 0.55f;
	scratch.steps = { { "Scratch", 8, 0.12f, 0.10f, 2.2f, 0.8f, 0.0f, 0.0f } };
	patterns_[static_cast<int>(scratch.type)] = scratch;

	MeleeAttackPattern lungeScratch{};
	lungeScratch.type = MeleeAttackType::LungeScratch;
	lungeScratch.name = "LungeScratch";
	lungeScratch.recoveryTime = 0.75f;
	lungeScratch.cooldown = 1.25f;
	lungeScratch.forwardMoveSpeed = 3.0f;
	lungeScratch.forwardMoveDuration = 0.35f;
	lungeScratch.steps = { { "LungeScratch", 14, 0.24f, 0.14f, 3.2f, 1.0f, 0.0f, 0.0f } };
	patterns_[static_cast<int>(lungeScratch.type)] = lungeScratch;
}

void MeleeAttackController::StartAttack(MeleeAttackType type)
{
	if (!CanStartAttack()) return;
	const MeleeAttackPattern* pattern = FindPattern(type);
	if (!pattern || pattern->steps.empty()) return;
	currentType_ = type;
	attackElapsed_ = 0.0f;
	currentStepIndex_ = -1;
	isAttacking_ = true;
	isCurrentStepActive_ = false;
	lastHitSuccess_ = false;
	stepHitDone_.assign(pattern->steps.size(), false);
}

void MeleeAttackController::StopAttack()
{
	isAttacking_ = false;
	attackElapsed_ = 0.0f;
	currentStepIndex_ = -1;
	isCurrentStepActive_ = false;
}

void MeleeAttackController::ResetCooldown() { cooldownRemaining_ = 0.0f; }

void MeleeAttackController::Update(MeleeEnemy& owner, float deltaTime)
{
	if (cooldownRemaining_ > 0.0f) cooldownRemaining_ = std::max(0.0f, cooldownRemaining_ - deltaTime);
	if (!isAttacking_) return;

	attackElapsed_ += deltaTime;
	const MeleeAttackPattern* pattern = FindPattern(currentType_);
	if (!pattern || pattern->steps.empty())
	{
		isAttacking_ = false;
		return;
	}

	ApplyForwardMove(owner, deltaTime);
	isCurrentStepActive_ = false;
	currentStepIndex_ = -1;
	for (size_t i = 0; i < pattern->steps.size(); ++i)
	{
		const MeleeAttackStep& step = pattern->steps[i];
		const float stepEnd = step.startTime + step.activeTime;
		if (attackElapsed_ >= step.startTime && attackElapsed_ < stepEnd)
		{
			isCurrentStepActive_ = true;
			currentStepIndex_ = static_cast<int>(i);
			if (!stepHitDone_[i])
			{
				ProcessStepHit(owner, step);
				stepHitDone_[i] = true;
			}
		}
	}

	const MeleeAttackStep& lastStep = pattern->steps.back();
	const float attackEnd = lastStep.startTime + lastStep.activeTime + pattern->recoveryTime;
	if (attackElapsed_ >= attackEnd)
	{
		isAttacking_ = false;
		cooldownRemaining_ = pattern->cooldown;
		attackElapsed_ = 0.0f;
		currentStepIndex_ = -1;
		isCurrentStepActive_ = false;
	}
}

const char* MeleeAttackController::GetCurrentAttackName() const
{
	const MeleeAttackPattern* pattern = FindPattern(currentType_);
	return pattern ? pattern->name.c_str() : "None";
}

float MeleeAttackController::GetCurrentAttackNormalizedTime() const
{
	const MeleeAttackPattern* pattern = FindPattern(currentType_);
	if (!pattern || pattern->steps.empty()) return 0.0f;
	const MeleeAttackStep& lastStep = pattern->steps.back();
	const float attackEnd = lastStep.startTime + lastStep.activeTime + pattern->recoveryTime;
	if (attackEnd <= kEpsilon) return 0.0f;
	return std::clamp(attackElapsed_ / attackEnd, 0.0f, 1.0f);
}

float MeleeAttackController::GetCurrentStepNormalizedTime() const
{
	const MeleeAttackPattern* pattern = FindPattern(currentType_);
	if (!pattern || currentStepIndex_ < 0 || static_cast<size_t>(currentStepIndex_) >= pattern->steps.size()) return 0.0f;
	const MeleeAttackStep& step = pattern->steps[static_cast<size_t>(currentStepIndex_)];
	if (step.activeTime <= kEpsilon) return 0.0f;
	return std::clamp((attackElapsed_ - step.startTime) / step.activeTime, 0.0f, 1.0f);
}

MeleeAttackPattern* MeleeAttackController::FindPattern(MeleeAttackType type)
{
	auto it = patterns_.find(static_cast<int>(type));
	return it != patterns_.end() ? &it->second : nullptr;
}

const MeleeAttackPattern* MeleeAttackController::FindPattern(MeleeAttackType type) const
{
	auto it = patterns_.find(static_cast<int>(type));
	return it != patterns_.end() ? &it->second : nullptr;
}

void MeleeAttackController::ProcessStepHit(MeleeEnemy& owner, const MeleeAttackStep& step)
{
	lastHitSuccess_ = false;
	K4E::Collider* target = owner.GetTargetCollider();
	if (!target) return;

	const K4E::Vector3 ownerPos = owner.GetCenterPosition();
	const K4E::Vector3 forward = NormalizeXZ(owner.GetAttackForward());
	const K4E::Vector3 attackCenter = ownerPos + (forward * step.range);
	const float hitDist = K4E::MathUtil::DistanceXZ(target->GetCenterPosition(), attackCenter);
	if (hitDist > step.radius) return;

	lastHitSuccess_ = true;
	K4E::Actor* targetActor = target->GetOwner<K4E::Actor>();
	if (auto* player = dynamic_cast<IPlayerRuntime*>(targetActor))
	{
		const K4E::Vector3 attackerPosition = owner.GetCenterPosition();
		player->ApplyDamage(static_cast<float>(step.damage), &attackerPosition); // ActorからRuntimeへdynamic_castし、多重継承のポインタ補正を安全に行う。
	}
}

void MeleeAttackController::ApplyForwardMove(MeleeEnemy& owner, float) const
{
	const MeleeAttackPattern* pattern = FindPattern(currentType_);
	if (!pattern) return;
	if (pattern->forwardMoveSpeed <= 0.0f || attackElapsed_ > pattern->forwardMoveDuration) return;
	const K4E::Vector3 moveForward = NormalizeXZ(owner.GetAttackForward());
	owner.ApplyAttackMove(moveForward * pattern->forwardMoveSpeed);
}
