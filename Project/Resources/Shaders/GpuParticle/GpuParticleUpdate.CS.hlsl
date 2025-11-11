#include "GpuParticleData.hlsli" // パーティクルデータ構造体

// 時間制御用定数
struct PerFrame
{
    float time; // 経過時間
    float deltaTime; // フレーム間の時間差
};

static const uint kMaxParticleCount = 131072; // 最大パーティクル数 2^17

RWStructuredBuffer<Particle> gParticles : register(u0); // 書き込み可能なパーティクルバッファ
RWStructuredBuffer<int> gFreeListIndex : register(u1); // フリーリストインデックスバッファ
RWStructuredBuffer<uint> gFreeList : register(u2); // フリーリストバッファ

ConstantBuffer<PerFrame> gPerFrame : register(b2); // フレーム情報

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;
    if (particleIndex >= kMaxParticleCount)
        return;

    Particle p = gParticles[particleIndex];

    // 既に死んでるなら何もしない
    if (p.color.a == 0.0f || p.lifeTime <= 0.0f)
    {
        return;
    }

    p.currentTime += gPerFrame.deltaTime;

    // まだ生きてる
    if (p.currentTime < p.lifeTime)
    {
        p.translate += p.velocity * gPerFrame.deltaTime;
        float alpha = 1.0f - (p.currentTime / p.lifeTime);
        p.color.a = saturate(alpha);
        gParticles[particleIndex] = p;
        return;
    }

    // ここに来たら「死んだ」パーティクルだけ
    p.color.a = 0.0f;
    p.scale = float3(0, 0, 0);
    gParticles[particleIndex] = p;

    // FreeList に push
    int freeListIndex;
    InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);

    uint newTop = (uint) (freeListIndex + 1);
    if (newTop < kMaxParticleCount)
    {
        gFreeList[newTop] = particleIndex;
    }
    else
    {
        InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
    }
}