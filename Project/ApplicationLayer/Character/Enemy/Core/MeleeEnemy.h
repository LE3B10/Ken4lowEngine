#pragma once

#include "EnemyBase.h"

namespace K4E = ::Ken4lowEngine;

class MeleeEnemy final : public EnemyBase
{
public:
	void Initialize() override;
	void Update(float deltaTime) override;
	void Draw() override;
	void DrawImGui() override;

	void SetTarget(K4E::Collider* target) { target_ = target; }

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
	float attackCooldown_ = 1.2f;
	int attackDamage_ = 10;
	float attackLockTime_ = 0.45f;

	float attackCooldownTimer_ = 0.0f;
	float attackLockTimer_ = 0.0f;
	float wanderTimer_ = 0.0f;
	K4E::Vector3 wanderDirection_{ 1.0f, 0.0f, 0.0f };

	AnimState animState_ = AnimState::Idle;
	const char* currentBehaviorName_ = "None";
};
