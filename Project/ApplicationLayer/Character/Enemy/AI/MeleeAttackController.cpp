#include "MeleeAttackController.h"

#include "../Core/MeleeEnemy.h"

#include <algorithm>
#include <cmath>

namespace K4E = ::Ken4lowEngine;

namespace
{
	constexpr float kEpsilon = 0.0001f;

	float LengthXZ(const K4E::Vector3& v)
	{
		return std::sqrt(v.x * v.x + v.z * v.z);
	}

	K4E::Vector3 NormalizeXZ(const K4E::Vector3& v)
	{
		const float len = LengthXZ(v);
		if (len < kEpsilon) { return { 0.0f, 0.0f, 1.0f }; }
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
	scratch.cooldown = 0.8f;
	scratch.steps = { { "Scratch", 8, 0.18f, 0.12f, 2.2f, 0.8f, 0.0f, 0.0f } };
	patterns_[static_cast<int>(scratch.type)] = scratch;

	MeleeAttackPattern oneTwo{};
	oneTwo.type = MeleeAttackType::OneTwo;
	oneTwo.name = "OneTwo";
	oneTwo.recoveryTime = 0.45f;
	oneTwo.cooldown = 1.15f;
	oneTwo.forwardMoveSpeed = 1.8f;
	oneTwo.forwardMoveDuration = 0.45f;
	oneTwo.steps = {
		{ "LeftScratch", 5, 0.16f, 0.10f, 2.0f, 0.75f, 0.0f, 0.0f },
		{ "RightScratch", 7, 0.42f, 0.12f, 2.1f, 0.8f, 0.0f, 0.0f },
	};
	patterns_[static_cast<int>(oneTwo.type)] = oneTwo;
}

void MeleeAttackController::StartAttack(MeleeAttackType type)
{
	if (!CanStartAttack()) { return; }
	const MeleeAttackPattern* pattern = FindPattern(type);
	if (!pattern || pattern->steps.empty()) { return; }
	currentType_ = type;
	attackElapsed_ = 0.0f;
	currentStepIndex_ = -1;
	isAttacking_ = true;
	isCurrentStepActive_ = false;
	lastHitSuccess_ = false;
	stepHitDone_.assign(pattern->steps.size(), false);
}

void MeleeAttackController::Update(MeleeEnemy& owner, float deltaTime)
{
	if (cooldownRemaining_ > 0.0f) { cooldownRemaining_ = std::max(0.0f, cooldownRemaining_ - deltaTime); }
	if (!isAttacking_) { return; }

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
	if (!target) { return; }

	const K4E::Vector3 ownerPos = owner.GetCenterPosition();
	const K4E::Vector3 forward = NormalizeXZ(owner.GetTargetPositionForAttack() - ownerPos);
	const K4E::Vector3 attackCenter = ownerPos + (forward * step.range);
	const K4E::Vector3 delta = target->GetCenterPosition() - attackCenter;
	const float hitDist = LengthXZ(delta);
	if (hitDist <= step.radius)
	{
		lastHitSuccess_ = true;
		owner.NotifyAttackHit(step.damage, forward);
	}
}

void MeleeAttackController::ApplyForwardMove(MeleeEnemy& owner, float) const
{
	const MeleeAttackPattern* pattern = FindPattern(currentType_);
	if (!pattern) { return; }
	if (pattern->forwardMoveSpeed <= 0.0f || attackElapsed_ > pattern->forwardMoveDuration) { return; }
	const K4E::Vector3 moveForward = NormalizeXZ(owner.GetTargetPositionForAttack() - owner.GetCenterPosition());
	owner.ApplyAttackMove(moveForward * pattern->forwardMoveSpeed);
}
