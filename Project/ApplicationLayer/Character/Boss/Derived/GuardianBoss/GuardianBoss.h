#pragma once
#include "BaseTypes/HumanoidBossBase.h"
#include "BehaviorTree/IBTNode.h"
#include "ApplicationLayer/Character/Enemy/Navigation/EnemyAStarNavigator.h"

#include <memory>
#include <string>
#include <vector>

/// ----------------------------------------------------------------
///						ガーディアンボス
/// ----------------------------------------------------------------
class GuardianBoss : public HumanoidBossBase
{
public:
	enum class BossTargetType
	{
		None,
		Player,
		DummyTarget,
	};

private:

	struct BossTargetState
	{
		bool hasTarget = false;
		bool isTargetVisible = false;
		bool isInMeleeRange = false;
		bool isInMidRange = false;
		bool isInFarRange = false;
		float directDistance = 0.0f;
		float pathDistance = 0.0f;
		K4E::Vector3 position{};
		K4E::Vector3 direction{ 0.0f, 0.0f, 1.0f };
		BossTargetType type = BossTargetType::None;
		std::string targetName = "None";
	};

	struct PlainsBossPhaseSettings
	{
		float phase2HpRate = 0.5f;
		float phaseChangeDuration = 2.0f;
		float phase2MoveSpeedMultiplier = 1.25f;
		float phase2CooldownMultiplier = 0.7f;
		float phase2AttackRangeMultiplier = 1.15f;
	};

	struct PlainsBossAttackSettings
	{
		bool enableMelee = true;
		bool enableStepAttack = true;
		bool enableCharge = true;
		bool enableShockwave = true;
		bool enableFarShot = true;
		float meleeRange = 4.5f;
		float midRange = 12.0f;
		float farRange = 24.0f;
		float attackCooldown = 1.8f;
		float stepAttackCooldown = 2.4f;
		float chargeCooldown = 3.5f;
		float shockwaveCooldown = 4.0f;
		float farShotCooldown = 3.0f;
		float stepAttackDistance = 5.0f;
		float stepAttackSpeed = 10.0f;
		float stepAttackRadius = 5.4f;
		float stepAttackDuration = 0.45f;
		float stepAttackRecovery = 0.35f;
		float chargeSpeed = 12.0f;
		float chargeDuration = 0.8f;
		float shockwaveWindup = 0.55f;
		float shockwaveRadius = 8.0f;
		float shockwaveDuration = 0.85f;
		float farShotSpeed = 16.0f;
		float farShotLifetime = 1.2f;
	};

	struct PlainsBossRuntimeState
	{
		bool isPhase2 = false;
		bool isChangingPhase = false;
		bool isMoving = false;
		float attackCooldownTimer = 0.0f;
		float stepAttackCooldownTimer = 0.0f;
		float chargeCooldownTimer = 0.0f;
		float shockwaveCooldownTimer = 0.0f;
		float farShotCooldownTimer = 0.0f;
		float actionTimer = 0.0f;
		float walkAnimTimer = 0.0f;
		std::string currentActionName = "Idle";
		K4E::Vector3 lastMoveDirection{ 0.0f, 0.0f, 1.0f };
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
		float targetRepathThreshold = 0.5f;
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
		int sourceObstacleAABBCount = 0;
		int blockingObstacleAABBCount = 0;
		bool lineBlocked = false;
		int blockedWaypointIndex = -1;
		std::string blockedObstacleName = "None";
		K4E::Vector3 lastStuckCheckPosition{};
		K4E::Vector3 lastPathTargetPos{};
		K4E::Vector3 currentWaypoint{};
		K4E::Vector3 currentMoveGoal{};
		bool hasReachableApproachPoint = false;
		std::string approachPointReason = "None";
		float currentPathDistance = 0.0f;
		float bestCandidatePathDistance = 0.0f;
		bool usedLocalAvoidance = false;
		std::string lastAvoidanceReason = "None";
		K4E::Vector3 blockedSegmentFrom{};
		K4E::Vector3 blockedSegmentTo{};
	};

	struct BossTraversalSettings
	{
		bool enabled = true;
		bool allowJumpOverLowObstacles = true;
		float minJumpObstacleHeight = 0.15f;
		float maxJumpObstacleHeight = 2.2f;
		float jumpTriggerDistance = 2.2f;
		float jumpVerticalVelocity = 11.0f;
		float jumpForwardSpeed = 5.5f;
		float jumpCooldown = 1.0f;
		float gravity = 24.0f;
		float groundY = 0.0f;
	};

	struct BossTraversalState
	{
		bool hasJumpCandidate = false;
		bool isJumpingObstacle = false;
		float jumpCooldownTimer = 0.0f;
		float currentVerticalVelocity = 0.0f;
		int selectedObstacleIndex = -1;
		float selectedObstacleHeight = 0.0f;
		K4E::AABB selectedObstacleAABB{};
		K4E::Vector3 jumpDirection{ 0.0f, 0.0f, 1.0f };
		std::string lastTraversalReason = "None";
	};

public:
	void SetupBoss() override;
	void SetFloorAABBs(const std::vector<K4E::AABB>* aabbs) { floorAABBs_ = aabbs; }
	void SetWallObstacleAABBs(const std::vector<K4E::AABB>* aabbs) { wallObstacleAABBs_ = aabbs; }
	void SetDebugDummyTarget(const K4E::Vector3& position, bool exists);
	void SetPreferredTargetType(BossTargetType type) { preferredTargetType_ = type; }
	BossTargetType GetCurrentTargetType() const { return targetState_.type; }
	void OnDamaged(float damage) override;
	void OnDead() override;
	void OnCollision(K4E::Collider* other) override;
	void Draw() override;
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
	void UpdateTargetState(float deltaTime);
	float CalculateCurrentPathDistance() const;
	bool FindReachableApproachPointNearTarget(K4E::Vector3& outPoint);
	bool IsPointInsideNavigationObstacle(const K4E::Vector3& point) const;
	bool TryLocalAvoidanceMove(float deltaTime);
	bool FindJumpableObstacleAhead();
	bool IsObstacleJumpable(const K4E::AABB& obstacle, int index);
	bool TryJumpOverObstacle(float deltaTime);
	void UpdateObstacleJump(float deltaTime);
	void UpdateCooldownTimers(float deltaTime);
	float GetCooldownMultiplier() const;
	float GetRangeMultiplier() const;
	void UpdateNavigationObstacleList();
	bool MoveAlongPath(float deltaTime);
	void StopMove();
	void UpdateStuckState(float deltaTime);
	bool ResolveObstaclePenetrationXZ(float deltaTime);
	bool TryLandOnObstacleTop(float deltaTime);
	void ChangeBossState(BossState newState);
	void EnterIdle();
	void EnterMove();
	void EnterAttack(const char* actionName);
	BTNodeResult TickBehaviorTree(float deltaTime);
	void BuildBehaviorTree();
	BTNodeResult TickContinueCurrentAction(float deltaTime);
	BTNodeResult TickMeleeAttack(float deltaTime);
	BTNodeResult TickStepAttack(float deltaTime);
	void StartStepAttack();
	void UpdateStepAttack(float deltaTime);
	bool CanUseStepAttack() const;
	BTNodeResult TickShockwaveAttack(float deltaTime);
	void StartShockwaveAttack();
	void UpdateShockwaveAttack(float deltaTime);
	bool CanUseShockwave() const;
	BTNodeResult TickFarShotAttack(float deltaTime);
	void StartFarShotAttack();
	void UpdateFarShotAttack(float deltaTime);
	bool CanUseFarShot() const;
	BTNodeResult TickPhase2Transition(float deltaTime);
	bool ShouldEnterPhase2() const;
	void StartPhase2();
	void UpdatePhase2Transition(float deltaTime);
	BTNodeResult TickChaseTarget(float deltaTime);
	float GetCurrentMoveSpeed() const;

	PlainsBossPhaseSettings phaseSettings_{};
	PlainsBossAttackSettings attackSettings_{};
	PlainsBossRuntimeState runtime_{};
	BossTargetState targetState_{};
	BossTargetType preferredTargetType_ = BossTargetType::DummyTarget;
	bool hasDebugDummyTarget_ = false;
	K4E::Vector3 debugDummyTargetPosition_{};
	float rotateSpeed_ = 5.5f;
	float chargeTimer_ = 0.0f;
	K4E::Vector3 stepAttackDirection_{ 0.0f, 0.0f, 1.0f };
	K4E::Vector3 stepAttackStartPosition_{};
	K4E::Vector3 shockwaveDirection_{ 0.0f, 0.0f, 1.0f };
	K4E::Vector3 shockwaveOrigin_{};
	K4E::Vector3 farShotDirection_{ 0.0f, 0.0f, 1.0f };
	K4E::Vector3 farShotOrigin_{};
	float phaseTransitionTimer_ = 0.0f;

	std::unique_ptr<IBTNode> behaviorRoot_;
	PathSettings pathSettings_{};
	PathState pathState_{};
	BossTraversalSettings traversalSettings_{};
	BossTraversalState traversalState_{};
	EnemyAStarNavigator navigator_{};
	const std::vector<K4E::AABB>* floorAABBs_ = nullptr;
	const std::vector<K4E::AABB>* wallObstacleAABBs_ = nullptr;
	std::vector<K4E::AABB> pathBlockingObstacleAABBs_{};
	std::vector<K4E::AABB> climbableObstacleAABBs_{};
	std::vector<K4E::Vector3> approachPointCandidates_{};
	K4E::Vector3 lastLocalAvoidanceDirection_{};
	bool hasLastLocalAvoidanceDirection_ = false;
	bool pathDebugDrawEnabled_ = true;
	bool obstacleDebugDrawEnabled_ = true;
	float stuckTimer_ = 0.0f;
	bool isStuck_ = false;
	K4E::Vector3 movementVelocity_{};
};
