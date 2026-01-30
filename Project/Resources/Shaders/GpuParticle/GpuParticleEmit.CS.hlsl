// GpuParticleEmit.CS.hlsl
// ------------------------------------------------------------
// Emit CS（整理版）
//  - 既存の Particle / EmitterCBData を壊さずに整理
//  - typeごとの初期化をPreset関数に分離
//  - Ribbon(疑似)は kind=RIBBON + BILLBOARD_VELOCITY で扱う
// ------------------------------------------------------------

#include "GpuParticleData.hlsli"

// UAVs
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);

// CBVs
ConstantBuffer<EmitterCBData> gEmitter : register(b1);
ConstantBuffer<PerFrame> gPerFrame : register(b2);

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
float3 SampleUnitDir(float3 seed)
{
    float3 r = GPURand3(seed) * 2.0f - 1.0f;
    return normalize(r);
}

// 半径内（球）に散らす（見た目用途として十分）
float3 SampleSphere(float3 seed, float radius)
{
    float3 dir = SampleUnitDir(seed);
    float t = GPURand1(seed + 19.19f);
    float r = radius * pow(t, 1.0f / 3.0f);
    return dir * r;
}

float3 SampleHemisphereUp(float3 seed)
{
    float3 d = SampleUnitDir(seed);
    d.y = abs(d.y);
    return normalize(d);
}

uint DecideRenderKind(uint type)
{
    // ★None(0) は Mesh として扱う（Mesh Particle 用）
    if (GPUParticle_GetBillboardFlags(gEmitter.billboardMode) == BILLBOARD_NONE)
    {
        return GPU_PARTICLE_KIND_MESH;
    }

    // Ribbon
    if (type == GPU_PARTICLE_TYPE_BULLETTRACER ||
        type == GPU_PARTICLE_TYPE_BOSS_RUSH_TRAIL)
    {
        return GPU_PARTICLE_KIND_RIBBON;
    }

    return GPU_PARTICLE_KIND_SPRITE;
}

void FinalizeParticle(inout Particle p, uint kind)
{
    p.currentTime = 0.0f;
    p.type = gEmitter.type;

    // CB側のbillboardModeは「低bit=フラグ」として使う想定
    uint bbFlags = GPUParticle_GetBillboardFlags(gEmitter.billboardMode);

    // Ribbonのときは「速度方向に伸ばす」フラグを追加
    if (kind == GPU_PARTICLE_KIND_RIBBON)
    {
        bbFlags |= BILLBOARD_RIBBON;

        // リボンは基本カメラに見えるようにしておく（未指定でも安全）
        bbFlags |= BILLBOARD_CAMERA;
    }

    p.billboardMode = GPUParticle_PackBillboardMode(kind, bbFlags);
}

// ------------------------------------------------------------
// Presets (type別 初期化)
// ------------------------------------------------------------
void Preset_Default(uint i, float3 seed, inout Particle p)
{
    float3 dir = SampleUnitDir(seed);
    float t = frac((float) i / max(gEmitter.count, 1u) + gPerFrame.time * 0.2f);

    float r = saturate(abs(t * 6.0f - 3.0f) - 1.0f);
    float g = saturate(2.0f - abs(t * 6.0f - 2.0f));
    float b = saturate(2.0f - abs(t * 6.0f - 4.0f));

    p.translate = gEmitter.translate + dir * gEmitter.radius;
    p.scale = float3(0.5f, 0.5f, 0.5f);
    p.velocity = dir * 2.0f;
    p.lifeTime = 1.0f;
    p.color = float4(r, g, b, 1.0f);
}

void Preset_HitSpark(uint i, float3 seed, inout Particle p)
{
    float3 dir = SampleHemisphereUp(seed);
    dir = normalize(dir + float3(0.0f, 0.35f, 0.0f));

    float dist = GPURand1(seed + 1.0f) * gEmitter.radius;
    p.translate = gEmitter.translate + dir * (dist * 0.10f);

    float speed = 8.0f + GPURand1(seed + 2.0f) * 16.0f;
    p.velocity = dir * speed;

    float w = 0.02f + GPURand1(seed + 3.0f) * 0.03f;
    float h = 0.08f + GPURand1(seed + 4.0f) * 0.18f;
    p.scale = float3(w, h, 1.0f);

    p.lifeTime = 0.06f + GPURand1(seed + 5.0f) * 0.12f;
    p.color = float4(1.0f, 0.95f, 0.65f, 1.0f);
}

void Preset_ExplosionFire(uint i, float3 seed, inout Particle p)
{
    float3 offset = SampleSphere(seed, gEmitter.radius);
    p.translate = gEmitter.translate + offset;

    float3 dir = normalize(offset + float3(0.0f, 0.35f, 0.0f));
    float speed = 1.8f + GPURand1(seed + 2.0f) * 4.2f;
    p.velocity = dir * speed;

    float s = 0.08f + GPURand1(seed + 3.0f) * 0.15f;
    p.scale = float3(s, s, s);

    p.lifeTime = 0.35f + GPURand1(seed + 4.0f) * 0.55f;

    float heat = GPURand1(seed + 5.0f);
    float3 c = lerp(float3(1.0f, 0.65f, 0.05f), float3(1.0f, 0.15f, 0.0f), heat);
    p.color = float4(c, 0.9f);
}

void Preset_RushTrail(uint i, float3 seed, inout Particle p)
{
    // 残像トレイル（疑似リボン）：細長いスプライト + 速度方向に伸ばす
    float3 offset = SampleSphere(seed, gEmitter.radius * 0.35f);
    p.translate = gEmitter.translate + offset;

    float3 dir = SampleUnitDir(seed + 2.0f);
    p.velocity = dir * (0.2f + GPURand1(seed + 3.0f) * 0.8f);

    float w = 0.06f + GPURand1(seed + 4.0f) * 0.10f;
    float h = 0.18f + GPURand1(seed + 5.0f) * 0.35f;
    p.scale = float3(w, h, 1.0f);

    p.lifeTime = 0.12f + GPURand1(seed + 6.0f) * 0.25f;
    p.color = float4(0.75f, 0.9f, 1.0f, 0.55f);
}

void Preset_SpinSlash(uint i, float3 seed, inout Particle p)
{
    // 旋風斬り：円周上 + 円周方向の速度 → リボンに見せやすい
    float a = GPURand1(seed + 1.0f) * 6.2831853f;
    float r = gEmitter.radius * (0.65f + GPURand1(seed + 2.0f) * 0.35f);

    float3 pos = gEmitter.translate + float3(cos(a) * r, 0.0f, sin(a) * r);
    pos.y += GPURand1(seed + 3.0f) * 0.25f;
    p.translate = pos;

    float3 tang = normalize(float3(-sin(a), 0.0f, cos(a))); // 円周方向
    p.velocity = tang * (3.0f + GPURand1(seed + 4.0f) * 7.0f);

    // リボンっぽく：細長く
    float w = 0.03f + GPURand1(seed + 5.0f) * 0.06f;
    float h = 0.60f + GPURand1(seed + 6.0f) * 1.20f;
    p.scale = float3(w, h, 1.0f);

    p.lifeTime = 0.10f + GPURand1(seed + 7.0f) * 0.20f;
    p.color = float4(1.0f, 0.9f, 0.45f, 0.75f);
}

void Preset_BulletTracer(uint i, float3 seed, inout Particle p)
{
    float3 dir = SampleUnitDir(seed + 11.0f);

    // 銃口から少し先へ
    p.translate = gEmitter.translate + dir * (0.2f + GPURand1(seed + 12.0f) * 0.2f);

    // 速めに動かす（Updateで位置が進む）
    p.velocity = dir * (60.0f + GPURand1(seed + 13.0f) * 60.0f);

    // トレーサーは “細く長い”
    float w = 0.02f + GPURand1(seed + 14.0f) * 0.03f;
    float h = 1.50f + GPURand1(seed + 15.0f) * 2.50f;
    p.scale = float3(w, h, 1.0f);

    p.lifeTime = 0.05f + GPURand1(seed + 16.0f) * 0.08f;
    p.color = float4(1.0f, 0.95f, 0.8f, 0.85f);
}

// ------------------------------------------------------------
// Presets (追加分)
// ------------------------------------------------------------
void Preset_MuzzleFlash(uint i, float3 seed, inout Particle p)
{
    float3 dir = SampleUnitDir(seed + 1.0f);
    p.translate = gEmitter.translate + dir * (0.01f + GPURand1(seed + 2.0f) * 0.03f);

    float s = 0.10f + GPURand1(seed + 3.0f) * 0.25f;
    p.scale = float3(s, s, 1.0f);

    p.velocity = dir * (0.5f + GPURand1(seed + 4.0f) * 1.5f);
    p.lifeTime = 0.03f + GPURand1(seed + 5.0f) * 0.05f;

    // 黄〜橙（強め）
    float t = GPURand1(seed + 6.0f);
    float3 c = lerp(float3(1.0f, 0.95f, 0.6f), float3(1.0f, 0.55f, 0.05f), t);
    p.color = float4(c, 0.95f);
}

void Preset_Blood(uint i, float3 seed, inout Particle p)
{
    float3 dir = SampleHemisphereUp(seed + 1.0f);
    p.translate = gEmitter.translate + dir * (GPURand1(seed + 2.0f) * gEmitter.radius * 0.08f);

    float speed = 2.0f + GPURand1(seed + 3.0f) * 8.0f;
    p.velocity = dir * speed;

    float s = 0.03f + GPURand1(seed + 4.0f) * 0.08f;
    p.scale = float3(s, s, 1.0f);

    p.lifeTime = 0.25f + GPURand1(seed + 5.0f) * 0.55f;

    float t = GPURand1(seed + 6.0f);
    float3 c = lerp(float3(0.55f, 0.05f, 0.05f), float3(0.85f, 0.10f, 0.10f), t);
    p.color = float4(c, 0.85f);
}

void Preset_ImpactDust(uint i, float3 seed, inout Particle p)
{
    float3 dir = SampleHemisphereUp(seed + 1.0f);
    float dist = GPURand1(seed + 2.0f) * gEmitter.radius * 0.15f;
    p.translate = gEmitter.translate + dir * dist;

    float speed = 0.6f + GPURand1(seed + 3.0f) * 2.2f;
    p.velocity = dir * speed;

    float s = 0.10f + GPURand1(seed + 4.0f) * 0.22f;
    p.scale = float3(s, s, 1.0f);

    p.lifeTime = 0.45f + GPURand1(seed + 5.0f) * 0.95f;

    float g = 0.55f + GPURand1(seed + 6.0f) * 0.25f;
    p.color = float4(g, g, g, 0.55f);
}

void Preset_ImpactMetal(uint i, float3 seed, inout Particle p)
{
    // 金属：細かい欠片+少し火花っぽい色
    float3 dir = SampleUnitDir(seed + 1.0f);
    dir.y = abs(dir.y) * 0.6f + 0.2f;
    dir = normalize(dir);

    p.translate = gEmitter.translate + dir * (GPURand1(seed + 2.0f) * gEmitter.radius * 0.08f);

    float speed = 4.0f + GPURand1(seed + 3.0f) * 14.0f;
    p.velocity = dir * speed;

    float w = 0.015f + GPURand1(seed + 4.0f) * 0.02f;
    float h = 0.05f + GPURand1(seed + 5.0f) * 0.12f;
    p.scale = float3(w, h, 1.0f);

    p.lifeTime = 0.08f + GPURand1(seed + 6.0f) * 0.18f;
    p.color = float4(1.0f, 0.85f, 0.55f, 0.95f);
}

void Preset_ImpactWood(uint i, float3 seed, inout Particle p)
{
    // 木：粉+破片（茶系）
    float3 dir = SampleHemisphereUp(seed + 1.0f);
    p.translate = gEmitter.translate + dir * (GPURand1(seed + 2.0f) * gEmitter.radius * 0.12f);

    float speed = 1.2f + GPURand1(seed + 3.0f) * 5.0f;
    p.velocity = dir * speed;

    float s = 0.06f + GPURand1(seed + 4.0f) * 0.14f;
    p.scale = float3(s, s, 1.0f);

    p.lifeTime = 0.35f + GPURand1(seed + 5.0f) * 0.80f;

    float t = GPURand1(seed + 6.0f);
    float3 c = lerp(float3(0.35f, 0.22f, 0.12f), float3(0.55f, 0.36f, 0.20f), t);
    p.color = float4(c, 0.65f);
}

void Preset_ExplosionSmoke(uint i, float3 seed, inout Particle p)
{
    float3 offset = SampleSphere(seed + 1.0f, gEmitter.radius);
    p.translate = gEmitter.translate + offset;

    float3 dir = normalize(offset + float3(0.0f, 0.8f, 0.0f));
    float speed = 0.4f + GPURand1(seed + 2.0f) * 1.6f;
    p.velocity = dir * speed;

    float s = 0.25f + GPURand1(seed + 3.0f) * 0.65f;
    p.scale = float3(s, s, 1.0f);

    p.lifeTime = 1.00f + GPURand1(seed + 4.0f) * 2.00f;

    float g = 0.18f + GPURand1(seed + 5.0f) * 0.18f;
    p.color = float4(g, g, g, 0.45f);
}

void Preset_FootDust(uint i, float3 seed, inout Particle p)
{
    float3 offset = SampleSphere(seed + 1.0f, gEmitter.radius * 0.10f);
    offset.y = abs(offset.y) * 0.05f;
    p.translate = gEmitter.translate + offset;

    float3 dir = SampleHemisphereUp(seed + 2.0f);
    float speed = 0.3f + GPURand1(seed + 3.0f) * 1.2f;
    p.velocity = dir * speed;

    float s = 0.05f + GPURand1(seed + 4.0f) * 0.12f;
    p.scale = float3(s, s, 1.0f);

    p.lifeTime = 0.30f + GPURand1(seed + 5.0f) * 0.60f;

    float g = 0.45f + GPURand1(seed + 6.0f) * 0.20f;
    p.color = float4(g, g, g, 0.35f);
}

void Preset_EnvDust(uint i, float3 seed, inout Particle p)
{
    // 常時漂う微粒子
    float3 offset = SampleSphere(seed + 1.0f, gEmitter.radius);
    p.translate = gEmitter.translate + offset;

    float3 dir = SampleUnitDir(seed + 2.0f);
    p.velocity = dir * (0.05f + GPURand1(seed + 3.0f) * 0.15f);

    float s = 0.01f + GPURand1(seed + 4.0f) * 0.03f;
    p.scale = float3(s, s, 1.0f);

    p.lifeTime = 2.0f + GPURand1(seed + 5.0f) * 5.0f;

    float g = 0.75f + GPURand1(seed + 6.0f) * 0.15f;
    p.color = float4(g, g, g, 0.12f);
}

void Preset_PickupGlow(uint i, float3 seed, inout Particle p)
{
    float a = GPURand1(seed + 1.0f) * 6.2831853f;
    float r = gEmitter.radius * (0.10f + GPURand1(seed + 2.0f) * 0.25f);
    p.translate = gEmitter.translate + float3(cos(a) * r, GPURand1(seed + 3.0f) * 0.10f, sin(a) * r);

    p.velocity = float3(0.0f, 0.15f + GPURand1(seed + 4.0f) * 0.35f, 0.0f);

    float s = 0.04f + GPURand1(seed + 5.0f) * 0.08f;
    p.scale = float3(s, s, 1.0f);

    p.lifeTime = 0.8f + GPURand1(seed + 6.0f) * 1.2f;

    float3 c = lerp(float3(0.2f, 0.9f, 1.0f), float3(1.0f, 0.95f, 0.45f), GPURand1(seed + 7.0f));
    p.color = float4(c, 0.55f);
}

void Preset_SkillEffect(uint i, float3 seed, inout Particle p)
{
    // 周回+上昇（スキルっぽく）
    float a = GPURand1(seed + 1.0f) * 6.2831853f;
    float r = gEmitter.radius * (0.15f + GPURand1(seed + 2.0f) * 0.45f);

    float3 pos = gEmitter.translate + float3(cos(a) * r, GPURand1(seed + 3.0f) * 0.20f, sin(a) * r);
    p.translate = pos;

    float3 tang = normalize(float3(-sin(a), 0.0f, cos(a)));
    p.velocity = tang * (0.4f + GPURand1(seed + 4.0f) * 1.2f) + float3(0, 0.4f, 0);

    float s = 0.06f + GPURand1(seed + 5.0f) * 0.16f;
    p.scale = float3(s, s, 1.0f);

    p.lifeTime = 0.6f + GPURand1(seed + 6.0f) * 1.0f;

    float t = GPURand1(seed + 7.0f);
    float3 c = lerp(float3(0.55f, 0.25f, 1.0f), float3(0.2f, 0.9f, 1.0f), t);
    p.color = float4(c, 0.55f);
}

void Preset_BossAppearDust(uint i, float3 seed, inout Particle p)
{
    // 地面からドン！の砂埃
    float a = GPURand1(seed + 1.0f) * 6.2831853f;
    float r = gEmitter.radius * (0.25f + GPURand1(seed + 2.0f) * 0.75f);

    float3 pos = gEmitter.translate + float3(cos(a) * r, 0.0f, sin(a) * r);
    pos.y += GPURand1(seed + 3.0f) * 0.10f;
    p.translate = pos;

    float3 radial = normalize(float3(cos(a), 0.0f, sin(a)));
    float3 dir = normalize(radial * (0.6f + GPURand1(seed + 4.0f) * 0.6f) + float3(0, 0.9f, 0));

    p.velocity = dir * (0.8f + GPURand1(seed + 5.0f) * 3.0f);

    float s = 0.20f + GPURand1(seed + 6.0f) * 0.50f;
    p.scale = float3(s, s, 1.0f);

    p.lifeTime = 0.9f + GPURand1(seed + 7.0f) * 1.3f;

    float3 c = lerp(float3(0.30f, 0.25f, 0.20f), float3(0.55f, 0.50f, 0.40f), GPURand1(seed + 8.0f));
    p.color = float4(c, 0.65f);
}

void Preset_BossAura(uint i, float3 seed, inout Particle p)
{
    float a = GPURand1(seed + 1.0f) * 6.2831853f;
    float r = gEmitter.radius * (0.55f + GPURand1(seed + 2.0f) * 0.45f);

    float3 pos = gEmitter.translate + float3(cos(a) * r, 0.2f + GPURand1(seed + 3.0f) * 0.8f, sin(a) * r);
    p.translate = pos;

    float3 tang = normalize(float3(-sin(a), 0.0f, cos(a)));
    p.velocity = tang * (0.1f + GPURand1(seed + 4.0f) * 0.4f);

    float s = 0.08f + GPURand1(seed + 5.0f) * 0.18f;
    p.scale = float3(s, s, 1.0f);

    p.lifeTime = 0.7f + GPURand1(seed + 6.0f) * 1.2f;

    p.color = float4(0.35f, 0.85f, 1.0f, 0.25f);
}

void Preset_Shockwave(uint i, float3 seed, inout Particle p)
{
    // 外向きに走る帯（Ribbon扱いが映える）
    float a = GPURand1(seed + 1.0f) * 6.2831853f;
    float r = gEmitter.radius * (0.10f + GPURand1(seed + 2.0f) * 0.20f);

    float3 radial = normalize(float3(cos(a), 0.0f, sin(a)));
    p.translate = gEmitter.translate + radial * r;

    p.velocity = radial * (8.0f + GPURand1(seed + 3.0f) * 10.0f);

    float w = 0.03f + GPURand1(seed + 4.0f) * 0.05f;
    float h = 0.60f + GPURand1(seed + 5.0f) * 1.00f;
    p.scale = float3(w, h, 1.0f);

    p.lifeTime = 0.10f + GPURand1(seed + 6.0f) * 0.18f;
    p.color = float4(0.85f, 0.95f, 1.0f, 0.55f);
}

void Preset_BossDeathSoul(uint i, float3 seed, inout Particle p)
{
    float3 offset = SampleSphere(seed + 1.0f, gEmitter.radius * 0.35f);
    p.translate = gEmitter.translate + offset;

    float3 dir = normalize(offset + float3(0, 1.2f, 0));
    p.velocity = dir * (0.6f + GPURand1(seed + 2.0f) * 2.0f);

    float s = 0.05f + GPURand1(seed + 3.0f) * 0.15f;
    p.scale = float3(s, s, 1.0f);

    p.lifeTime = 0.8f + GPURand1(seed + 4.0f) * 1.8f;
    p.color = float4(0.55f, 0.25f, 1.0f, 0.35f);
}

void Preset_BossDebrisDust(uint i, float3 seed, inout Particle p)
{
    float3 offset = SampleSphere(seed + 1.0f, gEmitter.radius);
    offset.y = abs(offset.y) * 0.25f;
    p.translate = gEmitter.translate + offset;

    float3 dir = normalize(offset + float3(0, 0.8f, 0));
    p.velocity = dir * (1.0f + GPURand1(seed + 2.0f) * 5.0f);

    float s = 0.12f + GPURand1(seed + 3.0f) * 0.35f;
    p.scale = float3(s, s, 1.0f);

    p.lifeTime = 0.6f + GPURand1(seed + 4.0f) * 1.4f;

    float3 c = lerp(float3(0.35f, 0.30f, 0.25f), float3(0.60f, 0.55f, 0.45f), GPURand1(seed + 5.0f));
    p.color = float4(c, 0.70f);
}

void Preset_Heal(uint i, float3 seed, inout Particle p)
{
    float3 offset = SampleSphere(seed + 1.0f, gEmitter.radius * 0.35f);
    p.translate = gEmitter.translate + offset;

    p.velocity = float3(0.0f, 0.6f + GPURand1(seed + 2.0f) * 1.4f, 0.0f);

    float s = 0.06f + GPURand1(seed + 3.0f) * 0.14f;
    p.scale = float3(s, s, 1.0f);

    p.lifeTime = 0.6f + GPURand1(seed + 4.0f) * 1.2f;

    float t = GPURand1(seed + 5.0f);
    float3 c = lerp(float3(0.25f, 1.0f, 0.55f), float3(0.75f, 1.0f, 0.85f), t);
    p.color = float4(c, 0.55f);
}

void SpawnByType(uint i, float3 seed, inout Particle p)
{
    switch (gEmitter.type)
    {
        case GPU_PARTICLE_TYPE_MUZZLEFLASH: // マズルフラッシュ (=1)
            Preset_MuzzleFlash(i, seed, p);
            break;

        case GPU_PARTICLE_TYPE_BULLETTRACER: // トレーサー (=2)
            Preset_BulletTracer(i, seed, p);
            break;

        case GPU_PARTICLE_TYPE_HITSPARK: // ヒットスパーク (=3)
            Preset_HitSpark(i, seed, p);
            break;

        case GPU_PARTICLE_TYPE_BLOOD: // 血しぶき (=4)
            Preset_Blood(i, seed, p);
            break;

        case GPU_PARTICLE_TYPE_IMPACT_DUST: // 砂埃 (=5)
            Preset_ImpactDust(i, seed, p);
            break;

        case GPU_PARTICLE_TYPE_IMPACT_METAL: // 金属破片 (=6)
            Preset_ImpactMetal(i, seed, p);
            break;

        case GPU_PARTICLE_TYPE_IMPACT_WOOD: // 木片 (=7)
            Preset_ImpactWood(i, seed, p);
            break;

        case GPU_PARTICLE_TYPE_EXPLOSION_FIRE: // 爆炎 (=8)
            Preset_ExplosionFire(i, seed, p);
            break;

        case GPU_PARTICLE_TYPE_EXPLOSION_SMOKE: // 爆煙 (=9)
            Preset_ExplosionSmoke(i, seed, p);
            break;

        case GPU_PARTICLE_TYPE_FOOT_DUST: // 足元の砂埃 (=10)
            Preset_FootDust(i, seed, p);
            break;

        case GPU_PARTICLE_TYPE_ENV_DUST: // 環境微粒子 (=11)
            Preset_EnvDust(i, seed, p);
            break;

        case GPU_PARTICLE_TYPE_PICKUP_GLOW: // アイテムの輝き (=12)
            Preset_PickupGlow(i, seed, p);
            break;

        case GPU_PARTICLE_TYPE_SKILL_EFFECT: // スキルエフェクト (=13)
            Preset_SkillEffect(i, seed, p);
            break;

        case GPU_PARTICLE_TYPE_BOSS_APPEAR_DUST: // ボス出現ダスト (=14)
            Preset_BossAppearDust(i, seed, p);
            break;

        case GPU_PARTICLE_TYPE_BOSS_AURA: // ボスオーラ (=15)
            Preset_BossAura(i, seed, p);
            break;

        case GPU_PARTICLE_TYPE_BOSS_SHOCKWAVE: // ショックウェーブ (=16)
            Preset_Shockwave(i, seed, p);
            break;

        case GPU_PARTICLE_TYPE_BOSS_RUSH_TRAIL: // ラッシュトレイル (=17)
            Preset_RushTrail(i, seed, p);
            break;

        case GPU_PARTICLE_TYPE_SPIN_ATTACK_SLASH: // 旋風斬り (=18)
            Preset_SpinSlash(i, seed, p);
            break;

        case GPU_PARTICLE_TYPE_BOSS_DEATH_SOUL: // ボス死亡魂 (=19)
            Preset_BossDeathSoul(i, seed, p);
            break;

        case GPU_PARTICLE_TYPE_BOSS_DEBRIS_DUST: // ボス破片砂埃 (=20)
            Preset_BossDebrisDust(i, seed, p);
            break;

        case GPU_PARTICLE_TYPE_HEAL: // 回復エフェクト (=21)
            Preset_Heal(i, seed, p);
            break;

        default:
            Preset_Default(i, seed, p); // デフォルト (=0)
            break;
    }
}

// ------------------------------------------------------------
// Entry
// ------------------------------------------------------------
[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (gEmitter.emit == 0 || gEmitter.count == 0)
        return;

    uint kind = DecideRenderKind(gEmitter.type);

    for (uint i = 0; i < gEmitter.count; ++i)
    {
        int top;
        InterlockedAdd(gFreeListIndex[0], -1, top);

        if (0 <= top && top < (int) kMaxParticleCount)
        {
            uint particleIndex = gFreeList[top];

            Particle p = (Particle) 0;

            // seed：i と time と type を混ぜる
            float3 seed = float3((float) i * 12.9898f, gPerFrame.time * 78.233f, (float) gEmitter.type * 37.719f);

            SpawnByType(i, seed, p);
            FinalizeParticle(p, kind);

            gParticles[particleIndex] = p;
        }
        else
        {
            // 空き無し：巻き戻して終了
            InterlockedAdd(gFreeListIndex[0], 1, top);
            break;
        }
    }
}
