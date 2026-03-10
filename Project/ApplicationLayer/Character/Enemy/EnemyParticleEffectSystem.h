#pragma once

#include <cstdint>
#include <string>

#include "Vector3.h"
#include "GpuParticleType.h"
#include "BillboardMode.h"

namespace K4E = ::Ken4lowEngine;

class EnemyParticleEffectSystem
{
public:
    void Initialize();

    // 被弾位置が取れている場合はこちら
    void SpawnHitEffect(const K4E::Vector3& hitWorldPos);

    // 死亡位置（敵中心足元あたりを想定）
    void SpawnDeathEffect(const K4E::Vector3& deathWorldPos);

private:
    static constexpr uint32_t kHitPoolSize = 3;
    static constexpr uint32_t kDeathPoolSize = 2;

    uint32_t hitPoolIndex_ = 0;
    uint32_t deathPoolIndex_ = 0;
    bool isInitialized_ = false;

private:
    void CreateEmitters();

    void CreateEmitterIfNeeded(
        const std::string& name,
        const std::string& textureFilePath,
        float radius,
        Ken4lowEngine::GpuParticleType spriteType,
        Ken4lowEngine::BillboardMode billboardFlags = Ken4lowEngine::BillboardMode::Camera
    );

    static K4E::Vector3 AddY(const K4E::Vector3& v, float y);
};