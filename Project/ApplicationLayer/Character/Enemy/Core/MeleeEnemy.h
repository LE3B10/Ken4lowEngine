#pragma once

#include "EnemyBase.h"
#include "../AI/MeleeAttackController.h"

namespace K4E = ::Ken4lowEngine;

class MeleeEnemy final : public EnemyBase
{
public:
	void Initialize() override;
	void Update(float deltaTime) override;
	void Draw() override;
	void DrawImGui() override;

	void SetTarget(K4E::Collider* target) { target_ = target; }

	K4E::Collider* GetTargetCollider() const { return target_; }
	K4E::Vector3 GetTargetPositionForAttack() const;
	void ApplyAttackMove(const K4E::Vector3& horizontalVelocity);
	void NotifyAttackHit(int damage, const K4E::Vector3& forward);

private:
	enum class AnimState
	{
		Idle,
		Move,
		Attack,
		Dead,
	};

	bool HasTarget() const;
	bool IsTargetInDetectRange() const;
	bool IsTargetInMeleeRange() const;
	bool IsTargetInAttackStartRange() const;
	bool IsTargetInAttackHoldRange() const;
	bool IsAttackCooldownReady() const;
	bool IsDeadCondition() const;
	float GetDistanceToTarget() const;
	K4E::Vector3 GetTargetPosition() const;
	void FaceToTarget();
	void StopMove();
	bool IsMoveResumeDistanceReached() const;

	void DeadAction();
	void MeleeAttackAction();
	void CombatIdleAction();
	void ChaseTargetAction();
	void WanderAction(float deltaTime);
	void EvaluateBehavior(float deltaTime);

private:
	K4E::Collider* target_ = nullptr;

	float detectRange_ = 18.0f;
	float meleeAttackRange_ = 2.8f;
	float moveSpeed_ = 3.2f;
	float stopDistance_ = 1.8f;
	float attackStartRange_ = 2.4f;
	float resumeChaseDistance_ = 2.8f;
	float minOneTwoForwardDistance_ = 1.6f;
	MeleeAttackType selectedAttackType_ = MeleeAttackType::Scratch;
	MeleeAttackController attackController_{};
	float attackLockTimer_ = 0.0f;
	float attackLockTime_ = 0.18f;
	bool isStuck_ = false;
	bool shouldChase_ = true;
	float wanderTimer_ = 0.0f;
	K4E::Vector3 wanderDirection_{ 1.0f, 0.0f, 0.0f };

	AnimState animState_ = AnimState::Idle;
	const char* currentBehaviorName_ = "None";
};
