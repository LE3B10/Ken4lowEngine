#include "GpuParticleData.hlsli"
#include "GpuParticleTypeParams.hlsli"

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);

ConstantBuffer<PerFrame> gPerFrame : register(b2);

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

    p.translate += p.velocity * dt;
    float3 centerDir = normalize(-p.translate + float3(1e-4f, 1e-4f, 1e-4f));
    p.velocity += centerDir * param.convergence * dt;
    p.velocity += normalize(p.translate + float3(1e-4f, 1e-4f, 1e-4f)) * param.divergence * dt;
    p.velocity.y += sin(p.currentTime * 12.0f + particleIndex * 0.13f) * param.floaty * dt;

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

    float fadeIn = (param.fadeInRatio <= 0.0f) ? 1.0f : saturate(t / max(param.fadeInRatio, 1e-4f));
    float fadeOutStart = 1.0f - saturate(param.fadeOutRatio);
    float fadeOut = (t <= fadeOutStart) ? 1.0f : saturate((1.0f - t) / max(1.0f - fadeOutStart, 1e-4f));
    float a = pow(1.0f - t, param.alphaPow) * param.baseAlpha * fadeIn * fadeOut;
    p.color.a = saturate(a);

    gParticles[particleIndex] = p;
}
