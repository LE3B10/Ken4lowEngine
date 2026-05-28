#pragma once
#include "BaseTypes/HumanoidBossBase.h"
#include "BehaviorTree/IBTNode.h"

#include <memory>
#include <string>

/// ----------------------------------------------------------------
///						ガーディアンボス
/// ----------------------------------------------------------------
class GuardianBoss : public HumanoidBossBase
{
private:

	struct PlainsBossPhaseSettings
	{
		float phase2HpRate = 0.5f;
		float phase2MoveSpeedMultiplier = 1.25f;
		float phase2CooldownMultiplier = 0.65f;
	};

	struct PlainsBossAttackSettings
	{
		float meleeRange = 4.5f;
		float chargeRange = 11.5f;
		float shockwaveRange = 21.0f;
		float moveStartDistance = 6.0f;
		float moveStopDistance = 3.5f;
		float attackCooldown = 1.8f;
		float chargeCooldown = 2.8f;
		float shockwaveCooldown = 3.4f;
	};

	struct PlainsBossRuntimeState
	{
		bool isPhase2 = false;
		float attackCooldownTimer = 0.0f;
		float stateTimer = 0.0f;
		std::string currentActionName = "Idle";
	};

public:
	void SetupBoss() override;
	void OnDamaged(float damage) override;
	void OnDead() override;
	void OnCollision(K4E::Collider* other) override;
	void DrawImGui() override;

protected:
	void UpdateState(float deltaTime) override;
	void UpdateMovement(float deltaTime) override;
	void UpdateAttack(float deltaTime) override;
	void CheckDeath() override;
	void SetupAttacks() override;
	void SetupPhaseData() override;
	void SetupWeakPoints() override;

private:
	void FaceTarget(float deltaTime);
	void ChangeBossState(BossState newState);
	void EnterIdle();
	void EnterMove();
	void EnterAttack(const char* actionName);
	void UpdatePhaseTransition();
	bool StartAttackByDistance();
	BTNodeResult TickBehaviorTree(float deltaTime);
	void BuildBehaviorTree();
	float GetCurrentMoveSpeed() const;

	PlainsBossPhaseSettings phaseSettings_{};
	PlainsBossAttackSettings attackSettings_{};
	PlainsBossRuntimeState runtime_{};
	float rotateSpeed_ = 5.5f;
	float chargeTimer_ = 0.0f;

	std::unique_ptr<IBTNode> behaviorRoot_;
};
