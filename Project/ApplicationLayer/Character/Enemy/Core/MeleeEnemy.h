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
	bool IsAttackCooldownReady() const;
	bool IsDeadCondition() const;
	float GetDistanceToTarget() const;
	K4E::Vector3 GetTargetPosition() const;
	void FaceToTarget();

	void DeadAction();
	void MeleeAttackAction();
	void ChaseTargetAction();
	void WanderAction(float deltaTime);
	void EvaluateBehavior(float deltaTime);

private:
	K4E::Collider* target_ = nullptr;

	float detectRange_ = 18.0f;
	float meleeAttackRange_ = 2.8f;
	float moveSpeed_ = 3.2f;
	MeleeAttackType selectedAttackType_ = MeleeAttackType::Scratch;
	MeleeAttackController attackController_{};
	float wanderTimer_ = 0.0f;
	K4E::Vector3 wanderDirection_{ 1.0f, 0.0f, 0.0f };

	AnimState animState_ = AnimState::Idle;
	const char* currentBehaviorName_ = "None";
};
