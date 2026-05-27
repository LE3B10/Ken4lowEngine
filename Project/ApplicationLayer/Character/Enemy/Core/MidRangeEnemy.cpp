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
    // 追加: 初期設定の最大HPをEnemyBaseへ反映する。
    ApplyBasicStatsToEnemyBase();
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
    // 追加: まず基礎更新を行う。
    EnemyBase::Update(deltaTime);
    // 追加: 死亡演出開始前はHP0到達を検知する。
    if (!IsDeathActive() && GetHp() <= 0)
    {
        StartDeathAnimation("HpZero");
    }

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
    // 追加: 死亡中は死亡演出と既存爆弾更新のみ行う。
    if (IsDeathActive())
    {
        UpdateDeathAnimation(deltaTime);
        UpdateVisualAnimation(deltaTime);
        return;
    }
    // 追加: 被ダメージリアクション中は専用更新を行う。
    UpdateHitReaction(deltaTime);
    if (hitReactionState_.active && hitReaction_.stopBehaviorWhileActive)
    {
        behaviorState_.currentBehaviorName = "HitReaction";
        behaviorState_.lastReason = "被ダメージ行動停止";
        animationState_.animState = AnimState::Idle;
        UpdateVisualAnimation(deltaTime);
        return;
    }

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
                // 追加: 発射時だけ距離に応じた初速を反映する。
                BombProjectileSettings launchSettings = bombProjectile_;
                launchSettings.initialSpeed = CalculateBombInitialSpeed(targetState_.distance);
                bomb->Launch(start, targetState_.position, launchSettings);
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

void MidRangeEnemy::TakeDamage(int amount)
{
    // 追加: 方向不明時はターゲット逆方向でリアクションする。
    EnemyBase::TakeDamage(amount);
    Vector3 fallbackDir = { 0.0f, 0.0f, -1.0f };
    if (targetState_.hasTarget)
    {
        fallbackDir = NormalizeXZ(GetCenterPosition() - targetState_.position);
    }
    StartHitReaction(fallbackDir);
    if (GetHp() <= 0)
    {
        StartDeathAnimation("DamagedNoDir");
    }
}

void MidRangeEnemy::TakeDamage(int amount, const Vector3& hitDir, float hitPower)
{
    // 追加: 被弾方向がある場合は逆方向へノックバックする。
    EnemyBase::TakeDamage(amount, hitDir, hitPower);
    StartHitReaction(hitDir * -1.0f);
    if (GetHp() <= 0)
    {
        StartDeathAnimation("DamagedWithDir");
    }
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
    // 追加: 被ダメージ中はのけぞり量を加算する。
    if (hitReactionState_.active)
    {
        bodyLean += hitReaction_.bodyLean;
    }
    // 追加: 死亡演出中は倒れ角を優先する。
    if (IsDeathActive())
    {
        const float t = std::clamp(deathAnimationState_.timer / std::max(deathAnimation_.duration, kEpsilon), 0.0f, 1.0f);
        bodyLean = deathAnimation_.fallRotateX * t;
    }
    body_.transform.rotate_.x += (bodyLean - body_.transform.rotate_.x) * ret;

    // 追加: 頭向き可否の各条件をデバッグ表示用に個別保存する。
    headLookState_.enabledCondition = headLook_.enabled;
    headLookState_.hasTargetCondition = targetState_.hasTarget;
    headLookState_.inDetectCondition = targetState_.inDetectRange;
    headLookState_.deathCondition = !IsDeathActive();
    const bool canLook = headLookState_.enabledCondition
        && headLookState_.hasTargetCondition
        && headLookState_.inDetectCondition
        && headLookState_.deathCondition;
    headLookState_.targetVisible = canLook;
    headLookState_.toTarget = targetState_.position - GetCenterPosition();
    headLookState_.horizontalDistance = std::max(LengthXZ(headLookState_.toTarget), kEpsilon);
    if (canLook)
    {
        // 追加: MeleeEnemy同様にターゲットへの生ベクトルから頭向きを算出する。
        const Vector3 toTarget = headLookState_.toTarget;
        const float desiredYaw = std::atan2(-toTarget.x, toTarget.z);
        const float bodyYaw = orientation_.y;
        const float yawDelta = NormalizeAngleRad(desiredYaw - bodyYaw);
        headLookState_.targetYaw = std::clamp(ToDeg(yawDelta), -headLook_.yawLimitDeg, headLook_.yawLimitDeg);

        // 追加: 生の水平距離を使ってPitchを計算し、上下追従を正しく反映する。
        const float rawPitchDeg = -ToDeg(std::atan2(toTarget.y, headLookState_.horizontalDistance));
        const float signedPitchDeg = rawPitchDeg * headLook_.pitchSign;
        headLookState_.targetPitch = std::clamp(signedPitchDeg, headLook_.pitchMinDeg, headLook_.pitchMaxDeg);
        headLookState_.reason = "TargetInRange";
    }
    else
    {
        headLookState_.targetYaw = 0.0f;
        headLookState_.targetPitch = 0.0f;
        // 追加: 頭向き不可の理由を条件ごとに明示する。
        if (!headLookState_.enabledCondition)
        {
            headLookState_.reason = "Disabled";
        }
        else if (!headLookState_.hasTargetCondition)
        {
            headLookState_.reason = "NoTarget";
        }
        else if (!headLookState_.inDetectCondition)
        {
            headLookState_.reason = "OutOfDetectRange";
        }
        else if (!headLookState_.deathCondition)
        {
            headLookState_.reason = "Dead";
        }
        else
        {
            headLookState_.reason = "Unknown";
        }
    }

    const float headRet = std::clamp(deltaTime * headLook_.lerpSpeed, 0.0f, 1.0f);
    headLookState_.currentYaw += (headLookState_.targetYaw - headLookState_.currentYaw) * headRet;
    headLookState_.currentPitch += (headLookState_.targetPitch - headLookState_.currentPitch) * headRet;
    parts_[head].transform.rotate_.y = ToRad(headLookState_.currentYaw);
    parts_[head].transform.rotate_.x = ToRad(headLookState_.currentPitch);
    // 追加: パーツ回転をObject3Dへ反映するため、最後に階層更新する。
    UpdateVisualHierarchy();
}

void MidRangeEnemy::StartHitReaction(const Vector3& hitDirection)
{
    // 追加: 被ダメージリアクション開始。
    if (!hitReaction_.enabled)
    {
        hitReactionState_.lastReason = "Disabled";
        return;
    }
    hitReactionState_.active = true;
    hitReactionState_.timer = hitReaction_.duration;
    hitReactionState_.knockbackDirection = NormalizeXZ(hitDirection);
    hitReactionState_.lastReason = "Damaged";
    SetHitFlashDuration(hitReaction_.flashDuration);
    StartHitFlash();
    if (hitReaction_.interruptAttack)
    {
        bombAttackState_.casting = false;
        bombAttackState_.castTimer = 0.0f;
        bombAttackState_.thrownThisCast = false;
        bombAttackState_.lastReason = "InterruptedByHitReaction";
    }
}

void MidRangeEnemy::UpdateHitReaction(float deltaTime)
{
    if (!hitReactionState_.active)
    {
        return;
    }
    hitReactionState_.timer -= deltaTime;
    Vector3 move = hitReactionState_.knockbackDirection * hitReaction_.knockbackPower * deltaTime;
    move.y += hitReaction_.knockbackUpPower * deltaTime;
    SetCenterPosition(GetCenterPosition() + move);
    if (hitReactionState_.timer <= 0.0f)
    {
        hitReactionState_.active = false;
        hitReactionState_.timer = 0.0f;
        hitReactionState_.lastReason = "Finished";
    }
}

void MidRangeEnemy::StartDeathAnimation(const std::string& reason)
{
    if (deathAnimationState_.active)
    {
        return;
    }
    deathAnimationState_.active = true;
    deathAnimationState_.timer = 0.0f;
    deathAnimationState_.startPosition = GetCenterPosition();
    deathAnimationState_.startRotation = body_.transform.rotate_;
    deathAnimationState_.lastReason = reason;
    animationState_.animState = AnimState::Dead;
    if (deathAnimation_.stopMoveOnDeath)
    {
        SetVelocity({ 0.0f, 0.0f, 0.0f });
    }
}

void MidRangeEnemy::UpdateDeathAnimation(float deltaTime)
{
    if (!deathAnimationState_.active)
    {
        return;
    }
    deathAnimationState_.timer += deltaTime;
    const float t = std::clamp(deathAnimationState_.timer / std::max(deathAnimation_.duration, kEpsilon), 0.0f, 1.0f);
    Vector3 pos = deathAnimationState_.startPosition;
    pos.y -= deathAnimation_.sinkDistance * t;
    SetCenterPosition(pos);
}

bool MidRangeEnemy::IsDeathActive() const
{
    return deathAnimationState_.active;
}

float MidRangeEnemy::CalculateBombInitialSpeed(float distance) const
{
    if (!bombProjectile_.useDistanceBasedSpeed)
    {
        return bombProjectile_.initialSpeed;
    }
    const float distanceOverBase = std::max(0.0f, distance - bombProjectile_.speedBaseDistance);
    const float speed = bombProjectile_.initialSpeed + distanceOverBase * bombProjectile_.speedPerDistance;
    return std::clamp(speed, bombProjectile_.minInitialSpeed, bombProjectile_.maxInitialSpeed);
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
        ImGui::Checkbox("読み込み時にHPを最大に戻す", &basicStats_.resetHpOnLoad);
        ImGui::Text("現在HP: %d", GetHp());
        ImGui::Text("最大HP: %d", GetMaxHp());
        if (ImGui::Button("10ダメージを与える"))
        {
            // 追加: デバッグ用ダメージボタン。
            TakeDamage(10);
        }
        if (ImGui::Button("死亡演出を再生"))
        {
            // 追加: デバッグ用死亡演出ボタン。
            StartDeathAnimation("DebugButton");
        }
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
        ImGui::Checkbox("距離で初速を変える", &bombProjectile_.useDistanceBasedSpeed);
        ImGui::SliderFloat("最小初速", &bombProjectile_.minInitialSpeed, 0.0f, 60.0f);
        ImGui::SliderFloat("最大初速", &bombProjectile_.maxInitialSpeed, 0.0f, 60.0f);
        ImGui::SliderFloat("距離ごとの初速加算", &bombProjectile_.speedPerDistance, 0.0f, 4.0f);
        ImGui::SliderFloat("基準距離", &bombProjectile_.speedBaseDistance, 0.0f, 30.0f);
        ImGui::Text("現在距離から計算した初速: %.2f", CalculateBombInitialSpeed(targetState_.distance));
    }
    if (ImGui::CollapsingHeader("被ダメージリアクション"))
    {
        ImGui::Checkbox("被ダメージリアクションを使う", &hitReaction_.enabled);
        ImGui::SliderFloat("ひるみ時間", &hitReaction_.duration, 0.01f, 2.0f);
        ImGui::SliderFloat("ノックバック力", &hitReaction_.knockbackPower, 0.0f, 8.0f);
        ImGui::SliderFloat("上方向ノックバック", &hitReaction_.knockbackUpPower, 0.0f, 3.0f);
        ImGui::SliderFloat("のけぞり量", &hitReaction_.bodyLean, -1.0f, 1.0f);
        ImGui::SliderFloat("フラッシュ時間", &hitReaction_.flashDuration, 0.01f, 1.0f);
        ImGui::Checkbox("攻撃を中断する", &hitReaction_.interruptAttack);
        ImGui::Checkbox("リアクション中は行動停止", &hitReaction_.stopBehaviorWhileActive);
        ImGui::Text("リアクション中: %s", hitReactionState_.active ? "はい" : "いいえ");
        ImGui::Text("リアクション残り時間: %.2f", hitReactionState_.timer);
        ImGui::Text("最後のリアクション理由: %s", hitReactionState_.lastReason.c_str());
    }
    if (ImGui::CollapsingHeader("死亡演出"))
    {
        ImGui::Checkbox("死亡演出を使う", &deathAnimation_.enabled);
        ImGui::SliderFloat("死亡演出時間", &deathAnimation_.duration, 0.1f, 4.0f);
        ImGui::SliderFloat("倒れる角度X", &deathAnimation_.fallRotateX, -3.14f, 3.14f);
        ImGui::SliderFloat("沈む距離", &deathAnimation_.sinkDistance, 0.0f, 2.0f);
        ImGui::SliderFloat("フェード開始時間", &deathAnimation_.fadeDelay, 0.0f, 3.0f);
        ImGui::SliderFloat("フェード時間", &deathAnimation_.fadeDuration, 0.0f, 3.0f);
        ImGui::Checkbox("死亡時にコリジョン無効", &deathAnimation_.disableCollisionOnDeath);
        ImGui::Checkbox("死亡時に移動停止", &deathAnimation_.stopMoveOnDeath);
        ImGui::Text("死亡演出中: %s", deathAnimationState_.active ? "はい" : "いいえ");
        ImGui::Text("死亡演出時間: %.2f", deathAnimationState_.timer);
        ImGui::Text("最後の死亡理由: %s", deathAnimationState_.lastReason.c_str());
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
        ImGui::SliderFloat("Pitch最小", &headLook_.pitchMinDeg, -180.0f, 180.0f);
        ImGui::SliderFloat("Pitch最大", &headLook_.pitchMaxDeg, -180.0f, 180.0f);
        ImGui::SliderFloat("Pitch符号", &headLook_.pitchSign, -1.0f, 1.0f);
        ImGui::SliderFloat("補間速度", &headLook_.lerpSpeed, 0.1f, 30.0f);
        ImGui::Text("現在Yaw: %.2f", headLookState_.currentYaw);
        ImGui::Text("現在Pitch: %.2f", headLookState_.currentPitch);
        ImGui::Text("目標Yaw: %.2f", headLookState_.targetYaw);
        ImGui::Text("目標Pitch: %.2f", headLookState_.targetPitch);
        ImGui::Text("ターゲットを見る条件OK: %s", headLookState_.targetVisible ? "はい" : "いいえ");
        ImGui::Text("enabled条件: %s", headLookState_.enabledCondition ? "はい" : "いいえ");
        ImGui::Text("targetあり条件: %s", headLookState_.hasTargetCondition ? "はい" : "いいえ");
        ImGui::Text("検知範囲内条件: %s", headLookState_.inDetectCondition ? "はい" : "いいえ");
        ImGui::Text("死亡していない条件: %s", headLookState_.deathCondition ? "はい" : "いいえ");
        ImGui::Text("頭がターゲットを見ているか: %s", headLookState_.targetVisible ? "はい" : "いいえ");
        ImGui::Text("頭向き理由: %s", headLookState_.reason.c_str());
        ImGui::Text("ターゲットとの差分X: %.2f", headLookState_.toTarget.x);
        ImGui::Text("ターゲットとの差分Y: %.2f", headLookState_.toTarget.y);
        ImGui::Text("ターゲットとの差分Z: %.2f", headLookState_.toTarget.z);
        ImGui::Text("水平距離: %.2f", headLookState_.horizontalDistance);
    }
    if (ImGui::CollapsingHeader("状態表示"))
    {
        ImGui::Text("現在行動: %s", behaviorState_.currentBehaviorName.c_str());
        ImGui::Text("最後の理由: %s", behaviorState_.lastReason.c_str());
        ImGui::Text("ターゲット距離: %.2f", targetState_.distance);
        ImGui::Text("現在計算された爆弾初速: %.2f", CalculateBombInitialSpeed(targetState_.distance));
        ImGui::Text("死亡中か: %s", IsDeathActive() ? "はい" : "いいえ");
        ImGui::Text("被ダメージリアクション中か: %s", hitReactionState_.active ? "はい" : "いいえ");
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
    hitReaction_ = HitReactionSettings{};
    deathAnimation_ = DeathAnimationSettings{};
    // 追加: デフォルト復帰後も最大HPをEnemyBaseへ反映する。
    ApplyBasicStatsToEnemyBase();
}

void MidRangeEnemy::ApplyBasicStatsToEnemyBase()
{
    // 追加: JSON/デフォルト反映時に最大HPをEnemyBaseへ同期する。
    SetMaxHp(basicStats_.maxHp);
    if (basicStats_.resetHpOnLoad)
    {
        SetCurrentHp(basicStats_.maxHp);
    }
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
        j["bombProjectile"]["useDistanceBasedSpeed"] = bombProjectile_.useDistanceBasedSpeed;
        j["bombProjectile"]["minInitialSpeed"] = bombProjectile_.minInitialSpeed;
        j["bombProjectile"]["maxInitialSpeed"] = bombProjectile_.maxInitialSpeed;
        j["bombProjectile"]["speedPerDistance"] = bombProjectile_.speedPerDistance;
        j["bombProjectile"]["speedBaseDistance"] = bombProjectile_.speedBaseDistance;
        j["bombProjectile"]["upwardVelocity"] = bombProjectile_.upwardVelocity;
        j["bombProjectile"]["gravity"] = bombProjectile_.gravity;
        j["bombProjectile"]["lifeTime"] = bombProjectile_.lifeTime;
        j["bombProjectile"]["hitRadius"] = bombProjectile_.hitRadius;
        j["bombProjectile"]["explosionRadius"] = bombProjectile_.explosionRadius;
        j["bombProjectile"]["directHitDamage"] = bombProjectile_.directHitDamage;
        j["bombProjectile"]["explosionDamage"] = bombProjectile_.explosionDamage;
        j["bombProjectile"]["directHitAlsoExplosionDamage"] = bombProjectile_.directHitAlsoExplosionDamage;
        j["hitReaction"]["hitReactionEnabled"] = hitReaction_.enabled;
        j["hitReaction"]["hitReactionDuration"] = hitReaction_.duration;
        j["hitReaction"]["hitReactionKnockbackPower"] = hitReaction_.knockbackPower;
        j["hitReaction"]["hitReactionKnockbackUpPower"] = hitReaction_.knockbackUpPower;
        j["hitReaction"]["hitReactionBodyLean"] = hitReaction_.bodyLean;
        j["hitReaction"]["hitReactionFlashDuration"] = hitReaction_.flashDuration;
        j["hitReaction"]["hitReactionInterruptAttack"] = hitReaction_.interruptAttack;
        j["hitReaction"]["hitReactionStopBehaviorWhileActive"] = hitReaction_.stopBehaviorWhileActive;
        j["deathAnimation"]["deathAnimationEnabled"] = deathAnimation_.enabled;
        j["deathAnimation"]["deathAnimationDuration"] = deathAnimation_.duration;
        j["deathAnimation"]["deathAnimationFallRotateX"] = deathAnimation_.fallRotateX;
        j["deathAnimation"]["deathAnimationSinkDistance"] = deathAnimation_.sinkDistance;
        j["deathAnimation"]["deathAnimationFadeDelay"] = deathAnimation_.fadeDelay;
        j["deathAnimation"]["deathAnimationFadeDuration"] = deathAnimation_.fadeDuration;
        j["deathAnimation"]["deathAnimationDisableCollisionOnDeath"] = deathAnimation_.disableCollisionOnDeath;
        j["deathAnimation"]["deathAnimationStopMoveOnDeath"] = deathAnimation_.stopMoveOnDeath;
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
        j["headLook"]["pitchSign"] = headLook_.pitchSign;
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
        // 追加: JSONカテゴリ欠損時も既定値で安全に読み込む。
        const auto basicStatsJson = j.value("basicStats", nlohmann::json::object());
        const auto distanceJson = j.value("distance", nlohmann::json::object());
        const auto moveJson = j.value("move", nlohmann::json::object());
        const auto bombAttackJson = j.value("bombAttack", nlohmann::json::object());
        const auto bombProjectileJson = j.value("bombProjectile", nlohmann::json::object());
        const auto hitReactionJson = j.value("hitReaction", nlohmann::json::object());
        const auto deathAnimationJson = j.value("deathAnimation", nlohmann::json::object());
        const auto pathJson = j.value("path", nlohmann::json::object());
        const auto animationJson = j.value("animation", nlohmann::json::object());
        const auto headLookJson = j.value("headLook", nlohmann::json::object());

        basicStats_.maxHp = basicStatsJson.value("maxHp", basicStats_.maxHp);
        basicStats_.resetHpOnLoad = basicStatsJson.value("resetHpOnLoad", basicStats_.resetHpOnLoad);
        distance_.detectRange = distanceJson.value("detectRange", distance_.detectRange);
        distance_.attackMinRange = distanceJson.value("attackMinRange", distance_.attackMinRange);
        distance_.attackMaxRange = distanceJson.value("attackMaxRange", distance_.attackMaxRange);
        distance_.idealRange = distanceJson.value("idealRange", distance_.idealRange);
        distance_.tooCloseRange = distanceJson.value("tooCloseRange", distance_.tooCloseRange);
        move_.moveSpeed = moveJson.value("moveSpeed", move_.moveSpeed);
        move_.retreatSpeed = moveJson.value("retreatSpeed", move_.retreatSpeed);
        move_.rotateSpeed = moveJson.value("rotateSpeed", move_.rotateSpeed);
        bombAttack_.cooldown = bombAttackJson.value("cooldown", bombAttack_.cooldown);
        bombAttack_.castTime = bombAttackJson.value("castTime", bombAttack_.castTime);
        bombAttack_.throwHeightOffset = bombAttackJson.value("throwHeightOffset", bombAttack_.throwHeightOffset);
        bombProjectile_.initialSpeed = bombProjectileJson.value("initialSpeed", bombProjectile_.initialSpeed);
        bombProjectile_.useDistanceBasedSpeed = bombProjectileJson.value("useDistanceBasedSpeed", bombProjectile_.useDistanceBasedSpeed);
        bombProjectile_.minInitialSpeed = bombProjectileJson.value("minInitialSpeed", bombProjectile_.minInitialSpeed);
        bombProjectile_.maxInitialSpeed = bombProjectileJson.value("maxInitialSpeed", bombProjectile_.maxInitialSpeed);
        bombProjectile_.speedPerDistance = bombProjectileJson.value("speedPerDistance", bombProjectile_.speedPerDistance);
        bombProjectile_.speedBaseDistance = bombProjectileJson.value("speedBaseDistance", bombProjectile_.speedBaseDistance);
        bombProjectile_.upwardVelocity = bombProjectileJson.value("upwardVelocity", bombProjectile_.upwardVelocity);
        bombProjectile_.gravity = bombProjectileJson.value("gravity", bombProjectile_.gravity);
        bombProjectile_.lifeTime = bombProjectileJson.value("lifeTime", bombProjectile_.lifeTime);
        bombProjectile_.hitRadius = bombProjectileJson.value("hitRadius", bombProjectile_.hitRadius);
        bombProjectile_.explosionRadius = bombProjectileJson.value("explosionRadius", bombProjectile_.explosionRadius);
        bombProjectile_.directHitDamage = bombProjectileJson.value("directHitDamage", bombProjectile_.directHitDamage);
        bombProjectile_.explosionDamage = bombProjectileJson.value("explosionDamage", bombProjectile_.explosionDamage);
        bombProjectile_.directHitAlsoExplosionDamage = bombProjectileJson.value("directHitAlsoExplosionDamage", bombProjectile_.directHitAlsoExplosionDamage);
        hitReaction_.enabled = hitReactionJson.value("hitReactionEnabled", hitReaction_.enabled);
        hitReaction_.duration = hitReactionJson.value("hitReactionDuration", hitReaction_.duration);
        hitReaction_.knockbackPower = hitReactionJson.value("hitReactionKnockbackPower", hitReaction_.knockbackPower);
        hitReaction_.knockbackUpPower = hitReactionJson.value("hitReactionKnockbackUpPower", hitReaction_.knockbackUpPower);
        hitReaction_.bodyLean = hitReactionJson.value("hitReactionBodyLean", hitReaction_.bodyLean);
        hitReaction_.flashDuration = hitReactionJson.value("hitReactionFlashDuration", hitReaction_.flashDuration);
        hitReaction_.interruptAttack = hitReactionJson.value("hitReactionInterruptAttack", hitReaction_.interruptAttack);
        hitReaction_.stopBehaviorWhileActive = hitReactionJson.value("hitReactionStopBehaviorWhileActive", hitReaction_.stopBehaviorWhileActive);
        deathAnimation_.enabled = deathAnimationJson.value("deathAnimationEnabled", deathAnimation_.enabled);
        deathAnimation_.duration = deathAnimationJson.value("deathAnimationDuration", deathAnimation_.duration);
        deathAnimation_.fallRotateX = deathAnimationJson.value("deathAnimationFallRotateX", deathAnimation_.fallRotateX);
        deathAnimation_.sinkDistance = deathAnimationJson.value("deathAnimationSinkDistance", deathAnimation_.sinkDistance);
        deathAnimation_.fadeDelay = deathAnimationJson.value("deathAnimationFadeDelay", deathAnimation_.fadeDelay);
        deathAnimation_.fadeDuration = deathAnimationJson.value("deathAnimationFadeDuration", deathAnimation_.fadeDuration);
        deathAnimation_.disableCollisionOnDeath = deathAnimationJson.value("deathAnimationDisableCollisionOnDeath", deathAnimation_.disableCollisionOnDeath);
        deathAnimation_.stopMoveOnDeath = deathAnimationJson.value("deathAnimationStopMoveOnDeath", deathAnimation_.stopMoveOnDeath);
        path_.pathFindEnabled = pathJson.value("pathFindEnabled", path_.pathFindEnabled);
        path_.repathInterval = pathJson.value("repathInterval", path_.repathInterval);
        path_.waypointReachDistance = pathJson.value("waypointReachDistance", path_.waypointReachDistance);
        path_.pathGridSize = pathJson.value("pathGridSize", path_.pathGridSize);
        path_.pathSearchRadius = pathJson.value("pathSearchRadius", path_.pathSearchRadius);
        path_.obstacleExpandRadius = pathJson.value("obstacleExpandRadius", path_.obstacleExpandRadius);
        path_.cornerCuttingDisabled = pathJson.value("cornerCuttingDisabled", path_.cornerCuttingDisabled);
        animation_.walkSwingSpeed = animationJson.value("walkSwingSpeed", animation_.walkSwingSpeed);
        animation_.walkArmAmplitude = animationJson.value("walkArmAmplitude", animation_.walkArmAmplitude);
        animation_.walkLegAmplitude = animationJson.value("walkLegAmplitude", animation_.walkLegAmplitude);
        animation_.castArmPitch = animationJson.value("castArmPitch", animation_.castArmPitch);
        animation_.castArmYaw = animationJson.value("castArmYaw", animation_.castArmYaw);
        animation_.throwArmPitch = animationJson.value("throwArmPitch", animation_.throwArmPitch);
        animation_.bodyCastLean = animationJson.value("bodyCastLean", animation_.bodyCastLean);
        animation_.throwBodyLean = animationJson.value("throwBodyLean", animation_.throwBodyLean);
        animation_.returnSpeed = animationJson.value("returnSpeed", animation_.returnSpeed);
        headLook_.enabled = headLookJson.value("enabled", headLook_.enabled);
        headLook_.yawLimitDeg = headLookJson.value("yawLimitDeg", headLook_.yawLimitDeg);
        headLook_.pitchMinDeg = headLookJson.value("pitchMinDeg", headLook_.pitchMinDeg);
        headLook_.pitchMaxDeg = headLookJson.value("pitchMaxDeg", headLook_.pitchMaxDeg);
        headLook_.pitchSign = headLookJson.value("pitchSign", headLook_.pitchSign);
        headLook_.lerpSpeed = headLookJson.value("lerpSpeed", headLook_.lerpSpeed);
        // 追加: 読み込んだ最大HP設定をEnemyBaseへ反映する。
        ApplyBasicStatsToEnemyBase();

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
