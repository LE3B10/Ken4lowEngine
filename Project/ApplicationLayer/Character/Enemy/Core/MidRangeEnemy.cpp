#define NOMINMAX
#include "MidRangeEnemy.h"

#include "Wireframe.h"

#include <imgui.h>
#include <json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>

using namespace Ken4lowEngine;

namespace
{
    constexpr float kEpsilon = 0.0001f;

    float LengthXZ(const Vector3& v)
    {
        return std::sqrt(v.x * v.x + v.z * v.z);
    }

    Vector3 NormalizeXZ(const Vector3& v)
    {
        const float len = LengthXZ(v);
        if (len < kEpsilon)
        {
            return { 0.0f, 0.0f, 0.0f };
        }
        return { v.x / len, 0.0f, v.z / len };
    }
}

void MidRangeEnemy::Initialize()
{
    EnemyBase::Initialize();
    LoadTuningFromJson(jsonPath_);
}

void MidRangeEnemy::SetTarget(const Vector3& target)
{
    targetPosition_ = target;
    hasTarget_ = true;
}

bool MidRangeEnemy::HasTarget() const
{
    return hasTarget_;
}

bool MidRangeEnemy::IsTargetInDetectRange() const
{
    return targetDistance_ <= distance_.detectRange;
}

void MidRangeEnemy::FaceToTarget(float deltaTime)
{
    (void)deltaTime;
}

void MidRangeEnemy::FaceToMoveDirection(const Vector3& moveDirection, float deltaTime)
{
    (void)moveDirection;
    (void)deltaTime;
}

void MidRangeEnemy::MoveAlongPath(float deltaTime)
{
    if (!path_.pathFindEnabled)
    {
        pathState_.pathFound = false;
        pathState_.lastRepathReason = "PathDisabled";
        return;
    }

    EnemyAStarNavigator::Settings navSettings{};
    navSettings.cellSize = path_.pathGridSize;
    navSettings.agentRadius = path_.obstacleExpandRadius;
    navSettings.repathIntervalSec = path_.repathInterval;
    navSettings.waypointReachDistance = path_.waypointReachDistance;
    navSettings.searchRangeCells = static_cast<int>(path_.pathSearchRadius / std::max(0.1f, path_.pathGridSize));
    navSettings.disableCornerCutting = path_.cornerCuttingDisabled;
    navigator_.SetSettings(navSettings);

    const auto* obstacleAabbs = wallObstacleAABBs_ ? wallObstacleAABBs_ : GetResolvedNavigationObstacleAABBs();
    navigator_.SetWorldAABBs(obstacleAabbs);

    Vector3 waypoint = targetPosition_;
    const Vector3 pos = GetCenterPosition();
    const bool hasWaypoint = navigator_.GetNextWaypoint(pos, targetPosition_, pos.y, deltaTime, waypoint);

    pathState_.pathFound = hasWaypoint;
    pathState_.currentWaypoint = waypoint;
    pathState_.lineBlocked = !hasWaypoint;
    pathState_.lastRepathReason = hasWaypoint ? "WaypointAcquired" : "FallbackDirect";

    const Vector3 navTarget = hasWaypoint ? waypoint : targetPosition_;
    const Vector3 moveDir = NormalizeXZ(navTarget - pos);
    lastMoveDirection_ = moveDir;
    SetCenterPosition(pos + moveDir * move_.moveSpeed * deltaTime);
}

void MidRangeEnemy::Update(float deltaTime)
{
    EnemyBase::Update(deltaTime);

    cooldownTimer_ = std::max(0.0f, cooldownTimer_ - deltaTime);

    if (casting_)
    {
        castTimer_ += deltaTime;
    }

    for (auto& bomb : bombs_)
    {
        bomb->Update(deltaTime);
    }

    bombs_.erase(std::remove_if(bombs_.begin(), bombs_.end(), [](const std::unique_ptr<MidRangeBombProjectile>& bomb)
    {
        return !bomb->IsAlive();
    }), bombs_.end());

    if (!HasTarget())
    {
        inDetect_ = false;
        lastReason_ = "ターゲットなし";
        UpdateVisualAnimation(deltaTime);
        return;
    }

    Vector3 toTarget = targetPosition_ - GetCenterPosition();
    toTarget.y = 0.0f;
    targetDistance_ = LengthXZ(toTarget);
    inDetect_ = IsTargetInDetectRange();

    if (!inDetect_)
    {
        lastReason_ = "検知範囲外";
        animState_ = AnimState::Idle;
        UpdateVisualAnimation(deltaTime);
        return;
    }

    if (targetDistance_ > distance_.attackMaxRange)
    {
        MoveAlongPath(deltaTime);
        animState_ = AnimState::Walk;
        lastReason_ = "経路接近";
    }
    else if (targetDistance_ < distance_.tooCloseRange)
    {
        const Vector3 dir = NormalizeXZ(toTarget);
        const Vector3 retreat = Vector3{ -dir.x, 0.0f, -dir.z };
        lastMoveDirection_ = retreat;
        SetCenterPosition(GetCenterPosition() + retreat * move_.retreatSpeed * deltaTime);
        animState_ = AnimState::Walk;
        lastReason_ = "後退";
    }
    else
    {
        animState_ = AnimState::Idle;
        if (!casting_ && cooldownTimer_ <= 0.0f)
        {
            casting_ = true;
            castTimer_ = 0.0f;
            animState_ = AnimState::Cast;
            lastReason_ = "構え開始";
        }

        if (casting_ && castTimer_ >= bombAttack_.castTime)
        {
            auto bomb = std::make_unique<MidRangeBombProjectile>();
            bomb->Initialize();

            Vector3 start = GetCenterPosition();
            start.y += bombAttack_.throwHeightOffset;
            bomb->Launch(start, targetPosition_, bombProjectile_);
            bombs_.push_back(std::move(bomb));

            casting_ = false;
            castTimer_ = 0.0f;
            cooldownTimer_ = bombAttack_.cooldown;
            animState_ = AnimState::Throw;
            lastReason_ = "爆弾投擲";
        }
    }

    UpdateVisualAnimation(deltaTime);
}

void MidRangeEnemy::UpdateVisualAnimation(float deltaTime)
{
    visualAnimTimer_ += deltaTime;
}

void MidRangeEnemy::Draw()
{
    EnemyBase::Draw();

    for (const auto& bomb : bombs_)
    {
        bomb->Draw();
    }
}

void MidRangeEnemy::DrawImGui()
{
    if (!ImGui::Begin("MidRangeEnemy Debug"))
    {
        ImGui::End();
        return;
    }
    // 追加: MeleeEnemyに揃えてカテゴリ分けした調整UI。
    if (ImGui::CollapsingHeader("データ保存/読み込み"))
    {
        if (ImGui::Button("保存"))
        {
            SaveTuningToJson(jsonPath_);
        }
        if (ImGui::Button("読み込み"))
        {
            LoadTuningFromJson(jsonPath_);
        }
        if (ImGui::Button("デフォルトに戻す"))
        {
            ResetTuningToDefault();
        }
    }
    if (ImGui::CollapsingHeader("基本ステータス"))
    {
        ImGui::DragInt("最大HP", &basicStats_.maxHp, 1, 1, 9999);
    }
    if (ImGui::CollapsingHeader("検知・距離"))
    {
        ImGui::SliderFloat("検知範囲", &distance_.detectRange, 1.0f, 80.0f);
        ImGui::SliderFloat("攻撃最小距離", &distance_.attackMinRange, 0.0f, 20.0f);
        ImGui::SliderFloat("攻撃最大距離", &distance_.attackMaxRange, 0.0f, 30.0f);
        ImGui::SliderFloat("理想距離", &distance_.idealRange, 0.0f, 30.0f);
        ImGui::SliderFloat("近すぎる距離", &distance_.tooCloseRange, 0.0f, 20.0f);
    }
    if (ImGui::CollapsingHeader("移動"))
    {
        ImGui::SliderFloat("移動速度", &move_.moveSpeed, 0.0f, 10.0f);
        ImGui::SliderFloat("後退速度", &move_.retreatSpeed, 0.0f, 10.0f);
        ImGui::SliderFloat("回転速度", &move_.rotateSpeed, 0.0f, 20.0f);
    }
    if (ImGui::CollapsingHeader("爆弾攻撃"))
    {
        ImGui::SliderFloat("クールダウン", &bombAttack_.cooldown, 0.0f, 10.0f);
        ImGui::SliderFloat("構え時間", &bombAttack_.castTime, 0.0f, 3.0f);
        ImGui::SliderFloat("投擲高さ", &bombAttack_.throwHeightOffset, 0.0f, 5.0f);
    }
    if (ImGui::CollapsingHeader("爆弾Projectile"))
    {
        ImGui::SliderFloat("初速", &bombProjectile_.initialSpeed, 0.0f, 60.0f);
    }
    if (ImGui::CollapsingHeader("経路探索"))
    {
        ImGui::Checkbox("有効", &path_.pathFindEnabled);
        ImGui::SliderFloat("リパス間隔", &path_.repathInterval, 0.05f, 2.0f);
    }
    if (ImGui::CollapsingHeader("アニメーション"))
    {
        ImGui::SliderFloat("歩行腕振り", &animation_.walkArmAmplitude, 0.0f, 1.5f);
    }
    if (ImGui::CollapsingHeader("頭向き"))
    {
        ImGui::Checkbox("有効", &headLook_.enabled);
        ImGui::SliderFloat("最大Yaw", &headLook_.maxYaw, 0.0f, 2.0f);
        ImGui::SliderFloat("最大Pitch", &headLook_.maxPitch, 0.0f, 1.5f);
    }
    if (ImGui::CollapsingHeader("状態表示"))
    {
        ImGui::Text("最後の理由: %s", lastReason_.c_str());
        ImGui::Text("クールダウン: %.2f", cooldownTimer_);
        ImGui::Text("構え時間: %.2f", castTimer_);
        ImGui::Text("爆弾数: %d", static_cast<int>(bombs_.size()));
    }
    ImGui::End();
}

void MidRangeEnemy::ResetTuningToDefault()
{
    basicStats_ = BasicStatsSettings{};
    distance_ = DistanceSettings{};
    move_ = MoveSettings{};
    bombAttack_ = BombAttackSettings{};
    bombProjectile_ = BombProjectileSettings{};
    path_ = PathSettings{};
    animation_ = AnimationSettings{};
    headLook_ = HeadLookSettings{};
}

void MidRangeEnemy::SaveTuningToJson(const std::filesystem::path& path) const
{
    nlohmann::json j{};
    j["basicStats"]["maxHp"] = basicStats_.maxHp;
    j["basicStats"]["resetHpOnLoad"] = basicStats_.resetHpOnLoad;
    j["distance"]["detectRange"] = distance_.detectRange;
    j["distance"]["attackMinRange"] = distance_.attackMinRange;
    j["distance"]["attackMaxRange"] = distance_.attackMaxRange;
    j["distance"]["idealRange"] = distance_.idealRange;
    j["distance"]["tooCloseRange"] = distance_.tooCloseRange;
    j["move"]["moveSpeed"] = move_.moveSpeed;
    j["move"]["retreatSpeed"] = move_.retreatSpeed;
    j["move"]["rotateSpeed"] = move_.rotateSpeed;
    j["bombAttack"]["cooldown"] = bombAttack_.cooldown;
    j["bombAttack"]["castTime"] = bombAttack_.castTime;
    j["bombAttack"]["throwHeightOffset"] = bombAttack_.throwHeightOffset;
    j["bombProjectile"]["initialSpeed"] = bombProjectile_.initialSpeed;
    j["path"]["pathFindEnabled"] = path_.pathFindEnabled;
    j["path"]["repathInterval"] = path_.repathInterval;
    j["path"]["waypointReachDistance"] = path_.waypointReachDistance;
    j["path"]["pathGridSize"] = path_.pathGridSize;
    j["path"]["pathSearchRadius"] = path_.pathSearchRadius;
    j["path"]["obstacleExpandRadius"] = path_.obstacleExpandRadius;
    j["path"]["cornerCuttingDisabled"] = path_.cornerCuttingDisabled;
    j["animation"]["walkSwingSpeed"] = animation_.walkSwingSpeed;
    j["animation"]["walkArmAmplitude"] = animation_.walkArmAmplitude;
    j["animation"]["walkLegAmplitude"] = animation_.walkLegAmplitude;
    j["headLook"]["enabled"] = headLook_.enabled;
    j["headLook"]["maxYaw"] = headLook_.maxYaw;
    j["headLook"]["maxPitch"] = headLook_.maxPitch;
    j["headLook"]["followSpeed"] = headLook_.followSpeed;
    std::filesystem::create_directories(path.parent_path());
    std::ofstream ofs(path);
    ofs << j.dump(4);
}

void MidRangeEnemy::LoadTuningFromJson(const std::filesystem::path& path)
{
    std::ifstream ifs(path);
    if (!ifs.is_open())
    {
        return;
    }
    nlohmann::json j{};
    ifs >> j;
    basicStats_.maxHp = j["basicStats"].value("maxHp", basicStats_.maxHp);
    basicStats_.resetHpOnLoad = j["basicStats"].value("resetHpOnLoad", basicStats_.resetHpOnLoad);
    distance_.detectRange = j["distance"].value("detectRange", distance_.detectRange);
    distance_.attackMinRange = j["distance"].value("attackMinRange", distance_.attackMinRange);
    distance_.attackMaxRange = j["distance"].value("attackMaxRange", distance_.attackMaxRange);
    distance_.idealRange = j["distance"].value("idealRange", distance_.idealRange);
    distance_.tooCloseRange = j["distance"].value("tooCloseRange", distance_.tooCloseRange);
    path_.pathFindEnabled = j["path"].value("pathFindEnabled", path_.pathFindEnabled);
    path_.repathInterval = j["path"].value("repathInterval", path_.repathInterval);
    path_.waypointReachDistance = j["path"].value("waypointReachDistance", path_.waypointReachDistance);
    path_.pathGridSize = j["path"].value("pathGridSize", path_.pathGridSize);
    path_.pathSearchRadius = j["path"].value("pathSearchRadius", path_.pathSearchRadius);
    path_.obstacleExpandRadius = j["path"].value("obstacleExpandRadius", path_.obstacleExpandRadius);
    path_.cornerCuttingDisabled = j["path"].value("cornerCuttingDisabled", path_.cornerCuttingDisabled);
    animation_.walkSwingSpeed = j["animation"].value("walkSwingSpeed", animation_.walkSwingSpeed);
    animation_.walkArmAmplitude = j["animation"].value("walkArmAmplitude", animation_.walkArmAmplitude);
    animation_.walkLegAmplitude = j["animation"].value("walkLegAmplitude", animation_.walkLegAmplitude);
    headLook_.enabled = j["headLook"].value("enabled", headLook_.enabled);
    headLook_.maxYaw = j["headLook"].value("maxYaw", headLook_.maxYaw);
    headLook_.maxPitch = j["headLook"].value("maxPitch", headLook_.maxPitch);
    headLook_.followSpeed = j["headLook"].value("followSpeed", headLook_.followSpeed);
}
