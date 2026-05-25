#pragma once

#include "EnemyBase.h"
#include "../AI/MeleeAttackController.h"
#include "../Navigation/EnemyAStarNavigator.h"
#include <string>

namespace K4E = ::Ken4lowEngine;

class MeleeEnemy final : public EnemyBase
{
public:
	void Initialize() override;
	void Update(float deltaTime) override;
	void Draw() override;
	void DrawImGui() override;
	void OnCollisionEnter(K4E::Collider* other) override;
	void OnCollisionStay(K4E::Collider* other) override;

	void SetTarget(K4E::Collider* target) { target_ = target; }
	void SetFloorAABBs(const std::vector<K4E::AABB>* aabbs) { floorAABBs_ = aabbs; }
	void SetWallObstacleAABBs(const std::vector<K4E::AABB>* aabbs) { wallObstacleAABBs_ = aabbs; }

	K4E::Collider* GetTargetCollider() const { return target_; }
	K4E::Vector3 GetTargetPositionForAttack() const;
	K4E::Vector3 GetAttackForward() const { return animationState_.attackForward; }
	void ApplyAttackMove(const K4E::Vector3& horizontalVelocity);
	void NotifyAttackHit(int damage, const K4E::Vector3& forward);
	void ForceAttack(MeleeAttackType type);
	void StopAttack();
	void ResetAttackCooldown();
	void SetSelectedAttackType(MeleeAttackType type) { attackSettings_.selectedAttackType = type; }
	MeleeAttackType GetSelectedAttackType() const { return attackSettings_.selectedAttackType; }
	bool IsAttacking() const { return attackController_.IsAttacking(); }
	const char* GetCurrentAttackName() const { return attackController_.GetCurrentAttackName(); }

private:
	enum class AnimState
	{
		Idle,
		Walk,
		Scratch,
		OneTwo,
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
	void FaceToTarget(float deltaTime);
	void FaceToMoveDirection(float deltaTime);
	void ApplyVisualYawFromDirection(const K4E::Vector3& direction, float deltaTime);
	void StopMove();
	bool IsMoveResumeDistanceReached() const;
	bool MoveAlongPath(float deltaTime);
	void TryJumpToTarget(float deltaTime);
	float CalculateJumpVelocityForHeight(float heightDelta) const;

	bool ResolveObstaclePenetrationXZ(float deltaTime);
	void UpdateStuckState(float deltaTime);

	void DeadAction();
	void MeleeAttackAction();
	void CombatIdleAction();
	void ChaseTargetAction();
	void WanderAction(float deltaTime);
	void EvaluateBehavior(float deltaTime);
	void UpdateVisualAnimation(float deltaTime);
	bool IsInsideStageBounds(const K4E::Vector3& position) const;
	const char* GetAnimStateName() const;

private:
	struct DetectionSettings
	{
		float detectRange = 18.0f;
		float meleeAttackRange = 2.8f;
		float stopDistance = 1.8f;
		float attackStartRange = 2.4f;
		float resumeChaseDistance = 2.8f;
		float minOneTwoForwardDistance = 1.6f;
	};

	struct MoveSettings { float moveSpeed = 3.2f; float rotateSpeed = 8.0f; float maxResolvePushPerFrame = 0.75f; float maxHorizontalPushPerFrame = 0.45f; };
	struct JumpSettings { bool enabled = true; float baseVelocity = 12.0f; float extraBoost = 0.8f; float gravityEstimate = 20.0f; float maxVelocity = 18.0f; float targetHeightThreshold = 1.0f; float horizontalDistanceMax = 8.0f; float cooldown = 0.9f; };
	struct JumpState { float cooldownTimer = 0.0f; float targetHeightDelta = 0.0f; float calculatedVelocity = 0.0f; float appliedVelocity = 0.0f; std::string lastReason = "None"; };
	struct PathSettings { bool enabled = true; float repathInterval = 0.25f; float waypointReachDistance = 0.85f; float gridSize = 1.5f; float searchRadius = 28.0f; float obstacleExpandRadius = 0.9f; float temporaryBlockDuration = 1.5f; float temporaryBlockRadius = 1.0f; bool cornerCuttingDisabled = true; float targetRepathThreshold = 1.2f; float stuckRepathExpandBonus = 0.25f; float maxStuckRepathExpandBonus = 1.0f; };
	struct PathState { bool found = false; std::string failureReason = "None"; float failedWaitTimer = 0.0f; float lastRepathTimer = 0.0f; float retryTimer = 0.0f; float targetMovedDistanceForRepath = 0.0f; float lastMovedDistance = 0.0f; std::string lastRepathReason = "None"; std::string blockedObstacleName = "None"; K4E::Vector3 lastStuckCheckPosition{}; K4E::Vector3 lastPathTargetPos{}; K4E::Vector3 currentWaypoint{}; K4E::Vector3 blockedSegmentFrom{}; K4E::Vector3 blockedSegmentTo{}; int blockedWaypointIndex = -1; bool lineBlocked = false; };
	struct StuckSettings { float checkTime = 0.8f; float distance = 0.2f; float moveThreshold = 0.18f; };
	struct StuckState { bool isStuck = false; float timer = 0.0f; };
	struct AttackSettings { MeleeAttackType selectedAttackType = MeleeAttackType::Scratch; float lockTime = 0.18f; };
	struct AttackState { float lockTimer = 0.0f; bool shouldChase = true; };
	struct AnimationSettings { float visualYawOffset = 0.0f; float walkAnimSpeed = 8.0f; float walkArmSwing = 0.55f; float walkLegSwing = 0.45f; float attackArmSwing = 1.25f; float attackReturnSpeed = 12.0f; float attackBodyLean = 0.15f; };
	struct AnimationStateData { float walkAnimTime = 0.0f; float rawYaw = 0.0f; float finalVisualYaw = 0.0f; float visualYawOffsetDeg = 0.0f; float debugCurrentYaw = 0.0f; float debugTargetYaw = 0.0f; float debugDeltaYaw = 0.0f; float debugNormalizedDeltaYaw = 0.0f; K4E::Vector3 facingDirection{0.0f,0.0f,1.0f}; K4E::Vector3 movementDirection{0.0f,0.0f,1.0f}; K4E::Vector3 targetDirection{0.0f,0.0f,1.0f}; K4E::Vector3 visualForward{0.0f,0.0f,1.0f}; K4E::Vector3 attackForward{0.0f,0.0f,1.0f}; AnimState animState = AnimState::Idle; };
	struct CollisionState { K4E::Vector3 lastSafePosition{}; K4E::Vector3 stageBoundsMin{-1000.0f,-1000.0f,-1000.0f}; K4E::Vector3 stageBoundsMax{1000.0f,1000.0f,1000.0f}; bool hasStageBounds = false; bool isOutsideStage = false; K4E::Vector3 lastResolvePush{}; bool isCollidingWithStage = false; std::string lastStageCollisionType = "None"; std::string lastStageCollisionName = "None"; bool blockedByObstacle = false; std::string lastBlockedObstacleName = "None"; int usingWorldAABBCount = 0; int usingObstacleAABBCount = 0; bool collisionManagerRegistered = false; int lastCollisionCount = 0; bool pushedThisFrame = false; bool restoredToSafePosition = false; bool isOverlappingWallObstacle = false; bool isOnFloor = false; std::string lastWallObstacleName = "None"; K4E::Vector3 lastWallResolvePush{}; };
	struct WanderState { float timer = 0.0f; K4E::Vector3 direction{1.0f,0.0f,0.0f}; };

	K4E::Collider* target_ = nullptr;
	DetectionSettings detection_{};
	MoveSettings move_{};
	JumpSettings jump_{};
	JumpState jumpState_{};
	PathSettings pathSettings_{};
	PathState pathState_{};
	StuckSettings stuckSettings_{};
	StuckState stuck_{};
	AttackSettings attackSettings_{};
	AttackState attackState_{};
	AnimationSettings animation_{};
	AnimationStateData animationState_{};
	CollisionState collision_{};
	WanderState wander_{};
	MeleeAttackController attackController_{};
	EnemyAStarNavigator navigator_{};
	K4E::Vector3 spawnPosition_{};
	const std::vector<K4E::AABB>* floorAABBs_ = nullptr;
	const std::vector<K4E::AABB>* wallObstacleAABBs_ = nullptr;
	const char* currentBehaviorName_ = "None";
};
