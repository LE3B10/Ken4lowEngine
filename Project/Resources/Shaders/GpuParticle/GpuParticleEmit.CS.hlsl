#include "GpuParticleData.hlsli" // パーティクルデータ構造体

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

static const uint kMaxParticleCount = 131072; // 最大パーティクル数 2^17

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
            uint particleIndex = gFreeList[freeListIndex]; // フリーリストからインデックスを取得

            // パーティクルデータを初期化して設定
            Particle particle = (Particle) 0;

            // ランダム方向ベクトルを生成
            float3 rand = rand3dTo3d(float3(gPerFrame.time, i, 0));

            // 単位球面上のランダムな方向ベクトル
            float3 dir = normalize(rand * 2.0f - 1.0f);

            // 色相を時間とインデックスで変化させる
            float t = frac((float) i / max(gEmitter.count, 1u) + gPerFrame.time * 0.2f);
            
            // HSV to RGB変換（彩度=1、明度=1固定）
            float r = saturate(abs(t * 6.0f - 3.0f) - 1.0f); // 赤成分
            float g = saturate(2.0f - abs(t * 6.0f - 2.0f)); // 緑成分
            float b = saturate(2.0f - abs(t * 6.0f - 4.0f)); // 青成分
            
            // パーティクルプロパティ設定
            particle.translate = gEmitter.translate + dir * gEmitter.radius;
            particle.scale = float3(0.5f, 0.5f, 0.5f);
            particle.velocity = dir * 2.0f;
            particle.lifeTime = 1.0f;
            particle.currentTime = 0.0f;
            particle.type = gEmitter.type;
            particle.billboardMode = gEmitter.billboardMode;
            particle.color = float4(r, g, b, 1.0f);

            gParticles[particleIndex] = particle;
        }
        else
        {
            // 空きが無かった：元に戻して終了
            InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
            break;
        }
    }
}