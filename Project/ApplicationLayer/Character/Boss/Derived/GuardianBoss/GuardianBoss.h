#pragma once
#include "BaseTypes/HumanoidBossBase.h"
#include "BehaviorTree/IBTNode.h"
#include "ApplicationLayer/Character/Enemy/Navigation/EnemyAStarNavigator.h"
#include "Math/MathUtils.h"

#include <memory>
#include <string>
#include <vector>

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

	struct PathSettings
	{
		bool enabled = true;
		float repathInterval = 0.25f;
		float waypointReachDistance = 1.2f;
		float gridSize = 1.5f;
		float searchRadius = 35.0f;
		float obstacleExpandRadius = 1.4f;
		float temporaryBlockDuration = 1.5f;
		float temporaryBlockRadius = 1.5f;
		bool cornerCuttingDisabled = true;
		float targetRepathThreshold = 1.5f;
		float stuckRepathExpandBonus = 0.35f;
		float maxStuckRepathExpandBonus = 1.5f;
	};

	struct PathState
	{
		bool found = false;
		std::string failureReason = "None";
		float failedWaitTimer = 0.0f;
		float lastRepathTimer = 0.0f;
		float retryTimer = 0.0f;
		float targetMovedDistanceForRepath = 0.0f;
		float lastMovedDistance = 0.0f;
		std::string lastRepathReason = "None";
		std::string blockedObstacleName = "None";
		K4E::Vector3 lastStuckCheckPosition{};
		K4E::Vector3 lastPathTargetPos{};
		K4E::Vector3 currentWaypoint{};
		K4E::Vector3 blockedSegmentFrom{};
		K4E::Vector3 blockedSegmentTo{};
		int blockedWaypointIndex = -1;
		bool lineBlocked = false;
	};

public:
	void SetupBoss() override;
	void SetFloorAABBs(const std::vector<K4E::AABB>* aabbs) { floorAABBs_ = aabbs; }
	void SetWallObstacleAABBs(const std::vector<K4E::AABB>* aabbs) { wallObstacleAABBs_ = aabbs; }
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
	bool MoveAlongPath(float deltaTime);
	void UpdateStuckState(float deltaTime);
	bool ResolveObstaclePenetrationXZ(float deltaTime);
	bool TryLandOnObstacleTop(float deltaTime);
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
	PathSettings pathSettings_{};
	PathState pathState_{};
	EnemyAStarNavigator navigator_{};
	const std::vector<K4E::AABB>* floorAABBs_ = nullptr;
	const std::vector<K4E::AABB>* wallObstacleAABBs_ = nullptr;
	std::vector<K4E::AABB> pathBlockingObstacleAABBs_{};
	std::vector<K4E::AABB> climbableObstacleAABBs_{};
	float stuckTimer_ = 0.0f;
	bool isStuck_ = false;
};
