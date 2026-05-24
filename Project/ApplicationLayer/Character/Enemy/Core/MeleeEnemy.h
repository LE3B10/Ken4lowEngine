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
	K4E::Vector3 GetAttackForward() const { return attackForward_; }
	void ApplyAttackMove(const K4E::Vector3& horizontalVelocity);
	void NotifyAttackHit(int damage, const K4E::Vector3& forward);
	void ForceAttack(MeleeAttackType type);
	void StopAttack();
	void ResetAttackCooldown();
	void SetSelectedAttackType(MeleeAttackType type) { selectedAttackType_ = type; }
	MeleeAttackType GetSelectedAttackType() const { return selectedAttackType_; }
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
	K4E::Collider* target_ = nullptr;

	float detectRange_ = 18.0f;
	float meleeAttackRange_ = 2.8f;
	float moveSpeed_ = 3.2f;
	float stopDistance_ = 1.8f;
	float attackStartRange_ = 2.4f;
	float resumeChaseDistance_ = 2.8f;
	float minOneTwoForwardDistance_ = 1.6f;
	float rotateSpeed_ = 8.0f;
	float visualYawOffset_ = 0.0f;
	float walkAnimSpeed_ = 8.0f;
	float walkArmSwing_ = 0.55f;
	float walkLegSwing_ = 0.45f;
	float attackArmSwing_ = 1.25f;
	float attackReturnSpeed_ = 12.0f;
	float attackBodyLean_ = 0.15f;
	float walkAnimTime_ = 0.0f;
	K4E::Vector3 lastSafePosition_{};
	K4E::Vector3 stageBoundsMin_{ -1000.0f, -1000.0f, -1000.0f };
	K4E::Vector3 stageBoundsMax_{ 1000.0f, 1000.0f, 1000.0f };
	bool hasStageBounds_ = false;
	bool isOutsideStage_ = false;
	K4E::Vector3 lastResolvePush_{};
	bool isCollidingWithStage_ = false;
	std::string lastStageCollisionType_ = "None";
	std::string lastStageCollisionName_ = "None";
	bool blockedByObstacle_ = false;
	std::string lastBlockedObstacleName_ = "None";
	int usingWorldAABBCount_ = 0;
	int usingObstacleAABBCount_ = 0;
	bool collisionManagerRegistered_ = false;
	int lastCollisionCount_ = 0;
	bool pushedThisFrame_ = false;
	bool restoredToSafePosition_ = false;
	bool isOverlappingWallObstacle_ = false;
	bool isOnFloor_ = false;
	std::string lastWallObstacleName_ = "None";
	K4E::Vector3 lastWallResolvePush_{};
	float maxResolvePushPerFrame_ = 0.75f;
	float maxHorizontalPushPerFrame_ = 0.45f;
	float stuckCheckTime_ = 0.8f;
	float stuckDistance_ = 0.2f;
	bool pathFindEnabled_ = true;
	float repathInterval_ = 0.25f;
	float waypointReachDistance_ = 0.6f;
	float pathGridSize_ = 1.5f;
	float pathSearchRadius_ = 28.0f;
	float obstacleExpandRadius_ = 0.6f;
	MeleeAttackType selectedAttackType_ = MeleeAttackType::Scratch;
	MeleeAttackController attackController_{};
	float attackLockTimer_ = 0.0f;
	float attackLockTime_ = 0.18f;
	bool isStuck_ = false;
	bool shouldChase_ = true;
	bool pathFound_ = false;
	std::string pathFailureReason_ = "None";
	float stuckTimer_ = 0.0f;
	float lastRepathTimer_ = 0.0f;
	float pathRetryTimer_ = 0.0f;
	float targetMovedDistanceForRepath_ = 0.0f;
	float lastMovedDistance_ = 0.0f;
	float targetRepathThreshold_ = 1.2f;
	float stuckRepathExpandBonus_ = 0.25f;
	float maxStuckRepathExpandBonus_ = 1.0f;
	std::string lastRepathReason_ = "None";
	std::string blockedObstacleName_ = "None";
	K4E::Vector3 lastStuckCheckPosition_{};
	K4E::Vector3 lastPathTargetPos_{};
	K4E::Vector3 spawnPosition_{};
	K4E::Vector3 currentPathWaypoint_{};
	K4E::Vector3 blockedSegmentFrom_{};
	K4E::Vector3 blockedSegmentTo_{};
	int blockedWaypointIndex_ = -1;
	bool lineBlocked_ = false;
	EnemyAStarNavigator navigator_{};
	float wanderTimer_ = 0.0f;
	K4E::Vector3 wanderDirection_{ 1.0f, 0.0f, 0.0f };
	float rawYaw_ = 0.0f;
	float finalVisualYaw_ = 0.0f;
	float visualYawOffsetDeg_ = 0.0f;
	float debugCurrentYaw_ = 0.0f;
	float debugTargetYaw_ = 0.0f;
	float debugDeltaYaw_ = 0.0f;
	float debugNormalizedDeltaYaw_ = 0.0f;
	K4E::Vector3 facingDirection_{ 0.0f, 0.0f, 1.0f };
	K4E::Vector3 movementDirection_{ 0.0f, 0.0f, 1.0f };
	K4E::Vector3 targetDirection_{ 0.0f, 0.0f, 1.0f };
	K4E::Vector3 visualForward_{ 0.0f, 0.0f, 1.0f };
	K4E::Vector3 attackForward_{ 0.0f, 0.0f, 1.0f };

	AnimState animState_ = AnimState::Idle;
	const std::vector<K4E::AABB>* floorAABBs_ = nullptr;
	const std::vector<K4E::AABB>* wallObstacleAABBs_ = nullptr;
	const char* currentBehaviorName_ = "None";
};
