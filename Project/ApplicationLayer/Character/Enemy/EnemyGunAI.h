#pragma once
#include "Vector3.h"
#include <cmath>
#include <cstdint>
#include <random>
#include <algorithm>

namespace K4E = ::Ken4lowEngine;

inline float LenXZ(const K4E::Vector3& v) { return std::sqrt(v.x * v.x + v.z * v.z); }

inline K4E::Vector3 NormalizeXZ(const K4E::Vector3& v)
{
    const float l = LenXZ(v);
    if (l < 1e-6f) return { 0.0f,0.0f,0.0f };
    return { v.x / l, 0.0f, v.z / l };
}

// v を右に90度（XZ）
inline K4E::Vector3 PerpRightXZ(const K4E::Vector3& v) { return { v.z, 0.0f, -v.x }; }

struct EnemyGunAIParams
{
    // 距離制御
    float preferredMinDist = 6.0f;   // 近すぎたら下がる
    float preferredMaxDist = 10.0f;  // 遠すぎたら詰める
    float attackRange = 14.0f;       // 撃ってよい距離

    // 移動速度
    float moveSpeed = 2.8f;
    float strafeSpeed = 2.2f;

    // ★射撃中の移動倍率（バースト中は落とすと“当てに来る”っぽくなる）
    float aimMoveMul = 0.65f;        // Aim中
    float burstMoveMul = 0.25f;      // Burst中（0にすると撃つ時だけ停止）

    // 射撃
    float reactionDelaySec = 0.25f;  // 発見してから撃ち始めるまで
    float aimHoldSec = 0.12f;        // ちょい溜め
    float fireIntervalSec = 0.10f;   // 連射間隔
    float burstCooldownSec = 0.28f;  // バースト間の休み
    int   burstMinShots = 3;
    int   burstMaxShots = 6;

    // リロード
    int   magSize = 18;
    float reloadSec = 1.2f;
    int   reloadWhenAmmoLE = 0;      // 0なら弾切れでリロード

    // 見失い
    float lostToInvestigateSec = 0.35f; // 見失ってこの秒数で「最後に見た地点へ」
};

struct EnemyGunAIInput
{
    float dt = 0.016f;

    K4E::Vector3 selfPos{};
    K4E::Vector3 selfForwardXZ{}; // (sin(yaw),0,cos(yaw)) など（未使用でもOK）
    K4E::Vector3 targetPos{};

    bool canSeeTarget = false;     // FOV+距離
    bool canShootTarget = false;   // 射線OK（LOS）

    float distToTarget = 99999.0f; // ★水平距離(XZ)を入れるのを推奨

    // 見失い用
    K4E::Vector3 lastSeenPos{};
    float timeSinceSeen = 99999.0f;
};

struct EnemyGunAIOutput
{
    // Move
    K4E::Vector3 moveDirXZ{};
    float moveSpeed = 0.0f;

    // Look
    bool  wantLookAt = false;
    K4E::Vector3 lookAt{};

    // Fire / Reload
    bool wantFire = false;
    bool wantReload = false;
};

enum class EnemyGunPhase : uint8_t
{
    Approach,
    Strafe,
    Aim,
    Burst,
    Reload,
    InvestigateLike
};

struct EnemyGunAIState
{
    EnemyGunPhase phase = EnemyGunPhase::Approach;

    float acquiredSec = 0.0f;     // 見えてからの経過
    float phaseSec = 0.0f;        // AimHold / Cooldown 用

    float shotAccSec = 0.0f;
    int shotsLeftInBurst = 0;

    float strafeSwitchSec = 0.0f;
    float strafeSign = 1.0f;

    float reloadAccSec = 0.0f;

    int ammoInMag = -1; // 初回に magSize で初期化
    std::mt19937 rng{ 12345u };
};

inline int RandRange(std::mt19937& rng, int a, int b)
{
    std::uniform_int_distribution<int> d(a, b);
    return d(rng);
}

inline void EnemyGunAI_Update(
    EnemyGunAIState& st,
    const EnemyGunAIParams& p,
    const EnemyGunAIInput& in,
    EnemyGunAIOutput& out)
{
    out = {};

    if (st.ammoInMag < 0) st.ammoInMag = p.magSize;

    // 常にターゲットへ向く（最低限）
    out.wantLookAt = true;
    out.lookAt = in.targetPos;

    // 見えてる時の“反応時間”
    if (in.canSeeTarget) st.acquiredSec += in.dt;
    else st.acquiredSec = 0.0f;

    // -------------------- Reload --------------------
    if (st.phase == EnemyGunPhase::Reload)
    {
        st.reloadAccSec += in.dt;
        out.wantReload = true;

        if (st.reloadAccSec >= p.reloadSec)
        {
            st.reloadAccSec = 0.0f;
            st.ammoInMag = p.magSize;
            st.phase = EnemyGunPhase::Aim;
            st.phaseSec = 0.0f;
        }
        return;
    }

    // -------------------- InvestigateLike --------------------
    if (!in.canSeeTarget && in.timeSinceSeen < p.lostToInvestigateSec)
    {
        st.phase = EnemyGunPhase::InvestigateLike;
    }
    if (st.phase == EnemyGunPhase::InvestigateLike)
    {
        const K4E::Vector3 to = in.lastSeenPos - in.selfPos;
        const K4E::Vector3 dir = NormalizeXZ(to);
        out.moveDirXZ = dir;
        out.moveSpeed = p.moveSpeed;
        return;
    }

    // -------------------- Move (距離管理/ストレイフ) --------------------
    const K4E::Vector3 toTarget = in.targetPos - in.selfPos;
    const K4E::Vector3 dirTo = NormalizeXZ(toTarget);

    // movement は毎フレーム出す（ただし phase は Aim/Burst を壊さない）
    {
        if (in.distToTarget > p.preferredMaxDist)
        {
            out.moveDirXZ = dirTo;
            out.moveSpeed = p.moveSpeed;
            if (st.phase != EnemyGunPhase::Aim && st.phase != EnemyGunPhase::Burst)
                st.phase = EnemyGunPhase::Approach;
        }
        else if (in.distToTarget < p.preferredMinDist)
        {
            out.moveDirXZ = dirTo * -1.0f;
            out.moveSpeed = p.moveSpeed;
            if (st.phase != EnemyGunPhase::Aim && st.phase != EnemyGunPhase::Burst)
                st.phase = EnemyGunPhase::Approach;
        }
        else
        {
            // 適正距離：ストレイフ
            if (st.phase != EnemyGunPhase::Burst) // ★Burst中はスイッチを止めて射撃を安定させる
            {
                st.strafeSwitchSec += in.dt;
                if (st.strafeSwitchSec >= 0.8f)
                {
                    st.strafeSwitchSec = 0.0f;
                    st.strafeSign *= -1.0f;
                }
            }
            K4E::Vector3 right = NormalizeXZ(PerpRightXZ(dirTo));
            out.moveDirXZ = right * st.strafeSign;
            out.moveSpeed = p.strafeSpeed;
            if (st.phase != EnemyGunPhase::Aim && st.phase != EnemyGunPhase::Burst)
                st.phase = EnemyGunPhase::Strafe;
        }

        // ★射撃中は移動速度を落とす
        if (st.phase == EnemyGunPhase::Aim)
            out.moveSpeed *= p.aimMoveMul;
        else if (st.phase == EnemyGunPhase::Burst)
            out.moveSpeed *= p.burstMoveMul;
    }

    // -------------------- Fire condition --------------------
    const bool canStartFire =
        in.canSeeTarget &&
        in.canShootTarget &&
        (in.distToTarget <= p.attackRange) &&
        (st.acquiredSec >= p.reactionDelaySec);

    // 弾が少ないならリロード
    if (st.ammoInMag <= p.reloadWhenAmmoLE)
    {
        st.phase = EnemyGunPhase::Reload;
        st.reloadAccSec = 0.0f;
        out.wantReload = true;
        return;
    }

    // 射撃できないならここまで（Burst中でも射線が切れたら解除）
    if (!canStartFire)
    {
        if (st.phase == EnemyGunPhase::Burst)
        {
            st.phase = EnemyGunPhase::Aim;
            st.phaseSec = 0.0f;
        }
        return;
    }

    // -------------------- Shoot phase machine --------------------
    st.phaseSec += in.dt;

    // Aim(溜め) / Cooldown 中
    if (st.phase != EnemyGunPhase::Burst)
    {
        st.phase = EnemyGunPhase::Aim;

        if (st.phaseSec < p.aimHoldSec)
        {
            return;
        }

        // Aim完了 → Burst開始
        st.phase = EnemyGunPhase::Burst;
        st.phaseSec = 0.0f;
        st.shotAccSec = 0.0f;
        st.shotsLeftInBurst = RandRange(st.rng, p.burstMinShots, p.burstMaxShots);
    }

    // Burst
    if (st.phase == EnemyGunPhase::Burst)
    {
        st.shotAccSec += in.dt;
        if (st.shotAccSec >= p.fireIntervalSec)
        {
            st.shotAccSec = 0.0f;

            if (st.ammoInMag > 0)
            {
                out.wantFire = true;
                st.ammoInMag--;
            }

            st.shotsLeftInBurst--;
            if (st.shotsLeftInBurst <= 0)
            {
                st.phase = EnemyGunPhase::Aim;
                st.phaseSec = -p.burstCooldownSec; // ★クールダウン
            }
        }
    }
}
