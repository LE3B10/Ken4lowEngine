#pragma once

#include "EnemyBase.h"
#include "ApplicationLayer/Character/Enemy/Navigation/EnemyAStarNavigator.h"
#include "ApplicationLayer/Character/Enemy/Projectile/MidRangeBombProjectile.h"
#include "ApplicationLayer/Character/Enemy/Effects/MidRangeBombEffectController.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace K4E = ::Ken4lowEngine;

class MidRangeEnemy final : public EnemyBase
{
private:
    enum class AnimState
    {
        Idle,
        Walk,
        Cast,
        Throw,
        Dead,
    };

    struct BasicStatsSettings
    {
        int maxHp = 120;
        bool resetHpOnLoad = true;
    };

    struct DistanceSettings
    {
        float detectRange = 24.0f;
        float attackMinRange = 5.0f;
        float attackMaxRange = 14.0f;
        float idealRange = 9.0f;
        float tooCloseRange = 4.0f;
    };

    struct MoveSettings
    {
        float moveSpeed = 2.6f;
        float retreatSpeed = 2.8f;
        float rotateSpeed = 8.0f;
    };

    struct BombAttackSettings
    {
        float cooldown = 2.0f;
        float castTime = 0.45f;
        float throwHeightOffset = 1.6f;
    };

    struct PathSettings
    {
        bool pathFindEnabled = true;
        float repathInterval = 0.25f;
        float waypointReachDistance = 0.85f;
        float pathGridSize = 1.5f;
        float pathSearchRadius = 28.0f;
        float obstacleExpandRadius = 0.9f;
        bool cornerCuttingDisabled = true;
    };

    struct PathState
    {
        bool pathFound = false;
        K4E::Vector3 currentWaypoint{};
        std::string lastRepathReason = "None";
        std::string failureReason = "None";
        bool lineBlocked = false;
    };

    struct AnimationSettings
    {
        float walkSwingSpeed = 7.0f;
        float walkArmAmplitude = 0.35f;
        float walkLegAmplitude = 0.25f;
        float castArmPitch = -0.8f;
        float castArmYaw = 0.2f;
        float throwArmPitch = 1.0f;
        float bodyCastLean = -0.08f;
        float throwBodyLean = 0.12f;
        float returnSpeed = 10.0f;
    };

    struct HeadLookSettings
    {
        bool enabled = true;
        float yawLimitDeg = 120.0f;
        float pitchMinDeg = -80.0f;
        float pitchMaxDeg = 80.0f;
        float pitchSign = -1.0f;
        float lerpSpeed = 12.0f;
    };
    struct HitReactionSettings
    {
        bool enabled = true;
        float duration = 0.18f;
        float knockbackPower = 2.0f;
        float knockbackUpPower = 0.4f;
        float bodyLean = -0.18f;
        float flashDuration = 0.12f;
        bool interruptAttack = false;
        bool stopBehaviorWhileActive = true;
    };
    struct HitReactionState
    {
        bool active = false;
        float timer = 0.0f;
        K4E::Vector3 knockbackDirection{};
        std::string lastReason = "None";
    };
    struct DeathAnimationSettings
    {
        bool enabled = true;
        float duration = 1.2f;
        float fallRotateX = 1.35f;
        float sinkDistance = 0.4f;
        float fadeDelay = 0.4f;
        float fadeDuration = 0.6f;
        bool disableCollisionOnDeath = true;
        bool stopMoveOnDeath = true;
    };
    struct DeathAnimationState
    {
        bool active = false;
        float timer = 0.0f;
        K4E::Vector3 startPosition{};
        K4E::Vector3 startRotation{};
        std::string lastReason = "None";
    };

    struct BombAttackState
    {
        float cooldownTimer = 0.0f;
        float castTimer = 0.0f;
        float throwAnimTimer = 0.0f;
        float throwAnimDuration = 0.25f;
        bool casting = false;
        bool thrownThisCast = false;
        std::string lastReason = "None";
    };
    struct SuicideBombSettings
    {
        bool enabled = true;
        float triggerHpRate = 0.30f;
        bool invincibleWhileActive = true;
        float timeLimit = 5.0f;
        float chaseSpeed = 5.0f;
        float rotateSpeed = 12.0f;
        float explodeDistance = 1.8f;
        float explosionRadius = 4.0f;
        int explosionDamage = 60;
        float explosionDebugDrawTime = 0.35f;
        bool stopNormalBombAttack = true;
        bool blinkEnabled = true;
        float blinkSpeed = 10.0f;
        bool delayDeathAnimationUntilExplosion = true;
        float explosionPositionMinY = 0.5f;
        float deathDelayAfterExplosion = 0.05f;
        float breakApartPower = 2.5f;
        float breakApartUpPower = 3.0f;
        bool useTargetDirectionForBreakApart = true;
        K4E::Vector4 blinkColorA{ 1.0f, 0.1f, 0.1f, 1.0f };
        K4E::Vector4 blinkColorB{ 1.0f, 1.0f, 0.1f, 1.0f };
    };
    struct SuicideBombState
    {
        bool active = false;
        bool exploded = false;
        float timer = 0.0f;
        float explosionDrawTimer = 0.0f;
        float deathDelayTimer = 0.0f;
        float blinkTimer = 0.0f;
        K4E::Vector3 explosionPosition{};
        std::string lastReason = "None";
    };

    struct TargetState
    {
        bool hasTarget = false;
        bool inDetectRange = false;
        bool inAttackRange = false;
        bool tooClose = false;
        float distance = 0.0f;
        K4E::Vector3 position{};
        K4E::Vector3 direction{};
    };

    struct AnimationStateData
    {
        float walkAnimTime = 0.0f;
        float attackAnimTime = 0.0f;
        float attackProgress = 0.0f;
        float currentYaw = 0.0f;
        float targetYaw = 0.0f;
        K4E::Vector3 moveDirection{ 0.0f, 0.0f, 1.0f };
        K4E::Vector3 faceDirection{ 0.0f, 0.0f, 1.0f };
        AnimState animState = AnimState::Idle;
    };

    struct HeadLookState
    {
        float currentYaw = 0.0f;
        float currentPitch = 0.0f;
        float targetYaw = 0.0f;
        float targetPitch = 0.0f;
        bool targetVisible = false;
        bool enabledCondition = false;
        bool hasTargetCondition = false;
        bool inDetectCondition = false;
        bool deathCondition = false;
        K4E::Vector3 toTarget{};
        float horizontalDistance = 0.0f;
        std::string reason = "Disabled";
    };

    struct TuningIoState
    {
        std::filesystem::path jsonPath = "Resources/DataAssets/Enemy/MidRangeEnemy/MidRangeEnemy_Normal.json";
        std::string lastLoadResult = "未読み込み";
        std::string lastSaveResult = "未保存";
    };

    struct BehaviorState
    {
        std::string currentBehaviorName = "None";
        std::string lastReason = "None";
    };

public:
    void Initialize() override;
    void Update(float deltaTime) override;
    void Draw() override;
    void DrawImGui() override;
    void SetTarget(const K4E::Vector3& target);

    void SetFloorAABBs(const std::vector<K4E::AABB>* aabbs)
    {
        // 追加: 床AABBを外部から受け取る。
        floorAABBs_ = aabbs;
    }

    void SetWallObstacleAABBs(const std::vector<K4E::AABB>* aabbs)
    {
        // 追加: 壁障害物AABBを外部から受け取る。
        wallObstacleAABBs_ = aabbs;
    }

private:
    bool HasTarget() const;
    bool IsTargetInDetectRange() const;
    void FaceToTarget(float deltaTime);
    void FaceToMoveDirection(const K4E::Vector3& moveDirection, float deltaTime);
    void MoveAlongPath(float deltaTime, float moveSpeed);
    void StartHitReaction(const K4E::Vector3& hitDirection);
    void UpdateHitReaction(float deltaTime);
    void StartDeathAnimation(const std::string& reason);
    void UpdateDeathAnimation(float deltaTime);
    float CalculateBombInitialSpeed(float distance) const;
    bool IsDeathActive() const;
    void UpdateVisualAnimation(float deltaTime);
    void ResetTuningToDefault();
    bool SaveTuningToJson(const std::filesystem::path& path, std::string* outMessage = nullptr) const;
    bool LoadTuningFromJson(const std::filesystem::path& path, std::string* outMessage = nullptr);
    void ApplyBasicStatsToEnemyBase();
    void ValidateTuningValues();
    void StartSuicideBombMode();
    void UpdateSuicideBombMode(float deltaTime);
    void ExplodeSuicideBomb(const std::string& reason);
    K4E::Vector3 CalculateSuicideBreakApartDirection() const;
    void KillBySuicideExplosion();
    void UpdateTargetState();
    void TakeDamage(int amount) override;
    void TakeDamage(int amount, const K4E::Vector3& hitDir, float hitPower) override;

private:
    BasicStatsSettings basicStats_{};
    DistanceSettings distance_{};
    MoveSettings move_{};
    BombAttackSettings bombAttack_{};
    SuicideBombSettings suicideBomb_{};
    BombProjectileSettings bombProjectile_{};
    PathSettings path_{};
    PathState pathState_{};
    AnimationSettings animation_{};
    HeadLookSettings headLook_{};
    HitReactionSettings hitReaction_{};
    DeathAnimationSettings deathAnimation_{};
    EnemyAStarNavigator navigator_{};
    BombAttackState bombAttackState_{};
    SuicideBombState suicideBombState_{};
    float suicideChargeEffectTimer_ = 0.0f;
    TargetState targetState_{};
    AnimationStateData animationState_{};
    HeadLookState headLookState_{};
    HitReactionState hitReactionState_{};
    DeathAnimationState deathAnimationState_{};
    TuningIoState tuningIo_{};
    BehaviorState behaviorState_{};
    bool usedEnemyBaseDeathForSuicide_ = false;
    K4E::Vector3 lastSuicideBreakApartDirection_{ 0.0f, 0.0f, 1.0f };
    std::vector<std::unique_ptr<MidRangeBombProjectile>> bombs_{};
    MidRangeBombEffectController bombEffectController_{};
    const std::vector<K4E::AABB>* floorAABBs_ = nullptr;
    const std::vector<K4E::AABB>* wallObstacleAABBs_ = nullptr;
};
