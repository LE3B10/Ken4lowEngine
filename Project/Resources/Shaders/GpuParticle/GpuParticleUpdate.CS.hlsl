#include "GpuParticleData.hlsli" // パーティクルデータ構造体

RWStructuredBuffer<Particle> gParticles : register(u0); // 書き込み可能なパーティクルバッファ
RWStructuredBuffer<int> gFreeListIndex : register(u1); // フリーリストインデックスバッファ
RWStructuredBuffer<uint> gFreeList : register(u2); // フリーリストバッファ

ConstantBuffer<PerFrame> gPerFrame : register(b2); // フレーム情報

/// ---------- タイプパラメータ ---------- ///
float BaseAlpha(uint type)
{
    switch (type)
    {
        case GPU_PARTICLE_TYPE_EXPLOSION_FIRE:
            return 0.90f;
        case GPU_PARTICLE_TYPE_EXPLOSION_SMOKE:
            return 0.45f;
        case GPU_PARTICLE_TYPE_BOSS_RUSH_TRAIL:
            return 0.55f;
        case GPU_PARTICLE_TYPE_SPIN_ATTACK_SLASH:
            return 0.75f;
        case GPU_PARTICLE_TYPE_BULLETTRACER:
            return 0.85f;
        case GPU_PARTICLE_TYPE_IMPACT_DUST:
            return 0.55f;
        case GPU_PARTICLE_TYPE_IMPACT_WOOD:
            return 0.65f;
        case GPU_PARTICLE_TYPE_FOOT_DUST:
            return 0.35f;
        case GPU_PARTICLE_TYPE_ENV_DUST:
            return 0.12f;
        case GPU_PARTICLE_TYPE_PICKUP_GLOW:
            return 0.55f;
        case GPU_PARTICLE_TYPE_SKILL_EFFECT:
            return 0.55f;
        case GPU_PARTICLE_TYPE_BOSS_AURA:
            return 0.25f;
        case GPU_PARTICLE_TYPE_BOSS_DEATH_SOUL:
            return 0.35f;
        case GPU_PARTICLE_TYPE_HEAL:
            return 0.55f;
        default:
            return 1.00f;
    }
}

/// ---------- (1-t)^power 関数 ---------- ///
float AlphaPow(uint type)
{
    switch (type)
    {
        case GPU_PARTICLE_TYPE_MUZZLEFLASH:
            return 2.8f;
        case GPU_PARTICLE_TYPE_HITSPARK:
            return 2.2f;
        case GPU_PARTICLE_TYPE_IMPACT_METAL:
            return 2.4f;
        case GPU_PARTICLE_TYPE_BULLETTRACER:
            return 1.2f;
        case GPU_PARTICLE_TYPE_BOSS_RUSH_TRAIL:
            return 1.6f;
        case GPU_PARTICLE_TYPE_SPIN_ATTACK_SLASH:
            return 1.6f;
        case GPU_PARTICLE_TYPE_EXPLOSION_FIRE:
            return 1.6f;
        case GPU_PARTICLE_TYPE_EXPLOSION_SMOKE:
            return 0.9f;
        case GPU_PARTICLE_TYPE_ENV_DUST:
            return 0.6f;
        case GPU_PARTICLE_TYPE_BOSS_AURA:
            return 0.7f;
        default:
            return 1.2f;
    }
}

// Y加速度（負=重力、正=浮力）
float AccelY(uint type)
{
    switch (type)
    {
        case GPU_PARTICLE_TYPE_BLOOD:
            return -9.8f;
        case GPU_PARTICLE_TYPE_BOSS_DEBRIS_DUST:
            return -3.5f; // 破片砂は少し落ちる
        case GPU_PARTICLE_TYPE_IMPACT_DUST:
            return -2.0f;
        case GPU_PARTICLE_TYPE_IMPACT_WOOD:
            return -3.0f;
        case GPU_PARTICLE_TYPE_EXPLOSION_FIRE:
            return +1.5f; // 火は上に
        case GPU_PARTICLE_TYPE_EXPLOSION_SMOKE:
            return +0.8f;
        case GPU_PARTICLE_TYPE_FOOT_DUST:
            return +0.25f;
        case GPU_PARTICLE_TYPE_HEAL:
            return +0.8f;
        case GPU_PARTICLE_TYPE_PICKUP_GLOW:
            return +0.25f;
        case GPU_PARTICLE_TYPE_BOSS_DEATH_SOUL:
            return +0.6f;
        default:
            return 0.0f;
    }
}

// 速度減衰（大きいほどすぐ止まる）
float Drag(uint type)
{
    switch (type)
    {
        case GPU_PARTICLE_TYPE_ENV_DUST:
            return 0.2f;
        case GPU_PARTICLE_TYPE_EXPLOSION_SMOKE:
            return 1.2f;
        case GPU_PARTICLE_TYPE_IMPACT_DUST:
            return 2.0f;
        case GPU_PARTICLE_TYPE_IMPACT_WOOD:
            return 2.0f;
        case GPU_PARTICLE_TYPE_FOOT_DUST:
            return 2.0f;
        case GPU_PARTICLE_TYPE_BOSS_APPEAR_DUST:
            return 2.5f;
        case GPU_PARTICLE_TYPE_BOSS_DEBRIS_DUST:
            return 2.0f;
        case GPU_PARTICLE_TYPE_BULLETTRACER:
            return 0.0f;
        default:
            return 0.6f;
    }
}

// スケール成長（xyに足す：煙/砂/回復など）
float ScaleGrow(uint type)
{
    switch (type)
    {
        case GPU_PARTICLE_TYPE_EXPLOSION_SMOKE:
            return 0.9f;
        case GPU_PARTICLE_TYPE_IMPACT_DUST:
            return 1.0f;
        case GPU_PARTICLE_TYPE_FOOT_DUST:
            return 0.8f;
        case GPU_PARTICLE_TYPE_BOSS_APPEAR_DUST:
            return 1.2f;
        case GPU_PARTICLE_TYPE_BOSS_DEBRIS_DUST:
            return 0.9f;
        case GPU_PARTICLE_TYPE_HEAL:
            return 0.4f;
        case GPU_PARTICLE_TYPE_PICKUP_GLOW:
            return 0.2f;
        case GPU_PARTICLE_TYPE_BOSS_DEATH_SOUL:
            return 0.4f;
        default:
            return 0.0f;
    }
}

// スケール縮小（火花/マズルなど）
float ScaleShrink(uint type)
{
    switch (type)
    {
        case GPU_PARTICLE_TYPE_MUZZLEFLASH:
            return 10.0f;
        case GPU_PARTICLE_TYPE_HITSPARK:
            return 8.0f;
        case GPU_PARTICLE_TYPE_IMPACT_METAL:
            return 8.0f;
        default:
            return 0.0f;
    }
}

/// ---------- メイン ---------- ///
[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;
    if (particleIndex >= kMaxParticleCount)
        return;

    Particle p = gParticles[particleIndex];

    // 生存判定は「寿命」だけ
    if (p.lifeTime <= 0.0f)
        return;

    float dt = gPerFrame.deltaTime;
    p.currentTime += dt;

    // ----- 死亡 -----
    if (p.currentTime >= p.lifeTime)
    {
        p.color.a = 0.0f;
        p.scale = float3(0, 0, 0);
        p.lifeTime = 0.0f; // 二重push防止
        p.currentTime = 0.0f;

        gParticles[particleIndex] = p;

        // FreeList push
        int oldTop;
        InterlockedAdd(gFreeListIndex[0], 1, oldTop);
        uint newTop = (uint) (oldTop + 1);

        if (newTop < kMaxParticleCount)
        {
            gFreeList[newTop] = particleIndex;
        }
        else
        {
            // full: rollback
            InterlockedAdd(gFreeListIndex[0], -1, oldTop);
        }
        return;
    }

    // ----- 生存中 update -----
    float t = saturate(p.currentTime / max(p.lifeTime, 1e-5f));

    // 速度減衰 + Y加速度
    float drag = Drag(p.type);
    float damp = max(0.0f, 1.0f - drag * dt);
    p.velocity *= damp;
    p.velocity.y += AccelY(p.type) * dt;

    // 位置
    p.translate += p.velocity * dt;

    // スケール変化（GPUスプライト/リボンはxyだけ触る）
    float grow = ScaleGrow(p.type);
    if (grow > 0.0f)
    {
        uint kind = GPUParticle_GetKind(p.billboardMode);
        if (kind == GPU_PARTICLE_KIND_SPRITE || kind == GPU_PARTICLE_KIND_RIBBON)
        {
            p.scale.xy += grow * dt;
        }
        else
        {
            p.scale += grow * dt;
        }
    }

    float shrink = ScaleShrink(p.type);
    if (shrink > 0.0f)
    {
        float s = max(0.0f, 1.0f - shrink * dt);
        p.scale *= s;
    }

    // フェード（タイプごとの指数 + ベースα）
    float a = pow(1.0f - t, AlphaPow(p.type)) * BaseAlpha(p.type);
    p.color.a = saturate(a);

    gParticles[particleIndex] = p;
}