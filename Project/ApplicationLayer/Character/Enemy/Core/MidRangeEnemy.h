#pragma once

#include "EnemyBase.h"
#include "ApplicationLayer/Character/Enemy/Navigation/EnemyAStarNavigator.h"
#include "ApplicationLayer/Character/Enemy/Projectile/MidRangeBombProjectile.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class MidRangeEnemy final : public EnemyBase
{
private:
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
        bool enabled = true;
        float repathInterval = 0.25f;
        float waypointReachDistance = 0.85f;
        float gridSize = 1.5f;
        float searchRadius = 28.0f;
        float obstacleExpandRadius = 0.9f;
        bool cornerCuttingDisabled = true;
    };

public:
    void Initialize() override;
    void Update(float deltaTime) override;
    void Draw() override;
    void DrawImGui() override;
    void SetTarget(const Ken4lowEngine::Vector3& target);

private:
    void SaveToJson(const std::filesystem::path& path) const;
    void LoadFromJson(const std::filesystem::path& path);

private:
    BasicStatsSettings basicStats_{};
    DistanceSettings distance_{};
    MoveSettings move_{};
    BombAttackSettings bombAttack_{};
    BombProjectileSettings bombProjectile_{};
    PathSettings path_{};
    EnemyAStarNavigator navigator_{};
    Ken4lowEngine::Vector3 targetPosition_{};
    float cooldownTimer_ = 0.0f;
    float castTimer_ = 0.0f;
    float targetDistance_ = 0.0f;
    bool hasTarget_ = false;
    bool casting_ = false;
    bool inDetect_ = false;
    std::string lastReason_ = "初期化";
    std::vector<std::unique_ptr<MidRangeBombProjectile>> bombs_{};
    std::filesystem::path jsonPath_ = "Resources/DataAssets/Enemy/MidRangeEnemy/MidRangeEnemy_Normal.json";
};
