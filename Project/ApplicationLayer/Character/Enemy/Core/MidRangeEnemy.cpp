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

    float ClampWithSortedRange(
        float value,
        float minValue,
        float maxValue
    )
    {
        if (minValue > maxValue)
        {
            std::swap(minValue, maxValue);
        }
        return std::clamp(value, minValue, maxValue);
    }
}

void MidRangeEnemy::Initialize()
{
    EnemyBase::Initialize();
    // 追加: 初期設定の最大HPをEnemyBaseへ反映する。
    ApplyBasicStatsToEnemyBase();
    LoadTuningFromJson(tuningIo_.jsonPath, &tuningIo_.lastLoadResult);
    // 追加: 初期配置を徘徊基準地点として保存する。
    wanderState_.spawnPosition = GetCenterPosition();
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

void MidRangeEnemy::MoveAlongPath(float deltaTime, float moveSpeed)
{
    if (!path_.pathFindEnabled)
    {
        pathState_.pathFound = false;
        pathState_.lastRepathReason = "PathDisabled";
        pathState_.failureReason = "PathFindDisabled";
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

    Vector3 waypoint = pathState_.destination;
    const Vector3 pos = GetCenterPosition();
    const bool hasWaypoint = navigator_.GetNextWaypoint(pos, pathState_.destination, pos.y, deltaTime, waypoint);

    pathState_.pathFound = hasWaypoint;
    pathState_.currentWaypoint = waypoint;
    pathState_.lineBlocked = !hasWaypoint;
    // 追加: 経路未取得時は壁抜け防止のため直進フォールバックを無効化する。
    pathState_.lastRepathReason = hasWaypoint ? "WaypointAcquired" : "NoWaypointStop";
    pathState_.failureReason = hasWaypoint ? "None" : "PathNotFound";
    if (!hasWaypoint)
    {
        animationState_.moveDirection = { 0.0f, 0.0f, 0.0f };
        return;
    }
    const Vector3 moveDir = NormalizeXZ(waypoint - pos);
    animationState_.moveDirection = moveDir;
    SetCenterPosition(pos + moveDir * moveSpeed * deltaTime);
}

void MidRangeEnemy::Update(float deltaTime)
{
    // 追加: まず基礎更新を行う。
    EnemyBase::Update(deltaTime);
    // 追加: 時限爆弾モード中のHP0は通常死亡より自爆を優先する。
    if (!IsDead() && !IsDeathActive() && GetHp() <= 0)
    {
        if (suicideBombState_.active && !suicideBombState_.exploded && suicideBomb_.delayDeathAnimationUntilExplosion)
        {
            ExplodeSuicideBomb("HpZeroSuicideBomb");
            return;
        }
        if (!suicideBombState_.exploded)
        {
            StartDeathAnimation("HpZero");
        }
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
    if (suicideBombState_.explosionDrawTimer > 0.0f)
    {
        // 追加: 爆発デバッグ描画の残り時間を更新する。
        suicideBombState_.explosionDrawTimer = std::max(0.0f, suicideBombState_.explosionDrawTimer - deltaTime);
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
    // 追加: 死亡中はEnemyBaseの分裂死亡更新を優先し、爆発表示だけ継続する。
    if (IsDead())
    {
        UpdateDeathAnimation(deltaTime);
        return;
    }

    // 追加: 非分裂死亡演出中は専用更新のみ継続する。
    if (IsDeathActive())
    {
        UpdateDeathAnimation(deltaTime);
        UpdateVisualAnimation(deltaTime);
        return;
    }
    // 追加: HP低下時の時限爆弾モード移行を通常行動より先に判定する。
    if (suicideBomb_.enabled
        && !suicideBombState_.active
        && !suicideBombState_.exploded
        && GetMaxHp() > 0
        && GetHpRate() <= suicideBomb_.triggerHpRate
        && !IsDeathActive())
    {
        StartSuicideBombMode();
    }
    if (suicideBombState_.active)
    {
        // 追加: 時限爆弾モード中は通常AIより優先して更新する。
        UpdateSuicideBombMode(deltaTime);
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

    // 追加: 通常行動用にターゲット状態を共通関数で更新する。
    UpdateTargetState();
    if (targetState_.inDetectRange)
    {
        // 追加: 検知復帰時は徘徊状態を即座に中断する。
        wanderState_.active = false;
        wanderState_.waiting = false;
        wanderState_.hasPoint = false;
        UpdateCombatBehavior(deltaTime);
    }
    else
    {
        // 追加: 検知範囲外では徘徊行動へ遷移する。
        UpdateWanderBehavior(deltaTime);
    }

    if (bombAttackState_.casting && targetState_.inDetectRange)
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

    if (bombAttackState_.throwAnimTimer > 0.0f && !bombAttackState_.casting)
    {
        animationState_.animState = AnimState::Throw;
    }

    UpdateVisualAnimation(deltaTime);
}

void MidRangeEnemy::UpdateCombatBehavior(float deltaTime)
{
    if (!HasTarget())
    {
        behaviorState_.currentBehaviorName = "Idle";
        behaviorState_.lastReason = "ターゲットなし";
        animationState_.animState = AnimState::Idle;
        return;
    }
    pathState_.destination = targetState_.position;
    if (!targetState_.inAttackRange)
    {
        MoveAlongPath(deltaTime, move_.moveSpeed);
        FaceToMoveDirection(animationState_.moveDirection, deltaTime);
        animationState_.animState = AnimState::Walk;
        behaviorState_.currentBehaviorName = "Approach";
        behaviorState_.lastReason = "経路接近";
        return;
    }
    if (targetState_.tooClose)
    {
        const Vector3 retreat = Vector3{ -targetState_.direction.x, 0.0f, -targetState_.direction.z };
        animationState_.moveDirection = retreat;
        SetCenterPosition(GetCenterPosition() + retreat * move_.retreatSpeed * deltaTime);
        FaceToMoveDirection(retreat, deltaTime);
        animationState_.animState = AnimState::Walk;
        behaviorState_.currentBehaviorName = "Retreat";
        behaviorState_.lastReason = "後退";
        return;
    }
    FaceToTarget(deltaTime);
    animationState_.animState = AnimState::Idle;
    behaviorState_.currentBehaviorName = "AttackReady";
    behaviorState_.lastReason = "攻撃距離内";
    if (!bombAttackState_.casting && bombAttackState_.cooldownTimer <= 0.0f)
    {
        // 追加: 通常戦闘でのみ爆弾構えを開始する。
        bombAttackState_.casting = true;
        bombAttackState_.castTimer = 0.0f;
        bombAttackState_.thrownThisCast = false;
        animationState_.animState = AnimState::Cast;
        behaviorState_.lastReason = "構え開始";
        bombAttackState_.lastReason = "CastStart";
    }
}

void MidRangeEnemy::UpdateWanderBehavior(float deltaTime)
{
    if (!wander_.enabled)
    {
        behaviorState_.currentBehaviorName = "Idle";
        behaviorState_.lastReason = "徘徊無効";
        animationState_.animState = AnimState::Idle;
        return;
    }
    wanderState_.active = true;
    wanderState_.timer -= deltaTime;
    if (wanderState_.waiting)
    {
        wanderState_.waitTimer = std::max(0.0f, wanderState_.waitTimer - deltaTime);
        behaviorState_.currentBehaviorName = "WanderWait";
        behaviorState_.lastReason = "徘徊待機";
        animationState_.animState = AnimState::Idle;
        if (wanderState_.waitTimer <= 0.0f)
        {
            wanderState_.waiting = false;
        }
        return;
    }
    if (!wanderState_.hasPoint && wanderState_.timer <= 0.0f)
    {
        TrySelectWanderPoint("徘徊地点選択");
    }
    if (!wanderState_.hasPoint)
    {
        animationState_.animState = AnimState::Idle;
        return;
    }
    MoveAlongPath(deltaTime, wander_.moveSpeed);
    FaceToMoveDirection(animationState_.moveDirection, deltaTime);
    animationState_.animState = AnimState::Walk;
    behaviorState_.currentBehaviorName = "WanderMove";
    behaviorState_.lastReason = wanderState_.lastReason;
    if (LengthXZ(GetCenterPosition() - wanderState_.currentPoint) <= wander_.pointReachDistance)
    {
        // 追加: 徘徊地点に到達したら待機に切り替える。
        wanderState_.waiting = true;
        wanderState_.waitTimer = wander_.waitTime;
        wanderState_.hasPoint = false;
        behaviorState_.currentBehaviorName = "WanderWait";
    }
}

bool MidRangeEnemy::RequestPathTo(const Vector3& destination, const std::string& reason)
{
    // 追加: 経路探索先をターゲット以外にも切り替えられるようにする。
    pathState_.destination = destination;
    pathState_.lastRepathReason = reason;
    return true;
}

bool MidRangeEnemy::TrySelectWanderPoint(const std::string& reason)
{
    const Vector3 currentPos = GetCenterPosition();
    const float distanceFromSpawn = LengthXZ(currentPos - wanderState_.spawnPosition);

    if (wander_.returnToSpawnWhenFar && distanceFromSpawn > wander_.maxDistanceFromSpawn)
    {
        RequestPathTo(wanderState_.spawnPosition, "Spawnから離れすぎたため帰還");

        wanderState_.currentPoint = wanderState_.spawnPosition;
        wanderState_.hasPoint = true;
        wanderState_.waiting = false;
        wanderState_.timer = wander_.interval;
        wanderState_.retryCount = 0;
        wanderState_.lastReason = "Spawnから離れすぎたため帰還";

        return true;
    }

    const int retryCount = std::max(1, wander_.maxRetryCount);
    const int selectedIndex = std::rand() % retryCount;
    const float angle = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * kTwoPi;
    const float dist = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * wander_.radius;

    Vector3 candidate = wanderState_.spawnPosition;
    candidate.x += std::cos(angle) * dist;
    candidate.z += std::sin(angle) * dist;
    candidate.y = currentPos.y;

    // 追加: RequestPathToは現在失敗判定を持たないため、到達不能分岐を作らず候補を採用する。
    RequestPathTo(candidate, reason);

    wanderState_.currentPoint = candidate;
    wanderState_.hasPoint = true;
    wanderState_.waiting = false;
    wanderState_.timer = wander_.interval;
    wanderState_.retryCount = selectedIndex + 1;
    wanderState_.lastReason = reason;

    return true;
}

void MidRangeEnemy::TakeDamage(int amount)
{
    if (suicideBombState_.active && suicideBomb_.invincibleWhileActive)
    {
        // 追加: 時限爆弾モード中は無敵化する。
        hitReactionState_.lastReason = "SuicideBombInvincible";
        return;
    }
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
    if (suicideBombState_.active && suicideBomb_.invincibleWhileActive)
    {
        // 追加: 時限爆弾モード中は無敵化する。
        hitReactionState_.lastReason = "SuicideBombInvincible";
        return;
    }
    // 追加: 被弾方向がある場合は逆方向へノックバックする。
    EnemyBase::TakeDamage(amount, hitDir, hitPower);
    StartHitReaction(hitDir * -1.0f);
    if (GetHp() <= 0)
    {
        StartDeathAnimation("DamagedWithDir");
    }
}

void MidRangeEnemy::StartSuicideBombMode()
{
    // 追加: 時限爆弾モードの開始状態を設定する。
    suicideBombState_.active = true;
    suicideBombState_.exploded = false;
    suicideBombState_.timer = suicideBomb_.timeLimit;
    suicideBombState_.deathDelayTimer = 0.0f;
    suicideBombState_.blinkTimer = 0.0f;
    suicideBombState_.lastReason = "HP低下で時限爆弾モード";
    behaviorState_.currentBehaviorName = "SuicideBomb";
    behaviorState_.lastReason = "HP低下で時限爆弾モード";
    bombAttackState_.casting = false;
    bombAttackState_.castTimer = 0.0f;
    bombAttackState_.thrownThisCast = false;
    animationState_.animState = AnimState::Walk;
}

void MidRangeEnemy::UpdateSuicideBombMode(float deltaTime)
{
    if (!suicideBombState_.active)
    {
        return;
    }
    // 追加: 時限爆弾モードのタイマーを毎フレーム更新する。
    suicideBombState_.timer = std::max(0.0f, suicideBombState_.timer - deltaTime);
    // 追加: 時間切れ自爆を最優先で処理する。
    if (suicideBombState_.timer <= 0.0f)
    {
        ExplodeSuicideBomb("TimeLimit");
        return;
    }
    // 追加: 時限爆弾モード中の点滅タイマーを進める。
    suicideBombState_.blinkTimer += deltaTime;
    // 追加: 通常行動と共通のターゲット状態更新を使用する。
    UpdateTargetState();
    if (HasTarget())
    {
        const Vector3 dir = targetState_.direction;
        const float distanceToTarget = targetState_.distance;
        if (distanceToTarget <= suicideBomb_.explodeDistance)
        {
            ExplodeSuicideBomb("NearTarget");
            return;
        }
        // 追加: 時限爆弾モード中も経路探索移動を使って壁抜けを防止する。
        MoveAlongPath(deltaTime, suicideBomb_.chaseSpeed);
    }
    const float originalRotateSpeed = move_.rotateSpeed;
    move_.rotateSpeed = suicideBomb_.rotateSpeed;
    FaceToMoveDirection(animationState_.moveDirection, deltaTime);
    move_.rotateSpeed = originalRotateSpeed;
    animationState_.animState = AnimState::Walk;
    behaviorState_.currentBehaviorName = "SuicideBomb";
    behaviorState_.lastReason = "時限爆弾追跡中";
}

void MidRangeEnemy::ExplodeSuicideBomb(const std::string& reason)
{
    if (suicideBombState_.exploded)
    {
        return;
    }
    // 追加: 爆発位置を地面下に埋まらないよう補正する。
    Vector3 explosionPos = GetCenterPosition();
    if (explosionPos.y < suicideBomb_.explosionPositionMinY)
    {
        explosionPos.y = suicideBomb_.explosionPositionMinY;
    }
    // 追加: 自爆の見た目を死亡演出より先に確定する。
    // 追加: 設定値が0でも爆発描画が見えるよう最短表示時間を保証する。
    const float drawTime = std::max(suicideBomb_.explosionDebugDrawTime, 0.35f);
    suicideBombState_.explosionPosition = explosionPos;
    suicideBombState_.explosionDrawTimer = drawTime;
    suicideBombState_.deathDelayTimer = 0.0f;
    suicideBombState_.active = false;
    suicideBombState_.exploded = true;
    suicideBombState_.lastReason = reason;
    behaviorState_.currentBehaviorName = "SuicideBombExploded";
    behaviorState_.lastReason = reason;
    // 追加: 自爆時点のターゲット情報を共通関数で更新する。
    UpdateTargetState();
    if (HasTarget())
    {
        const float distanceToTarget = LengthXZ(targetState_.position - suicideBombState_.explosionPosition);
        if (distanceToTarget <= suicideBomb_.explosionRadius)
        {
            // 追加: TODO 既存のプレイヤーダメージ接続先が確定したら範囲ダメージを適用する。
            behaviorState_.lastReason = "自爆範囲内";
        }
    }
    // 追加: 爆発直後は視認しやすい爆発色を反映する。
    SetColor({ 1.0f, 0.2f, 0.0f, 1.0f });
    // 追加: 自爆死はEnemyBaseの分裂死亡処理を必ず通す。
    KillBySuicideExplosion();
}

K4E::Vector3 MidRangeEnemy::CalculateSuicideBreakApartDirection() const
{
    // 追加: 自爆分裂方向の既定値を前方にする。
    Vector3 breakApartDirection = { 0.0f, 0.0f, 1.0f };
    if (suicideBomb_.useTargetDirectionForBreakApart && targetState_.hasTarget)
    {
        breakApartDirection = NormalizeXZ(targetState_.position - GetCenterPosition());
    }
    if (LengthXZ(breakApartDirection) <= kEpsilon)
    {
        breakApartDirection = { 0.0f, 0.0f, 1.0f };
    }
    return breakApartDirection;
}

void MidRangeEnemy::KillBySuicideExplosion()
{
    if (IsDead())
    {
        return;
    }
    // 追加: 自爆死は派生の無敵判定を回避するため基底のTakeDamageを直接呼ぶ。
    const int lethalDamage = std::max(GetHp(), GetMaxHp());
    Vector3 breakApartDirection = CalculateSuicideBreakApartDirection();
    lastSuicideBreakApartDirection_ = breakApartDirection;
    usedEnemyBaseDeathForSuicide_ = true;
    EnemyBase::TakeDamage(lethalDamage, breakApartDirection, suicideBomb_.breakApartPower);
}

void MidRangeEnemy::UpdateVisualAnimation(float deltaTime)
{
    if (IsDead() || suicideBombState_.exploded)
    {
        // 追加: 自爆後の分裂パーツ姿勢を通常アニメで上書きしない。
        return;
    }
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
    if (suicideBombState_.active && suicideBomb_.blinkEnabled)
    {
        // 追加: 時限爆弾モード中は本体色を点滅させる。
        const float blink = std::sin(suicideBombState_.blinkTimer * suicideBomb_.blinkSpeed);
        if (blink >= 0.0f)
        {
            SetColor(suicideBomb_.blinkColorA);
        }
        else
        {
            SetColor(suicideBomb_.blinkColorB);
        }
    }
    else if (!hitReactionState_.active)
    {
        // 追加: 非点滅時は通常色へ戻す。
        SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    }

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
        headLookState_.targetYaw = ClampWithSortedRange(
            ToDeg(yawDelta),
            -headLook_.yawLimitDeg,
            headLook_.yawLimitDeg
        );

        // 追加: 生の水平距離を使ってPitchを計算し、上下追従を正しく反映する。
        const float rawPitchDeg = -ToDeg(std::atan2(toTarget.y, headLookState_.horizontalDistance));
        const float signedPitchDeg = rawPitchDeg * headLook_.pitchSign;
        // 追加: Pitch範囲が逆転していても安全にClampできるようにする。
        headLookState_.targetPitch = ClampWithSortedRange(
            signedPitchDeg,
            headLook_.pitchMinDeg,
            headLook_.pitchMaxDeg
        );
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
    if (wander_.debugDrawEnabled)
    {
        // 追加: 徘徊デバッグとして初期位置と目的地を表示する。
        Wireframe::GetInstance()->DrawSphere(wanderState_.spawnPosition, 0.3f, { 0.1f, 1.0f, 0.1f, 1.0f });
        Wireframe::GetInstance()->DrawSphere(wanderState_.currentPoint, 0.3f, { 1.0f, 0.4f, 0.1f, 1.0f });
        Wireframe::GetInstance()->DrawLine(wanderState_.spawnPosition, wanderState_.currentPoint, { 0.3f, 1.0f, 0.3f, 1.0f });
    }

    if (targetState_.hasTarget)
    {
        Wireframe::GetInstance()->DrawLine(GetCenterPosition(), targetState_.position, { 1.0f, 0.8f, 0.2f, 1.0f });
    }
    if (suicideBombState_.active)
    {
        // 自爆攻撃の当たり判定確認用ワイヤーは、パーティクルとは独立して維持する。
        Wireframe::GetInstance()->DrawSphere(GetCenterPosition(), suicideBomb_.explosionRadius, { 1.0f, 0.1f, 0.1f, 0.35f });
        Wireframe::GetInstance()->DrawLine(GetCenterPosition(), targetState_.position, { 1.0f, 0.1f, 0.1f, 1.0f });
    }
    if (suicideBombState_.explosionDrawTimer > 0.0f)
    {
        // 自爆直後は爆発地点に範囲表示を残す。
        Vector3 drawPos = suicideBombState_.explosionPosition;
        if (drawPos.y < suicideBomb_.explosionPositionMinY)
        {
            drawPos.y = suicideBomb_.explosionPositionMinY;
        }
        Wireframe::GetInstance()->DrawSphere(drawPos, suicideBomb_.explosionRadius, { 1.0f, 0.0f, 0.0f, 0.9f });
        Wireframe::GetInstance()->DrawSphere(drawPos, suicideBomb_.explosionRadius * 0.5f, { 1.0f, 0.8f, 0.0f, 0.9f });
    }

    const Vector3 faceEnd = GetCenterPosition() + animationState_.faceDirection * 2.0f;
    Wireframe::GetInstance()->DrawLine(GetCenterPosition(), faceEnd, { 0.7f, 1.0f, 0.2f, 1.0f });
}

void MidRangeEnemy::DrawImGui()
{
#ifdef USE_IMGUI
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
        // 追加: 保存先パスを常時表示してI/O不整合を可視化する。
        ImGui::Text("保存先: %s", tuningIo_.jsonPath.string().c_str());
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
    if (ImGui::CollapsingHeader("徘徊"))
    {
        ImGui::Checkbox("徘徊を使う", &wander_.enabled);
        ImGui::SliderFloat("徘徊半径", &wander_.radius, 0.1f, 40.0f);
        ImGui::SliderFloat("徘徊間隔", &wander_.interval, 0.1f, 10.0f);
        ImGui::SliderFloat("徘徊移動速度", &wander_.moveSpeed, 0.0f, 10.0f);
        ImGui::SliderFloat("徘徊待機時間", &wander_.waitTime, 0.0f, 10.0f);
        ImGui::SliderFloat("徘徊地点到達距離", &wander_.pointReachDistance, 0.1f, 5.0f);
        ImGui::SliderInt("徘徊地点再試行回数", &wander_.maxRetryCount, 1, 32);
        ImGui::Checkbox("離れすぎたら初期位置へ戻る", &wander_.returnToSpawnWhenFar);
        ImGui::SliderFloat("初期位置からの最大距離", &wander_.maxDistanceFromSpawn, 1.0f, 60.0f);
        ImGui::Checkbox("徘徊デバッグ描画", &wander_.debugDrawEnabled);
    }
    if (ImGui::CollapsingHeader("爆弾攻撃"))
    {
        ImGui::SliderFloat("クールダウン", &bombAttack_.cooldown, 0.0f, 10.0f);
        ImGui::SliderFloat("構え時間", &bombAttack_.castTime, 0.05f, 3.0f);
        ImGui::SliderFloat("投擲高さ", &bombAttack_.throwHeightOffset, 0.0f, 5.0f);
    }
    if (ImGui::CollapsingHeader("時限爆弾モード"))
    {
        ImGui::Checkbox("時限爆弾モードを使う", &suicideBomb_.enabled);
        ImGui::SliderFloat("発動HP割合", &suicideBomb_.triggerHpRate, 0.01f, 1.0f);
        ImGui::Checkbox("発動中は無敵", &suicideBomb_.invincibleWhileActive);
        ImGui::SliderFloat("制限時間", &suicideBomb_.timeLimit, 0.1f, 30.0f);
        ImGui::SliderFloat("自爆追跡速度", &suicideBomb_.chaseSpeed, 0.0f, 20.0f);
        ImGui::SliderFloat("自爆回転速度", &suicideBomb_.rotateSpeed, 0.0f, 30.0f);
        ImGui::SliderFloat("自爆開始距離", &suicideBomb_.explodeDistance, 0.1f, 10.0f);
        ImGui::SliderFloat("自爆爆発範囲", &suicideBomb_.explosionRadius, 0.1f, 20.0f);
        ImGui::SliderInt("自爆ダメージ", &suicideBomb_.explosionDamage, 1, 999);
        ImGui::SliderFloat("爆発範囲表示時間", &suicideBomb_.explosionDebugDrawTime, 0.0f, 3.0f);
        ImGui::Checkbox("発動中は通常爆弾攻撃を止める", &suicideBomb_.stopNormalBombAttack);
        ImGui::Checkbox("点滅を使う", &suicideBomb_.blinkEnabled);
        ImGui::SliderFloat("点滅速度", &suicideBomb_.blinkSpeed, 0.0f, 40.0f);
        ImGui::Checkbox("爆発まで死亡演出を遅らせる", &suicideBomb_.delayDeathAnimationUntilExplosion);
        ImGui::SliderFloat("爆発位置の最低Y", &suicideBomb_.explosionPositionMinY, 0.0f, 5.0f);
        ImGui::SliderFloat("爆発後死亡遅延", &suicideBomb_.deathDelayAfterExplosion, 0.0f, 0.5f);
        ImGui::SliderFloat("自爆分裂威力", &suicideBomb_.breakApartPower, 0.1f, 10.0f);
        ImGui::SliderFloat("自爆上方向威力", &suicideBomb_.breakApartUpPower, 0.0f, 10.0f);
        ImGui::Checkbox("ターゲット方向へ分裂", &suicideBomb_.useTargetDirectionForBreakApart);
        ImGui::ColorEdit4("点滅色A", &suicideBomb_.blinkColorA.x);
        ImGui::ColorEdit4("点滅色B", &suicideBomb_.blinkColorB.x);
        ImGui::Text("時限爆弾モード中: %s", suicideBombState_.active ? "はい" : "いいえ");
        ImGui::Text("自爆済み: %s", suicideBombState_.exploded ? "はい" : "いいえ");
        ImGui::Text("残り時間: %.2f", suicideBombState_.timer);
        ImGui::Text("時間切れ条件: %s", suicideBombState_.timer <= 0.0f ? "成立" : "未成立");
        ImGui::Text("距離爆発条件: %s", targetState_.distance <= suicideBomb_.explodeDistance ? "成立" : "未成立");
        ImGui::Text("爆発表示時間設定: %.2f", suicideBomb_.explosionDebugDrawTime);
        ImGui::Text("最後の理由: %s", suicideBombState_.lastReason.c_str());
        ImGui::Text("爆発範囲表示残り: %.2f", suicideBombState_.explosionDrawTimer);
        ImGui::Text("自爆でEnemyBase死亡を使用: %s", usedEnemyBaseDeathForSuicide_ ? "はい" : "いいえ");
        ImGui::Text("IsDead(): %s", IsDead() ? "はい" : "いいえ");
        ImGui::Text("IsRemovable(): %s", IsRemovable() ? "はい" : "いいえ");
        ImGui::Text("自爆分裂方向X: %.2f", lastSuicideBreakApartDirection_.x);
        ImGui::Text("自爆分裂方向Y: %.2f", lastSuicideBreakApartDirection_.y);
        ImGui::Text("自爆分裂方向Z: %.2f", lastSuicideBreakApartDirection_.z);
        ImGui::Text("爆発位置X: %.2f", suicideBombState_.explosionPosition.x);
        ImGui::Text("爆発位置Y: %.2f", suicideBombState_.explosionPosition.y);
        ImGui::Text("爆発位置Z: %.2f", suicideBombState_.explosionPosition.z);
        ImGui::Text("現在HP: %d", GetHp());
        ImGui::Text("点滅中か: %s", (suicideBombState_.active && suicideBomb_.blinkEnabled) ? "はい" : "いいえ");
        ImGui::Text("点滅タイマー: %.2f", suicideBombState_.blinkTimer);
        ImGui::Text("現在HP割合: %.2f", GetHpRate());
        ImGui::Text("死亡演出中か: %s", IsDeathActive() ? "はい" : "いいえ");
        ImGui::Text("HP0か: %s", GetHp() <= 0 ? "はい" : "いいえ");
        ImGui::Text("自爆済みなので死亡演出中か: %s", (suicideBombState_.exploded && IsDeathActive()) ? "はい" : "いいえ");
        ImGui::Text("現在行動: %s", behaviorState_.currentBehaviorName.c_str());
        ImGui::Text("ターゲット距離: %.2f", targetState_.distance);
        ImGui::Text("経路が見つかったか: %s", pathState_.pathFound ? "はい" : "いいえ");
        ImGui::Text("経路失敗理由: %s", pathState_.failureReason.c_str());
        ImGui::Text("現在ウェイポイント: (%.2f, %.2f, %.2f)", pathState_.currentWaypoint.x, pathState_.currentWaypoint.y, pathState_.currentWaypoint.z);
        if (ImGui::Button("時限爆弾モードを強制開始"))
        {
            StartSuicideBombMode();
        }
        if (ImGui::Button("時間切れ爆発を強制実行"))
        {
            ExplodeSuicideBomb("DebugTimeLimit");
        }
        if (ImGui::Button("現在位置で自爆"))
        {
            ExplodeSuicideBomb("DebugCurrentPosition");
        }
        if (ImGui::Button("自爆分裂だけテスト"))
        {
            // 追加: 自爆分裂処理のみを即時確認する。
            KillBySuicideExplosion();
        }
        if (ImGui::Button("爆発表示だけ再生"))
        {
            // 追加: HP変更なしで爆発描画のみを検証できるようにする。
            Vector3 debugExplosionPos = GetCenterPosition();
            if (debugExplosionPos.y < suicideBomb_.explosionPositionMinY)
            {
                debugExplosionPos.y = suicideBomb_.explosionPositionMinY;
            }
            const float drawTime = std::max(suicideBomb_.explosionDebugDrawTime, 0.35f);
            suicideBombState_.explosionPosition = debugExplosionPos;
            suicideBombState_.explosionDrawTimer = drawTime;
            suicideBombState_.lastReason = "DebugExplosionDrawOnly";
        }
    }
    if (ImGui::CollapsingHeader("爆弾Projectile"))
    {
        ImGui::SliderFloat("爆弾初速", &bombProjectile_.initialSpeed, 0.0f, 60.0f);
        ImGui::Checkbox("距離で初速を変える", &bombProjectile_.useDistanceBasedSpeed);
        ImGui::SliderFloat("最小初速", &bombProjectile_.minInitialSpeed, 0.0f, 60.0f);
        ImGui::SliderFloat("最大初速", &bombProjectile_.maxInitialSpeed, 0.0f, 60.0f);
        ImGui::SliderFloat("距離ごとの初速加算", &bombProjectile_.speedPerDistance, 0.0f, 4.0f);
        ImGui::SliderFloat("基準距離", &bombProjectile_.speedBaseDistance, 0.0f, 30.0f);
        ImGui::SliderFloat("上方向速度", &bombProjectile_.upwardVelocity, 0.0f, 30.0f);
        ImGui::SliderFloat("重力", &bombProjectile_.gravity, 0.0f, 40.0f);
        ImGui::SliderFloat("寿命", &bombProjectile_.lifeTime, 0.1f, 15.0f);
        ImGui::SliderFloat("直撃判定半径", &bombProjectile_.hitRadius, 0.1f, 5.0f);
        // 追加: 通常爆弾の爆発半径の調整上限を拡張する。
        ImGui::SliderFloat("爆発半径", &bombProjectile_.explosionRadius, 0.1f, 30.0f);
        ImGui::SliderInt("直撃ダメージ", &bombProjectile_.directHitDamage, 1, 999);
        ImGui::SliderInt("爆発ダメージ", &bombProjectile_.explosionDamage, 1, 999);
        ImGui::Checkbox("直撃時に爆発ダメージも入れる", &bombProjectile_.directHitAlsoExplosionDamage);
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
        // 追加: Pitch符号を2択化して0.0fを防ぐ。
        const char* pitchSignItems[] = { "通常", "反転" };
        int pitchSignIndex = headLook_.pitchSign < 0.0f ? 1 : 0;
        if (ImGui::Combo("Pitch向き", &pitchSignIndex, pitchSignItems, IM_ARRAYSIZE(pitchSignItems)))
        {
            if (pitchSignIndex == 0)
            {
                headLook_.pitchSign = 1.0f;
            }
            else
            {
                headLook_.pitchSign = -1.0f;
            }
        }
        const bool pitchRangeReversed = headLook_.pitchMinDeg > headLook_.pitchMaxDeg;
        const float effectivePitchMin = std::min(headLook_.pitchMinDeg, headLook_.pitchMaxDeg);
        const float effectivePitchMax = std::max(headLook_.pitchMinDeg, headLook_.pitchMaxDeg);
        ImGui::Text("Pitch範囲が逆転中: %s", pitchRangeReversed ? "はい" : "いいえ");
        ImGui::Text("実際に使うPitch最小: %.2f", effectivePitchMin);
        ImGui::Text("実際に使うPitch最大: %.2f", effectivePitchMax);
        if (ImGui::Button("Pitch範囲を整列"))
        {
            // 追加: 逆転したPitch範囲を明示的に整列できるようにする。
            if (headLook_.pitchMinDeg > headLook_.pitchMaxDeg)
            {
                std::swap(headLook_.pitchMinDeg, headLook_.pitchMaxDeg);
            }
        }
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

#endif // USE_IMGUI
}

void MidRangeEnemy::ResetTuningToDefault()
{
    basicStats_ = BasicStatsSettings{};
    distance_ = DistanceSettings{};
    move_ = MoveSettings{};
    bombAttack_ = BombAttackSettings{};
    bombProjectile_ = BombProjectileSettings{};
    path_ = PathSettings{};
    // 追加: 徘徊設定のデフォルト復元を追加する。
    wander_ = WanderSettings{};
    animation_ = AnimationSettings{};
    headLook_ = HeadLookSettings{};
    hitReaction_ = HitReactionSettings{};
    deathAnimation_ = DeathAnimationSettings{};
    suicideBomb_ = SuicideBombSettings{};
    // 追加: デフォルト復帰後も最大HPをEnemyBaseへ反映する。
    ApplyBasicStatsToEnemyBase();
    // 追加: デフォルト復帰後に各値を再検証して破綻を防ぐ。
    ValidateTuningValues();
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

void MidRangeEnemy::ValidateTuningValues()
{
    // 追加: 頭向き設定の範囲・符号を安全な値へ補正する。
    headLook_.yawLimitDeg = std::clamp(headLook_.yawLimitDeg, 0.0f, 180.0f);
    suicideBomb_.triggerHpRate = std::clamp(suicideBomb_.triggerHpRate, 0.01f, 1.0f);
    suicideBomb_.timeLimit = std::max(0.1f, suicideBomb_.timeLimit);
    suicideBomb_.explodeDistance = std::max(0.1f, suicideBomb_.explodeDistance);
    suicideBomb_.explosionRadius = std::max(0.1f, suicideBomb_.explosionRadius);
    suicideBomb_.explosionPositionMinY = std::max(0.0f, suicideBomb_.explosionPositionMinY);
    suicideBomb_.deathDelayAfterExplosion = std::max(0.0f, suicideBomb_.deathDelayAfterExplosion);
    if (std::abs(headLook_.pitchSign) < 0.001f)
    {
        headLook_.pitchSign = 1.0f;
    }
    else if (headLook_.pitchSign < 0.0f)
    {
        headLook_.pitchSign = -1.0f;
    }
    else
    {
        headLook_.pitchSign = 1.0f;
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
        // 追加: 徘徊設定をJSONへ保存する。
        j["wander"]["enabled"] = wander_.enabled;
        j["wander"]["radius"] = wander_.radius;
        j["wander"]["interval"] = wander_.interval;
        j["wander"]["moveSpeed"] = wander_.moveSpeed;
        j["wander"]["waitTime"] = wander_.waitTime;
        j["wander"]["pointReachDistance"] = wander_.pointReachDistance;
        j["wander"]["maxRetryCount"] = wander_.maxRetryCount;
        j["wander"]["returnToSpawnWhenFar"] = wander_.returnToSpawnWhenFar;
        j["wander"]["maxDistanceFromSpawn"] = wander_.maxDistanceFromSpawn;
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
        j["suicideBomb"]["enabled"] = suicideBomb_.enabled;
        j["suicideBomb"]["triggerHpRate"] = suicideBomb_.triggerHpRate;
        j["suicideBomb"]["invincibleWhileActive"] = suicideBomb_.invincibleWhileActive;
        j["suicideBomb"]["timeLimit"] = suicideBomb_.timeLimit;
        j["suicideBomb"]["chaseSpeed"] = suicideBomb_.chaseSpeed;
        j["suicideBomb"]["rotateSpeed"] = suicideBomb_.rotateSpeed;
        j["suicideBomb"]["explodeDistance"] = suicideBomb_.explodeDistance;
        j["suicideBomb"]["explosionRadius"] = suicideBomb_.explosionRadius;
        j["suicideBomb"]["explosionDamage"] = suicideBomb_.explosionDamage;
        j["suicideBomb"]["explosionDebugDrawTime"] = suicideBomb_.explosionDebugDrawTime;
        j["suicideBomb"]["stopNormalBombAttack"] = suicideBomb_.stopNormalBombAttack;
        j["suicideBomb"]["blinkEnabled"] = suicideBomb_.blinkEnabled;
        j["suicideBomb"]["blinkSpeed"] = suicideBomb_.blinkSpeed;
        j["suicideBomb"]["delayDeathAnimationUntilExplosion"] = suicideBomb_.delayDeathAnimationUntilExplosion;
        j["suicideBomb"]["explosionPositionMinY"] = suicideBomb_.explosionPositionMinY;
        j["suicideBomb"]["deathDelayAfterExplosion"] = suicideBomb_.deathDelayAfterExplosion;
        j["suicideBomb"]["breakApartPower"] = suicideBomb_.breakApartPower;
        j["suicideBomb"]["breakApartUpPower"] = suicideBomb_.breakApartUpPower;
        j["suicideBomb"]["useTargetDirectionForBreakApart"] = suicideBomb_.useTargetDirectionForBreakApart;
        j["suicideBomb"]["blinkColorA"] = { suicideBomb_.blinkColorA.x, suicideBomb_.blinkColorA.y, suicideBomb_.blinkColorA.z, suicideBomb_.blinkColorA.w };
        j["suicideBomb"]["blinkColorB"] = { suicideBomb_.blinkColorB.x, suicideBomb_.blinkColorB.y, suicideBomb_.blinkColorB.z, suicideBomb_.blinkColorB.w };

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
        const auto wanderJson = j.value("wander", nlohmann::json::object());
        const auto animationJson = j.value("animation", nlohmann::json::object());
        const auto headLookJson = j.value("headLook", nlohmann::json::object());
        const auto suicideBombJson = j.value("suicideBomb", nlohmann::json::object());

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
        // 追加: wanderカテゴリが無い古いJSONでは既定値を維持する。
        wander_.enabled = wanderJson.value("enabled", wander_.enabled);
        wander_.radius = wanderJson.value("radius", wander_.radius);
        wander_.interval = wanderJson.value("interval", wander_.interval);
        wander_.moveSpeed = wanderJson.value("moveSpeed", wander_.moveSpeed);
        wander_.waitTime = wanderJson.value("waitTime", wander_.waitTime);
        wander_.pointReachDistance = wanderJson.value("pointReachDistance", wander_.pointReachDistance);
        wander_.maxRetryCount = wanderJson.value("maxRetryCount", wander_.maxRetryCount);
        wander_.returnToSpawnWhenFar = wanderJson.value("returnToSpawnWhenFar", wander_.returnToSpawnWhenFar);
        wander_.maxDistanceFromSpawn = wanderJson.value("maxDistanceFromSpawn", wander_.maxDistanceFromSpawn);
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
        suicideBomb_.deathDelayAfterExplosion = suicideBombJson.value("deathDelayAfterExplosion", suicideBomb_.deathDelayAfterExplosion);
        headLook_.pitchMinDeg = headLookJson.value("pitchMinDeg", headLook_.pitchMinDeg);
        headLook_.pitchMaxDeg = headLookJson.value("pitchMaxDeg", headLook_.pitchMaxDeg);
        headLook_.pitchSign = headLookJson.value("pitchSign", headLook_.pitchSign);
        headLook_.lerpSpeed = headLookJson.value("lerpSpeed", headLook_.lerpSpeed);
        suicideBomb_.enabled = suicideBombJson.value("enabled", suicideBomb_.enabled);
        suicideBomb_.triggerHpRate = suicideBombJson.value("triggerHpRate", suicideBomb_.triggerHpRate);
        suicideBomb_.invincibleWhileActive = suicideBombJson.value("invincibleWhileActive", suicideBomb_.invincibleWhileActive);
        suicideBomb_.timeLimit = suicideBombJson.value("timeLimit", suicideBomb_.timeLimit);
        suicideBomb_.chaseSpeed = suicideBombJson.value("chaseSpeed", suicideBomb_.chaseSpeed);
        suicideBomb_.rotateSpeed = suicideBombJson.value("rotateSpeed", suicideBomb_.rotateSpeed);
        suicideBomb_.explodeDistance = suicideBombJson.value("explodeDistance", suicideBomb_.explodeDistance);
        suicideBomb_.explosionRadius = suicideBombJson.value("explosionRadius", suicideBomb_.explosionRadius);
        suicideBomb_.explosionDamage = suicideBombJson.value("explosionDamage", suicideBomb_.explosionDamage);
        suicideBomb_.explosionDebugDrawTime = suicideBombJson.value("explosionDebugDrawTime", suicideBomb_.explosionDebugDrawTime);
        suicideBomb_.stopNormalBombAttack = suicideBombJson.value("stopNormalBombAttack", suicideBomb_.stopNormalBombAttack);
        suicideBomb_.blinkEnabled = suicideBombJson.value("blinkEnabled", suicideBomb_.blinkEnabled);
        suicideBomb_.blinkSpeed = suicideBombJson.value("blinkSpeed", suicideBomb_.blinkSpeed);
        suicideBomb_.delayDeathAnimationUntilExplosion = suicideBombJson.value("delayDeathAnimationUntilExplosion", suicideBomb_.delayDeathAnimationUntilExplosion);
        suicideBomb_.explosionPositionMinY = suicideBombJson.value("explosionPositionMinY", suicideBomb_.explosionPositionMinY);
        suicideBomb_.breakApartPower = suicideBombJson.value("breakApartPower", suicideBomb_.breakApartPower);
        suicideBomb_.breakApartUpPower = suicideBombJson.value("breakApartUpPower", suicideBomb_.breakApartUpPower);
        suicideBomb_.useTargetDirectionForBreakApart = suicideBombJson.value("useTargetDirectionForBreakApart", suicideBomb_.useTargetDirectionForBreakApart);
        if (suicideBombJson.contains("blinkColorA"))
        {
            const auto& color = suicideBombJson["blinkColorA"];
            if (color.is_array() && color.size() == 4)
            {
                suicideBomb_.blinkColorA = { color[0].get<float>(), color[1].get<float>(), color[2].get<float>(), color[3].get<float>() };
            }
        }
        if (suicideBombJson.contains("blinkColorB"))
        {
            const auto& color = suicideBombJson["blinkColorB"];
            if (color.is_array() && color.size() == 4)
            {
                suicideBomb_.blinkColorB = { color[0].get<float>(), color[1].get<float>(), color[2].get<float>(), color[3].get<float>() };
            }
        }
        // 追加: JSON読み込み後に危険値を補正する。
        ValidateTuningValues();
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

void MidRangeEnemy::UpdateTargetState()
{
    // 追加: ターゲット情報更新を通常行動と時限爆弾で共通化する。
    if (!targetState_.hasTarget)
    {
        targetState_.inDetectRange = false;
        targetState_.inAttackRange = false;
        targetState_.tooClose = false;
        targetState_.distance = 0.0f;
        targetState_.direction = { 0.0f, 0.0f, 0.0f };
        return;
    }
    Vector3 toTarget = targetState_.position - GetCenterPosition();
    toTarget.y = 0.0f;
    targetState_.distance = LengthXZ(toTarget);
    targetState_.direction = NormalizeXZ(toTarget);
    targetState_.inDetectRange = IsTargetInDetectRange();
    targetState_.inAttackRange = targetState_.distance <= distance_.attackMaxRange;
    targetState_.tooClose = targetState_.distance < distance_.tooCloseRange;
}
