#include "EnemyParticleEffectSystem.h"

#include "GpuParticleManager.h"
#include "GpuParticleEmitter.h"

using namespace Ken4lowEngine;

namespace
{
    // ここはあなたの手持ちテクスチャ名に合わせて変更してください
    // 最初は同じテクスチャでもOKです
    constexpr const char* kHitSparkTex = "Effects/white.dds";
    constexpr const char* kBloodTex = "Effects/white.dds";
    constexpr const char* kSmokeTex = "Effects/white.dds";
    constexpr const char* kShockTex = "Effects/white.dds";

    constexpr uint32_t kHitSparkCount = 18;
    constexpr uint32_t kBloodCount = 10;

    constexpr uint32_t kDeathSmokeCount = 36;
    constexpr uint32_t kDeathBloodCount = 20;
    constexpr uint32_t kDeathShockCount = 1;
}

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

    CreateEmitters();
    isInitialized_ = true;
}

void EnemyParticleEffectSystem::CreateEmitters()
{
    // -------------------------
    // 被弾用
    // -------------------------
    for (uint32_t i = 0; i < kHitPoolSize; ++i)
    {
        CreateEmitterIfNeeded(
            "EnemyHitSpark_" + std::to_string(i),
            kHitSparkTex,
            0.08f,
            GpuParticleType::Spark
        );

        CreateEmitterIfNeeded(
            "EnemyBlood_" + std::to_string(i),
            kBloodTex,
            0.10f,
            GpuParticleType::Blood
        );
    }

    // -------------------------
    // 死亡用
    // -------------------------
    for (uint32_t i = 0; i < kDeathPoolSize; ++i)
    {
        CreateEmitterIfNeeded(
            "EnemyDeathSmoke_" + std::to_string(i),
            kSmokeTex,
            0.28f,
            GpuParticleType::DeathBurstCore
        );

        CreateEmitterIfNeeded(
            "EnemyDeathBlood_" + std::to_string(i),
            kBloodTex,
            0.18f,
            GpuParticleType::Blood
        );

        CreateEmitterIfNeeded(
            "EnemyDeathShock_" + std::to_string(i),
            kShockTex,
            0.20f,
            GpuParticleType::Shockwave
        );
    }
}

void EnemyParticleEffectSystem::CreateEmitterIfNeeded(
    const std::string& name,
    const std::string& textureFilePath,
    float radius,
    GpuParticleType spriteType,
    BillboardMode billboardFlags)
{
    auto* pm = GpuParticleManager::GetInstance();
    if (!pm)
    {
        return;
    }

    // すでに存在していたら再作成しない
    if (pm->GetEmitter(name))
    {
        return;
    }

    GpuParticleEmitter::EmitterInfo info{};
    info.textureFilePath = textureFilePath;
    info.radius = radius;
    info.loopCount = 0;
    info.loopFrequency = 0.0f;
    info.drawType = 0;
    info.kind = GpuParticleKind::Sprite;
    info.spriteType = spriteType;
    info.billboardFlags = billboardFlags;

    pm->CreateEmitter(name, info);
}

void EnemyParticleEffectSystem::SpawnHitEffect(const Vector3& hitWorldPos)
{
    auto* pm = GpuParticleManager::GetInstance();
    if (!pm)
    {
        return;
    }

    const uint32_t slot = hitPoolIndex_ % kHitPoolSize;

    // 火花
    if (auto* e = pm->GetEmitter("EnemyHitSpark_" + std::to_string(slot)))
    {
        e->SetPosition(hitWorldPos);
        e->RequestEmit(kHitSparkCount);
    }

    // 血
    if (auto* e = pm->GetEmitter("EnemyBlood_" + std::to_string(slot)))
    {
        e->SetPosition(hitWorldPos);
        e->RequestEmit(kBloodCount);
    }

    ++hitPoolIndex_;
}

void EnemyParticleEffectSystem::SpawnDeathEffect(const Vector3& deathWorldPos)
{
    auto* pm = GpuParticleManager::GetInstance();
    if (!pm)
    {
        return;
    }

    const uint32_t slot = deathPoolIndex_ % kDeathPoolSize;

    const Vector3 centerPos = AddY(deathWorldPos, 0.8f);
    const Vector3 chestPos = AddY(deathWorldPos, 1.0f);
    const Vector3 groundPos = AddY(deathWorldPos, 0.1f);

    // 煙
    if (auto* e = pm->GetEmitter("EnemyDeathSmoke_" + std::to_string(slot)))
    {
        e->SetPosition(centerPos);
        e->RequestEmit(kDeathSmokeCount);
    }

    // 血しぶき
    if (auto* e = pm->GetEmitter("EnemyDeathBlood_" + std::to_string(slot)))
    {
        e->SetPosition(chestPos);
        e->RequestEmit(kDeathBloodCount);
    }

    // 衝撃波
    if (auto* e = pm->GetEmitter("EnemyDeathShock_" + std::to_string(slot)))
    {
        e->SetPosition(groundPos);
        e->RequestEmit(kDeathShockCount);
    }

    ++deathPoolIndex_;
}