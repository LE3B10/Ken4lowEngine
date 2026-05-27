#define NOMINMAX
#include "MidRangeEnemy.h"

#include "Wireframe.h"

#include <imgui.h>
#include <json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>

using namespace Ken4lowEngine;

void MidRangeEnemy::Initialize()
{
    // 追加: 中距離敵の初期化とJSON読み込み。
    EnemyBase::Initialize();
    LoadFromJson(jsonPath_);
    navigator_.SetWorldAABBs(GetGlobalStageNavigationObstacleAABBs());
}

void MidRangeEnemy::SetTarget(const Vector3& target)
{
    targetPosition_ = target;
    hasTarget_ = true;
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

    bombs_.erase(
        std::remove_if(
            bombs_.begin(),
            bombs_.end(),
            [](const std::unique_ptr<MidRangeBombProjectile>& bomb)
            {
                return !bomb->IsAlive();
            }
        ),
        bombs_.end()
    );

    if (!hasTarget_)
    {
        inDetect_ = false;
        lastReason_ = "ターゲットなし";
        return;
    }

    Vector3 toTarget = targetPosition_ - GetCenterPosition();
    targetDistance_ = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);
    inDetect_ = targetDistance_ <= distance_.detectRange;
    if (!inDetect_)
    {
        lastReason_ = "検知範囲外";
        return;
    }

    Vector3 dir = {};
    if (targetDistance_ > 0.0001f)
    {
        dir.x = toTarget.x / targetDistance_;
        dir.z = toTarget.z / targetDistance_;
    }
    else
    {
        dir = { 0.0f, 0.0f, 1.0f };
    }

    Vector3 pos = GetCenterPosition();

    if (targetDistance_ > distance_.attackMaxRange)
    {
        EnemyAStarNavigator::Settings navSettings{};
        navSettings.cellSize = path_.gridSize;
        navSettings.agentRadius = path_.obstacleExpandRadius;
        navSettings.repathIntervalSec = path_.repathInterval;
        navSettings.waypointReachDistance = path_.waypointReachDistance;
        navSettings.searchRangeCells = static_cast<int>(path_.searchRadius / std::max(0.1f, path_.gridSize));
        navSettings.disableCornerCutting = path_.cornerCuttingDisabled;
        navigator_.SetSettings(navSettings);

        Vector3 waypoint = targetPosition_;
        bool hasWaypoint = false;
        if (path_.enabled)
        {
            hasWaypoint = navigator_.GetNextWaypoint(pos, targetPosition_, pos.y, deltaTime, waypoint);
        }

        Vector3 navTarget = hasWaypoint ? waypoint : targetPosition_;
        Vector3 navDir = navTarget - pos;
        navDir.y = 0.0f;
        float navLen = std::sqrt(navDir.x * navDir.x + navDir.z * navDir.z);
        if (navLen > 0.0001f)
        {
            navDir.x /= navLen;
            navDir.z /= navLen;
            pos += navDir * move_.moveSpeed * deltaTime;
            SetCenterPosition(pos);
        }
        lastReason_ = "接近";
        return;
    }

    if (targetDistance_ < distance_.tooCloseRange)
    {
        pos -= dir * move_.retreatSpeed * deltaTime;
        SetCenterPosition(pos);
        lastReason_ = "後退";
        return;
    }

    if (targetDistance_ >= distance_.attackMinRange && targetDistance_ <= distance_.attackMaxRange)
    {
        if (!casting_ && cooldownTimer_ <= 0.0f)
        {
            casting_ = true;
            castTimer_ = 0.0f;
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
            lastReason_ = "爆弾投擲";
        }
    }
}

void MidRangeEnemy::Draw()
{
    EnemyBase::Draw();

    for (const auto& bomb : bombs_)
    {
        bomb->Draw();
    }

    Wireframe::GetInstance()->DrawSphere(GetCenterPosition(), distance_.detectRange, { 0.3f, 0.9f, 0.9f, 0.2f });

    const std::vector<Vector3>& pathPoints = navigator_.GetCurrentPath();
    for (size_t i = 1; i < pathPoints.size(); ++i)
    {
        Wireframe::GetInstance()->DrawLine(pathPoints[i - 1], pathPoints[i], { 0.2f, 1.0f, 0.9f, 1.0f });
    }
}

void MidRangeEnemy::DrawImGui()
{
    if (!ImGui::Begin("MidRangeEnemy Debug"))
    {
        ImGui::End();
        return;
    }
    if (ImGui::CollapsingHeader("データ保存/読み込み"))
    {
        if (ImGui::Button("保存"))
        {
            SaveToJson(jsonPath_);
        }
        if (ImGui::Button("読み込み"))
        {
            LoadFromJson(jsonPath_);
        }
    }
    ImGui::DragFloat("検知範囲", &distance_.detectRange, 0.1f, 1.0f, 100.0f);
    ImGui::DragFloat("攻撃最小距離", &distance_.attackMinRange, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("攻撃最大距離", &distance_.attackMaxRange, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("近すぎる距離", &distance_.tooCloseRange, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("移動速度", &move_.moveSpeed, 0.05f, 0.0f, 20.0f);
    ImGui::DragFloat("後退速度", &move_.retreatSpeed, 0.05f, 0.0f, 20.0f);
    ImGui::DragFloat("攻撃クールダウン", &bombAttack_.cooldown, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("構え時間", &bombAttack_.castTime, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("爆弾初速", &bombProjectile_.initialSpeed, 0.01f, 0.0f, 50.0f);
    ImGui::Text("クールダウン残り: %.2f", cooldownTimer_);
    ImGui::Text("構え中: %s", casting_ ? "はい" : "いいえ");
    ImGui::Text("現在の爆弾数: %d", static_cast<int>(bombs_.size()));
    ImGui::Text("最後の行動理由: %s", lastReason_.c_str());
    ImGui::End();
}

void MidRangeEnemy::SaveToJson(const std::filesystem::path& path) const
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

    std::filesystem::create_directories(path.parent_path());
    std::ofstream ofs(path);
    ofs << j.dump(4);
}

void MidRangeEnemy::LoadFromJson(const std::filesystem::path& path)
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
}
