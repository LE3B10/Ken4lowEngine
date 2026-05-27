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
    constexpr float kPi = 3.1415926535f;
    constexpr float kTwoPi = kPi * 2.0f;

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

    float NormalizeAngleRad(float v)
    {
        while (v > kPi)
        {
            v -= kTwoPi;
        }
        while (v < -kPi)
        {
            v += kTwoPi;
        }
        return v;
    }

    float ToDeg(float rad)
    {
        return rad * (180.0f / kPi);
    }

    float ToRad(float deg)
    {
        return deg * (kPi / 180.0f);
    }
}

void MidRangeEnemy::Initialize()
{
    EnemyBase::Initialize();
    LoadTuningFromJson(tuningIo_.jsonPath, &tuningIo_.lastLoadResult);
}

void MidRangeEnemy::SetTarget(const Vector3& target)
{
    targetState_.position = target;
    targetState_.hasTarget = true;
}

bool MidRangeEnemy::HasTarget() const
{
    return targetState_.hasTarget;
}

bool MidRangeEnemy::IsTargetInDetectRange() const
{
    return targetState_.distance <= distance_.detectRange;
}

void MidRangeEnemy::FaceToTarget(float deltaTime)
{
    if (!HasTarget())
    {
        return;
    }
    FaceToMoveDirection(targetState_.direction, deltaTime);
}

void MidRangeEnemy::FaceToMoveDirection(const Vector3& moveDirection, float deltaTime)
{
    const Vector3 dir = NormalizeXZ(moveDirection);
    if (LengthXZ(dir) < kEpsilon)
    {
        return;
    }
    animationState_.faceDirection = dir;
    animationState_.targetYaw = std::atan2(-dir.x, dir.z);
    animationState_.currentYaw = NormalizeAngleRad(orientation_.y);
    const float maxStep = move_.rotateSpeed * std::max(deltaTime, 0.0f);
    float deltaYaw = NormalizeAngleRad(animationState_.targetYaw - animationState_.currentYaw);
    deltaYaw = std::clamp(deltaYaw, -maxStep, maxStep);
    animationState_.currentYaw = NormalizeAngleRad(animationState_.currentYaw + deltaYaw);
    SetOrientation({ 0.0f, animationState_.currentYaw, 0.0f });
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

    Vector3 waypoint = targetState_.position;
    const Vector3 pos = GetCenterPosition();
    const bool hasWaypoint = navigator_.GetNextWaypoint(pos, targetState_.position, pos.y, deltaTime, waypoint);

    pathState_.pathFound = hasWaypoint;
    pathState_.currentWaypoint = waypoint;
    pathState_.lineBlocked = !hasWaypoint;
    pathState_.lastRepathReason = hasWaypoint ? "WaypointAcquired" : "FallbackDirect";

    const Vector3 navTarget = hasWaypoint ? waypoint : targetState_.position;
    const Vector3 moveDir = NormalizeXZ(navTarget - pos);
    animationState_.moveDirection = moveDir;
    SetCenterPosition(pos + moveDir * move_.moveSpeed * deltaTime);
}

void MidRangeEnemy::Update(float deltaTime)
{
    EnemyBase::Update(deltaTime);

    bombAttackState_.cooldownTimer = std::max(0.0f, bombAttackState_.cooldownTimer - deltaTime);
    if (bombAttackState_.casting)
    {
        bombAttackState_.castTimer += deltaTime;
    }
    if (bombAttackState_.throwAnimTimer > 0.0f)
    {
        bombAttackState_.throwAnimTimer = std::max(0.0f, bombAttackState_.throwAnimTimer - deltaTime);
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
            }),
        bombs_.end());

    if (!HasTarget())
    {
        targetState_.inDetectRange = false;
        behaviorState_.currentBehaviorName = "Idle";
        behaviorState_.lastReason = "ターゲットなし";
        animationState_.animState = AnimState::Idle;
        UpdateVisualAnimation(deltaTime);
        return;
    }

    Vector3 toTarget = targetState_.position - GetCenterPosition();
    toTarget.y = 0.0f;
    targetState_.distance = LengthXZ(toTarget);
    targetState_.direction = NormalizeXZ(toTarget);
    targetState_.inDetectRange = IsTargetInDetectRange();
    targetState_.inAttackRange = targetState_.distance <= distance_.attackMaxRange;
    targetState_.tooClose = targetState_.distance < distance_.tooCloseRange;

    if (!targetState_.inDetectRange)
    {
        behaviorState_.currentBehaviorName = "Idle";
        behaviorState_.lastReason = "検知範囲外";
        animationState_.animState = AnimState::Idle;
        UpdateVisualAnimation(deltaTime);
        return;
    }

    if (!targetState_.inAttackRange)
    {
        MoveAlongPath(deltaTime);
        FaceToMoveDirection(animationState_.moveDirection, deltaTime);
        animationState_.animState = AnimState::Walk;
        behaviorState_.currentBehaviorName = "Approach";
        behaviorState_.lastReason = "経路接近";
    }
    else if (targetState_.tooClose)
    {
        const Vector3 retreat = Vector3{ -targetState_.direction.x, 0.0f, -targetState_.direction.z };
        animationState_.moveDirection = retreat;
        SetCenterPosition(GetCenterPosition() + retreat * move_.retreatSpeed * deltaTime);
        FaceToMoveDirection(retreat, deltaTime);
        animationState_.animState = AnimState::Walk;
        behaviorState_.currentBehaviorName = "Retreat";
        behaviorState_.lastReason = "後退";
    }
    else
    {
        FaceToTarget(deltaTime);
        animationState_.animState = AnimState::Idle;
        behaviorState_.currentBehaviorName = "AttackReady";
        behaviorState_.lastReason = "攻撃距離内";

        if (!bombAttackState_.casting && bombAttackState_.cooldownTimer <= 0.0f)
        {
            bombAttackState_.casting = true;
            bombAttackState_.castTimer = 0.0f;
            bombAttackState_.thrownThisCast = false;
            animationState_.animState = AnimState::Cast;
            behaviorState_.lastReason = "構え開始";
            bombAttackState_.lastReason = "CastStart";
        }

        if (bombAttackState_.casting)
        {
            animationState_.animState = AnimState::Cast;
            if (bombAttackState_.castTimer >= bombAttack_.castTime && !bombAttackState_.thrownThisCast)
            {
                auto bomb = std::make_unique<MidRangeBombProjectile>();
                bomb->Initialize();
                Vector3 start = GetCenterPosition();
                start.y += bombAttack_.throwHeightOffset;
                bomb->Launch(start, targetState_.position, bombProjectile_);
                bombs_.push_back(std::move(bomb));

                bombAttackState_.thrownThisCast = true;
                bombAttackState_.casting = false;
                bombAttackState_.castTimer = 0.0f;
                bombAttackState_.cooldownTimer = bombAttack_.cooldown;
                bombAttackState_.throwAnimTimer = bombAttackState_.throwAnimDuration;
                animationState_.animState = AnimState::Throw;
                behaviorState_.lastReason = "爆弾投擲";
                bombAttackState_.lastReason = "Thrown";
            }
        }
    }

    if (bombAttackState_.throwAnimTimer > 0.0f && !bombAttackState_.casting)
    {
        animationState_.animState = AnimState::Throw;
    }

    UpdateVisualAnimation(deltaTime);
}

void MidRangeEnemy::UpdateVisualAnimation(float deltaTime)
{
    if (parts_.size() < 5 || !body_.object)
    {
        return;
    }
    const uint32_t head = partIndices_.head;
    const uint32_t lArm = partIndices_.leftArm;
    const uint32_t rArm = partIndices_.rightArm;
    const uint32_t lLeg = partIndices_.leftLeg;
    const uint32_t rLeg = partIndices_.rightLeg;

    const float speedRate = std::min(1.5f, LengthXZ(animationState_.moveDirection) * std::max(0.1f, move_.moveSpeed));
    animationState_.walkAnimTime += deltaTime * animation_.walkSwingSpeed * std::max(0.2f, speedRate);

    const float castDenom = std::max(bombAttack_.castTime, kEpsilon);
    animationState_.attackProgress = std::clamp(bombAttackState_.castTimer / castDenom, 0.0f, 1.0f);

    float armSwing = 0.0f;
    float legSwing = 0.0f;
    float leftArmPitch = 0.0f;
    float rightArmPitch = 0.0f;
    float rightArmYaw = 0.0f;
    float bodyLean = 0.0f;

    if (animationState_.animState == AnimState::Walk)
    {
        armSwing = std::sin(animationState_.walkAnimTime) * animation_.walkArmAmplitude;
        legSwing = std::sin(animationState_.walkAnimTime) * animation_.walkLegAmplitude;
    }
    if (animationState_.animState == AnimState::Cast)
    {
        const float p = animationState_.attackProgress;
        rightArmPitch = animation_.castArmPitch * p;
        rightArmYaw = animation_.castArmYaw * p;
        leftArmPitch = animation_.castArmPitch * 0.5f * p;
        bodyLean = animation_.bodyCastLean * p;
    }
    if (animationState_.animState == AnimState::Throw)
    {
        float throwProgress = 1.0f;
        if (bombAttackState_.throwAnimDuration > kEpsilon)
        {
            throwProgress = 1.0f - bombAttackState_.throwAnimTimer / bombAttackState_.throwAnimDuration;
            throwProgress = std::clamp(throwProgress, 0.0f, 1.0f);
        }
        rightArmPitch = animation_.castArmPitch + (animation_.throwArmPitch - animation_.castArmPitch) * throwProgress;
        rightArmYaw = animation_.castArmYaw * (1.0f - throwProgress);
        leftArmPitch = animation_.castArmPitch * 0.4f * (1.0f - throwProgress);
        bodyLean = animation_.bodyCastLean + (animation_.throwBodyLean - animation_.bodyCastLean) * throwProgress;
    }
    if (animationState_.animState == AnimState::Dead)
    {
        rightArmPitch = -1.0f;
        leftArmPitch = -1.0f;
        bodyLean = 0.25f;
        armSwing = 0.0f;
        legSwing = 0.0f;
    }

    const float ret = std::clamp(deltaTime * animation_.returnSpeed, 0.0f, 1.0f);
    parts_[lArm].transform.rotate_.x += ((-armSwing + leftArmPitch) - parts_[lArm].transform.rotate_.x) * ret;
    parts_[lArm].transform.rotate_.y += ((-rightArmYaw * 0.25f) - parts_[lArm].transform.rotate_.y) * ret;
    parts_[rArm].transform.rotate_.x += ((armSwing + rightArmPitch) - parts_[rArm].transform.rotate_.x) * ret;
    parts_[rArm].transform.rotate_.y += ((rightArmYaw) - parts_[rArm].transform.rotate_.y) * ret;
    parts_[lLeg].transform.rotate_.x += ((-legSwing) - parts_[lLeg].transform.rotate_.x) * ret;
    parts_[rLeg].transform.rotate_.x += ((legSwing) - parts_[rLeg].transform.rotate_.x) * ret;
    body_.transform.rotate_.x += (bodyLean - body_.transform.rotate_.x) * ret;

    const bool canLook = headLook_.enabled && targetState_.hasTarget && targetState_.inDetectRange;
    headLookState_.targetVisible = canLook;
    if (canLook)
    {
        const Vector3 dir = NormalizeXZ(targetState_.position - GetCenterPosition());
        const float bodyYaw = orientation_.y;
        const float worldYaw = std::atan2(-dir.x, dir.z);
        const float localYaw = NormalizeAngleRad(worldYaw - bodyYaw);
        headLookState_.targetYaw = std::clamp(ToDeg(localYaw), -headLook_.yawLimitDeg, headLook_.yawLimitDeg);

        const float dy = targetState_.position.y - (GetCenterPosition().y + 2.0f);
        const float flat = std::max(LengthXZ(dir), kEpsilon);
        const float pitchDeg = ToDeg(std::atan2(dy, flat));
        headLookState_.targetPitch = std::clamp(pitchDeg, headLook_.pitchMinDeg, headLook_.pitchMaxDeg);
        headLookState_.reason = "TargetInRange";
    }
    else
    {
        headLookState_.targetYaw = 0.0f;
        headLookState_.targetPitch = 0.0f;
        headLookState_.reason = headLook_.enabled ? "OutOfRange" : "Disabled";
    }

    const float headRet = std::clamp(deltaTime * headLook_.lerpSpeed, 0.0f, 1.0f);
    headLookState_.currentYaw += (headLookState_.targetYaw - headLookState_.currentYaw) * headRet;
    headLookState_.currentPitch += (headLookState_.targetPitch - headLookState_.currentPitch) * headRet;
    parts_[head].transform.rotate_.y = ToRad(headLookState_.currentYaw);
    parts_[head].transform.rotate_.x = ToRad(headLookState_.currentPitch);
}

void MidRangeEnemy::Draw()
{
    EnemyBase::Draw();

    for (const auto& bomb : bombs_)
    {
        bomb->Draw();
    }

    if (pathState_.pathFound)
    {
        Wireframe::GetInstance()->DrawLine(GetCenterPosition(), pathState_.currentWaypoint, { 0.2f, 0.8f, 1.0f, 1.0f });
        Wireframe::GetInstance()->DrawSphere(pathState_.currentWaypoint, 0.25f, { 0.2f, 0.8f, 1.0f, 1.0f });
    }

    if (targetState_.hasTarget)
    {
        Wireframe::GetInstance()->DrawLine(GetCenterPosition(), targetState_.position, { 1.0f, 0.8f, 0.2f, 1.0f });
    }

    const Vector3 faceEnd = GetCenterPosition() + animationState_.faceDirection * 2.0f;
    Wireframe::GetInstance()->DrawLine(GetCenterPosition(), faceEnd, { 0.7f, 1.0f, 0.2f, 1.0f });
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
            SaveTuningToJson(tuningIo_.jsonPath, &tuningIo_.lastSaveResult);
        }
        if (ImGui::Button("読み込み"))
        {
            LoadTuningFromJson(tuningIo_.jsonPath, &tuningIo_.lastLoadResult);
        }
        if (ImGui::Button("デフォルトに戻す"))
        {
            ResetTuningToDefault();
        }
        ImGui::Text("読み込み結果: %s", tuningIo_.lastLoadResult.c_str());
        ImGui::Text("保存結果: %s", tuningIo_.lastSaveResult.c_str());
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
        ImGui::SliderFloat("構え時間", &bombAttack_.castTime, 0.05f, 3.0f);
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
        ImGui::SliderFloat("歩行アニメ速度", &animation_.walkSwingSpeed, 0.1f, 20.0f);
        ImGui::SliderFloat("歩行腕振り", &animation_.walkArmAmplitude, 0.0f, 1.5f);
        ImGui::SliderFloat("歩行脚振り", &animation_.walkLegAmplitude, 0.0f, 1.5f);
        ImGui::SliderFloat("構え腕Pitch", &animation_.castArmPitch, -2.0f, 2.0f);
        ImGui::SliderFloat("構え腕Yaw", &animation_.castArmYaw, -2.0f, 2.0f);
        ImGui::SliderFloat("投げ腕Pitch", &animation_.throwArmPitch, -2.0f, 2.0f);
        ImGui::SliderFloat("構え体傾き", &animation_.bodyCastLean, -0.5f, 0.5f);
        ImGui::SliderFloat("投げ体傾き", &animation_.throwBodyLean, -0.5f, 0.5f);
        ImGui::SliderFloat("戻り速度", &animation_.returnSpeed, 0.1f, 30.0f);
        ImGui::Text("現在アニメ状態: %d", static_cast<int>(animationState_.animState));
        ImGui::Text("攻撃進行度: %.2f", animationState_.attackProgress);
    }
    if (ImGui::CollapsingHeader("頭向き"))
    {
        ImGui::Checkbox("頭をターゲットへ向ける", &headLook_.enabled);
        ImGui::SliderFloat("Yaw制限", &headLook_.yawLimitDeg, 0.0f, 180.0f);
        ImGui::SliderFloat("Pitch最小", &headLook_.pitchMinDeg, -89.0f, 0.0f);
        ImGui::SliderFloat("Pitch最大", &headLook_.pitchMaxDeg, 0.0f, 89.0f);
        ImGui::SliderFloat("補間速度", &headLook_.lerpSpeed, 0.1f, 30.0f);
        ImGui::Text("頭がターゲットを見ているか: %s", headLookState_.targetVisible ? "はい" : "いいえ");
        ImGui::Text("頭向き理由: %s", headLookState_.reason.c_str());
    }
    if (ImGui::CollapsingHeader("状態表示"))
    {
        ImGui::Text("現在行動: %s", behaviorState_.currentBehaviorName.c_str());
        ImGui::Text("最後の理由: %s", behaviorState_.lastReason.c_str());
        ImGui::Text("ターゲット距離: %.2f", targetState_.distance);
        ImGui::Text("検知中: %s", targetState_.inDetectRange ? "はい" : "いいえ");
        ImGui::Text("攻撃範囲内: %s", targetState_.inAttackRange ? "はい" : "いいえ");
        ImGui::Text("近すぎる: %s", targetState_.tooClose ? "はい" : "いいえ");
        ImGui::Text("構え中: %s", bombAttackState_.casting ? "はい" : "いいえ");
        ImGui::Text("クールダウン残り: %.2f", bombAttackState_.cooldownTimer);
        ImGui::Text("現在爆弾数: %d", static_cast<int>(bombs_.size()));
        ImGui::Text("経路が見つかったか: %s", pathState_.pathFound ? "はい" : "いいえ");
        ImGui::Text("現在ウェイポイント: (%.2f, %.2f, %.2f)", pathState_.currentWaypoint.x, pathState_.currentWaypoint.y, pathState_.currentWaypoint.z);
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

bool MidRangeEnemy::SaveTuningToJson(const std::filesystem::path& path, std::string* outMessage) const
{
    try
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
        j["animation"]["castArmPitch"] = animation_.castArmPitch;
        j["animation"]["castArmYaw"] = animation_.castArmYaw;
        j["animation"]["throwArmPitch"] = animation_.throwArmPitch;
        j["animation"]["bodyCastLean"] = animation_.bodyCastLean;
        j["animation"]["throwBodyLean"] = animation_.throwBodyLean;
        j["animation"]["returnSpeed"] = animation_.returnSpeed;
        j["headLook"]["enabled"] = headLook_.enabled;
        j["headLook"]["yawLimitDeg"] = headLook_.yawLimitDeg;
        j["headLook"]["pitchMinDeg"] = headLook_.pitchMinDeg;
        j["headLook"]["pitchMaxDeg"] = headLook_.pitchMaxDeg;
        j["headLook"]["lerpSpeed"] = headLook_.lerpSpeed;

        std::filesystem::create_directories(path.parent_path());
        std::ofstream ofs(path);
        ofs << j.dump(4);
        if (outMessage)
        {
            *outMessage = "保存成功";
        }
        return true;
    }
    catch (const std::exception& e)
    {
        if (outMessage)
        {
            *outMessage = std::string("保存失敗: ") + e.what();
        }
        return false;
    }
}

bool MidRangeEnemy::LoadTuningFromJson(const std::filesystem::path& path, std::string* outMessage)
{
    try
    {
        std::ifstream ifs(path);
        if (!ifs.is_open())
        {
            if (outMessage)
            {
                *outMessage = "ファイルなし: デフォルト値を使用";
            }
            return false;
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
        move_.moveSpeed = j["move"].value("moveSpeed", move_.moveSpeed);
        move_.retreatSpeed = j["move"].value("retreatSpeed", move_.retreatSpeed);
        move_.rotateSpeed = j["move"].value("rotateSpeed", move_.rotateSpeed);
        bombAttack_.cooldown = j["bombAttack"].value("cooldown", bombAttack_.cooldown);
        bombAttack_.castTime = j["bombAttack"].value("castTime", bombAttack_.castTime);
        bombAttack_.throwHeightOffset = j["bombAttack"].value("throwHeightOffset", bombAttack_.throwHeightOffset);
        bombProjectile_.initialSpeed = j["bombProjectile"].value("initialSpeed", bombProjectile_.initialSpeed);
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
        animation_.castArmPitch = j["animation"].value("castArmPitch", animation_.castArmPitch);
        animation_.castArmYaw = j["animation"].value("castArmYaw", animation_.castArmYaw);
        animation_.throwArmPitch = j["animation"].value("throwArmPitch", animation_.throwArmPitch);
        animation_.bodyCastLean = j["animation"].value("bodyCastLean", animation_.bodyCastLean);
        animation_.throwBodyLean = j["animation"].value("throwBodyLean", animation_.throwBodyLean);
        animation_.returnSpeed = j["animation"].value("returnSpeed", animation_.returnSpeed);
        headLook_.enabled = j["headLook"].value("enabled", headLook_.enabled);
        headLook_.yawLimitDeg = j["headLook"].value("yawLimitDeg", headLook_.yawLimitDeg);
        headLook_.pitchMinDeg = j["headLook"].value("pitchMinDeg", headLook_.pitchMinDeg);
        headLook_.pitchMaxDeg = j["headLook"].value("pitchMaxDeg", headLook_.pitchMaxDeg);
        headLook_.lerpSpeed = j["headLook"].value("lerpSpeed", headLook_.lerpSpeed);

        if (outMessage)
        {
            *outMessage = "読み込み成功";
        }
        return true;
    }
    catch (const std::exception& e)
    {
        if (outMessage)
        {
            *outMessage = std::string("読み込み失敗: ") + e.what();
        }
        return false;
    }
}
