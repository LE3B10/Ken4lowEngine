#pragma once

#include "EnemyBase.h"
#include "ApplicationLayer/Character/Enemy/Navigation/EnemyAStarNavigator.h"
#include "ApplicationLayer/Character/Enemy/Projectile/MidRangeBombProjectile.h"

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
        float throwBodyLean = 0.12f;
        float returnSpeed = 10.0f;
    };

    struct HeadLookSettings
    {
        bool enabled = true;
        float maxYaw = 1.0f;
        float maxPitch = 0.6f;
        float followSpeed = 8.0f;
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
    void MoveAlongPath(float deltaTime);
    void UpdateVisualAnimation(float deltaTime);
    void ResetTuningToDefault();
    void SaveTuningToJson(const std::filesystem::path& path) const;
    void LoadTuningFromJson(const std::filesystem::path& path);

private:
    BasicStatsSettings basicStats_{};
    DistanceSettings distance_{};
    MoveSettings move_{};
    BombAttackSettings bombAttack_{};
    BombProjectileSettings bombProjectile_{};
    PathSettings path_{};
    PathState pathState_{};
    AnimationSettings animation_{};
    HeadLookSettings headLook_{};
    EnemyAStarNavigator navigator_{};
    K4E::Vector3 targetPosition_{};
    K4E::Vector3 lastMoveDirection_{};
    float cooldownTimer_ = 0.0f;
    float castTimer_ = 0.0f;
    float targetDistance_ = 0.0f;
    float visualAnimTimer_ = 0.0f;
    float headYaw_ = 0.0f;
    float headPitch_ = 0.0f;
    bool hasTarget_ = false;
    bool casting_ = false;
    bool inDetect_ = false;
    AnimState animState_ = AnimState::Idle;
    std::string lastReason_ = "初期化";
    std::vector<std::unique_ptr<MidRangeBombProjectile>> bombs_{};
    std::filesystem::path jsonPath_ = "Resources/DataAssets/Enemy/MidRangeEnemy/MidRangeEnemy_Normal.json";
    const std::vector<K4E::AABB>* floorAABBs_ = nullptr;
    const std::vector<K4E::AABB>* wallObstacleAABBs_ = nullptr;
};
