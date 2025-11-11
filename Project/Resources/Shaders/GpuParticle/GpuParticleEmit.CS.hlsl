#include "GpuParticleData.hlsli" // パーティクルデータ構造体

// エミッタースフィア構造体
struct EmitterCBData
{
    float3 translate; // 位置
    float radius; // 半径
    uint count; // 発生数
    float frequency; // 発生頻度
    float frequencyTime; // 発生頻度タイマー
    uint emit; // 発生フラグ
};

// 乱数生成関数
float3 rand3dTo3d(float3 seed)
{
    // 3Dから3Dへのランダム変換
    seed = frac(seed * 0.1031f);
    seed += dot(seed, seed.yzx + 33.33f);
    return frac((seed.xxy + seed.yzz) * seed.zyx);
}

// 乱数生成関数
float rand3dTo1d(float3 seed)
{
    // 3Dから1Dへのランダム変換
    return rand3dTo3d(seed).x;
}

// ランダム生成関数
class RandomGenerator
{
    float3 seed;
    
    float3 Generate3d()
    {
        seed = rand3dTo3d(seed);
        return seed;
    }
    
    float Generate1d()
    {
        float result = rand3dTo1d(seed);
        seed.x = result; // シードを更新
        return result;
    }
};

// 原子加算関数の代替実装（実際にはInterlockedAddを使用する）
void InterLockedAdd(int dest, int add, int originalValue)
{
    originalValue = dest;
    dest += add;
}

// 時間制御用定数
struct PerFrame
{
    float time; // 経過時間
    float deltaTime; // フレーム間の時間差
};

static const uint kMaxParticleCount = 1024; // 最大パーティクル数

RWStructuredBuffer<Particle> gParticles : register(u0); // 書き込み可能なパーティクルバッファ
RWStructuredBuffer<int> gFreeListIndex : register(u1); // フリーリストインデックスバッファ
RWStructuredBuffer<uint> gFreeList : register(u2); // フリーリストバッファ

ConstantBuffer<EmitterCBData> gEmitter : register(b1); // エミッタースフィア定数バッファ
ConstantBuffer<PerFrame> gPerFrame : register(b2); // フレーム定数バッファ

[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // エミットしないなら何もしない
    if (gEmitter.emit == 0 || gEmitter.count == 0)
    {
        return;
    }

    // 1スレッドでまとめて Emit
    for (uint i = 0; i < gEmitter.count; ++i)
    {
        int freeListIndex;
        // トップを 1 減らして、元の値をもらう（= 今のトップ位置）
        InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);

        // 有効範囲チェック：0..kMaxParticleCount-1
        if (0 <= freeListIndex && freeListIndex < (int) kMaxParticleCount)
        {
            uint particleIndex = gFreeList[freeListIndex];

            Particle p = (Particle) 0;

            float3 rand = rand3dTo3d(float3(gPerFrame.time, i, 0));
            float3 dir = normalize(rand * 2.0f - 1.0f);

            float t = frac((float) i / max(gEmitter.count, 1u) + gPerFrame.time * 0.2f);
            
            float r = saturate(abs(t * 6.0f - 3.0f) - 1.0f);
            float g = saturate(2.0f - abs(t * 6.0f - 2.0f));
            float b = saturate(2.0f - abs(t * 6.0f - 4.0f));
            
            p.translate = gEmitter.translate + dir * gEmitter.radius;
            p.scale = float3(0.5f, 0.5f, 0.5f);
            p.velocity = dir * 2.0f;
            p.color = float4(r, g, b, 1.0f);
            p.lifeTime = 1.0f;
            p.currentTime = 0.0f;

            gParticles[particleIndex] = p;
        }
        else
        {
            // 空きが無かった：元に戻して終了
            InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
            break;
        }
    }
}