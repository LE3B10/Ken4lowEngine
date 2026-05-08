#include "GpuParticleData.hlsli"
#include "GpuParticleTypeParams.hlsli"

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);

ConstantBuffer<PerFrame> gPerFrame : register(b2);

float Hash13(float3 p)
{
    p = frac(p * 0.1031f);
    p += dot(p, p.yzx + 33.33f);
    return frac((p.x + p.y) * p.z);
}

float ValueNoise3D(float3 p)
{
    float3 i = floor(p);
    float3 f = frac(p);

    f = f * f * (3.0f - 2.0f * f);

    float n000 = Hash13(i + float3(0.0f, 0.0f, 0.0f));
    float n100 = Hash13(i + float3(1.0f, 0.0f, 0.0f));
    float n010 = Hash13(i + float3(0.0f, 1.0f, 0.0f));
    float n110 = Hash13(i + float3(1.0f, 1.0f, 0.0f));
    float n001 = Hash13(i + float3(0.0f, 0.0f, 1.0f));
    float n101 = Hash13(i + float3(1.0f, 0.0f, 1.0f));
    float n011 = Hash13(i + float3(0.0f, 1.0f, 1.0f));
    float n111 = Hash13(i + float3(1.0f, 1.0f, 1.0f));

    float nx00 = lerp(n000, n100, f.x);
    float nx10 = lerp(n010, n110, f.x);
    float nx01 = lerp(n001, n101, f.x);
    float nx11 = lerp(n011, n111, f.x);

    float nxy0 = lerp(nx00, nx10, f.y);
    float nxy1 = lerp(nx01, nx11, f.y);

    return lerp(nxy0, nxy1, f.z);
}

float FBM3D(float3 p)
{
    float value = 0.0f;
    float amp = 0.5f;

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        value += ValueNoise3D(p) * amp;
        p *= 2.03f;
        amp *= 0.5f;
    }

    return value;
}

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;
    if (particleIndex >= kMaxParticleCount)
        return;

    Particle p = gParticles[particleIndex];

    if (p.lifeTime <= 0.0f)
        return;

    float dt = gPerFrame.deltaTime;
    p.currentTime += dt;

    if (p.currentTime >= p.lifeTime)
    {
        p.color.a = 0.0f;
        p.scale = float3(0, 0, 0);
        p.lifeTime = 0.0f;
        p.currentTime = 0.0f;

        gParticles[particleIndex] = p;

        int oldTop;
        InterlockedAdd(gFreeListIndex[0], 1, oldTop);
        uint newTop = (uint) (oldTop + 1);

        if (newTop < kMaxParticleCount)
        {
            gFreeList[newTop] = particleIndex;
        }
        else
        {
            InterlockedAdd(gFreeListIndex[0], -1, oldTop);
        }
        return;
    }

    float t = saturate(p.currentTime / max(p.lifeTime, 1e-5f));
    ParticleTypeParam param = GetParticleTypeParam(p.type);

    float damp = max(0.0f, 1.0f - param.drag * dt);
    p.velocity *= damp;
    p.velocity.y += param.accelY * dt;

    if (p.type == GPU_PARTICLE_TYPE_VOXEL_ASH_FRAGMENT)
    {
        float3 noisePos = p.translate * 1.75f + float3(
            gPerFrame.time * 0.35f,
            gPerFrame.time * 0.22f,
            gPerFrame.time * 0.28f
        );

        float n1 = FBM3D(noisePos + float3(13.1f, 7.7f, 2.3f));
        float n2 = FBM3D(noisePos + float3(3.5f, 19.2f, 11.8f));
        float n3 = FBM3D(noisePos + float3(23.4f, 4.8f, 15.6f));

        float3 curlLikeWind = float3(
            n1 - 0.5f,
            (n2 - 0.5f) * 0.35f,
            n3 - 0.5f
        );

        float3 mainWind = float3(0.85f, 0.22f, 0.18f);
        float windStrength = lerp(0.35f, 1.15f, t);
        float noiseStrength = 1.25f;

        // 灰粒子だけノイズ風の揺れを足して、風に舞うようにする。
        p.velocity += (mainWind * windStrength + curlLikeWind * noiseStrength) * dt;
    }

    p.translate += p.velocity * dt;

    if (param.scaleGrow > 0.0f)
    {
        uint kind = GPUParticle_GetKind(p.billboardMode);
        if (kind == GPU_PARTICLE_KIND_SPRITE || kind == GPU_PARTICLE_KIND_RIBBON)
        {
            p.scale.xy += param.scaleGrow * dt;
        }
        else
        {
            p.scale += param.scaleGrow * dt;
        }
    }

    if (param.scaleShrink > 0.0f)
    {
        float s = max(0.0f, 1.0f - param.scaleShrink * dt);
        p.scale *= s;
    }

    float a = pow(1.0f - t, param.alphaPow) * param.baseAlpha;
    p.color.a = saturate(a);

    gParticles[particleIndex] = p;
}