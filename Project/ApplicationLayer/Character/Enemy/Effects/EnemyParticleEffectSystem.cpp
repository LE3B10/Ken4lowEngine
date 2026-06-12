#include "EnemyParticleEffectSystem.h"

#include "GpuParticleManager.h"

using namespace Ken4lowEngine;

Vector3 EnemyParticleEffectSystem::AddY(const Vector3& v, float y)
{
    Vector3 out = v;
    out.y += y;
    return out;
}

void EnemyParticleEffectSystem::Initialize()
{
    if (isInitialized_)
    {
        return;
    }

    // Sprite系GPUパーティクルはEffectSystem側に登録済みのRuntime effectNameで再生する。
    // Enemy側ではEmitter名・ParameterManager登録・Json保存対象を直接管理しない。
    isInitialized_ = true;
}

void EnemyParticleEffectSystem::SpawnHitEffect(const Vector3& hitWorldPos)
{
    // 未初期化のエフェクトシステムからプール番号を触らないようにする。
    if (!isInitialized_)
    {
        return;
    }

    auto* effects = EffectSystem::GetInstance();
    effects->Play("EnemyHitSpark", hitWorldPos);
    effects->Play("EnemyBlood", hitWorldPos);
}

void EnemyParticleEffectSystem::SpawnDeathEffect(const Vector3& deathWorldPos)
{
    // 初期化前や終了後相当の状態では共有エミッターへアクセスしない。
    if (!isInitialized_)
    {
        return;
    }

    const Vector3 centerPos = AddY(deathWorldPos, 0.8f);
    const Vector3 chestPos = AddY(deathWorldPos, 1.0f);
    const Vector3 groundPos = AddY(deathWorldPos, 0.1f);

    auto* effects = EffectSystem::GetInstance();
    effects->Play("EnemyDeathSmoke", centerPos);
    effects->Play("EnemyDeathBlood", chestPos);
    effects->Play("EnemyDeathShock", groundPos);
}
