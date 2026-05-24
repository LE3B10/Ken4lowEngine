#pragma once

#include "MeleeAttackPattern.h"

#include <unordered_map>

namespace Ken4lowEngine { class Vector3; }
class MeleeEnemy;

class MeleeAttackController
{
public:
	void Initialize();
	void StartAttack(MeleeAttackType type);
	void StopAttack();
	void ResetCooldown();
	void Update(MeleeEnemy& owner, float deltaTime);

	bool IsAttacking() const { return isAttacking_; }
	bool CanStartAttack() const { return !isAttacking_ && cooldownRemaining_ <= 0.0f; }
	const char* GetCurrentAttackName() const;
	float GetAttackElapsed() const { return attackElapsed_; }
	int GetCurrentStepIndex() const { return currentStepIndex_; }
	float GetCooldownRemaining() const { return cooldownRemaining_; }
	bool IsCurrentStepActive() const { return isCurrentStepActive_; }
	bool WasLastHitSuccess() const { return lastHitSuccess_; }
	MeleeAttackType GetCurrentAttackType() const { return currentType_; }

	MeleeAttackPattern* FindPattern(MeleeAttackType type);
	const MeleeAttackPattern* FindPattern(MeleeAttackType type) const;
private:
	void ProcessStepHit(MeleeEnemy& owner, const MeleeAttackStep& step);
	void ApplyForwardMove(MeleeEnemy& owner, float deltaTime) const;

private:
	std::unordered_map<int, MeleeAttackPattern> patterns_;
	MeleeAttackType currentType_ = MeleeAttackType::Scratch;
	float attackElapsed_ = 0.0f;
	float cooldownRemaining_ = 0.0f;
	int currentStepIndex_ = -1;
	bool isAttacking_ = false;
	bool isCurrentStepActive_ = false;
	bool lastHitSuccess_ = false;
	std::vector<bool> stepHitDone_;
};
