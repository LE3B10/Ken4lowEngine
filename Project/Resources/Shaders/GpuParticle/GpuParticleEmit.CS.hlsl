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
            
            // 乱数シード（種類ごとに使い回す）
            float3 seed = float3(gPerFrame.time, i, 0);
            
            switch (gEmitter.type)
            {
                case GPU_PARTICLE_TYPE_DEFAULT: // デフォルトパーティクルタイプ
                default:
                   {
                        // ランダム方向ベクトルを生成
                        float3 rand = rand3dTo3d(seed);

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
                        particle.color = float4(r, g, b, 1.0f);
                    }
                    break;
                
                case GPU_PARTICLE_TYPE_HITSPARK:
                {
                        // ランダム方向ベクトルを生成
                        float3 r3 = rand3dTo3d(seed);

                        // ランダム方向（上方向に寄せた半球）
                        float3 dir = normalize(r3 * 2.0f - 1.0f);
                        dir.y = abs(dir.y);
                        dir = normalize(dir + float3(0.0f, 0.35f, 0.0f));

                        // ヒット地点付近に少しだけ散らす
                        float dist = rand3dTo1d(seed + 1.0f) * gEmitter.radius;
                        float3 pos = gEmitter.translate + dir * (dist * 0.10f);
                        particle.translate = pos;

                        // スピード（火花らしく速め）
                        float speed = 8.0f + rand3dTo1d(seed + 2.0f) * 16.0f; // 8..24
                        particle.velocity = dir * speed;

                        // 細長いスプライト（火花の筋）
                        float w = 0.02f + rand3dTo1d(seed + 3.0f) * 0.03f;
                        float h = 0.08f + rand3dTo1d(seed + 4.0f) * 0.18f;
                        particle.scale = float3(w, h, 1.0f);

                        // 短命
                        particle.lifeTime = 0.06f + rand3dTo1d(seed + 5.0f) * 0.12f;

                        // 白〜黄〜橙のランダム
                        float t = rand3dTo1d(seed + 6.0f);
                        float3 c = lerp(float3(1.0f, 0.95f, 0.60f), float3(1.0f, 0.50f, 0.10f), t);
                        particle.color = float4(c, 1.0f);
                    }
                  
                case GPU_PARTICLE_TYPE_EXPLOSION_FIRE: // 爆発・火の粒
                    {
                        float3 r3 = rand3dTo3d(seed);

                        // ランダム方向ベクトル
                        float3 dir = normalize(r3 * 2.0f - 1.0f);

                        // 中心から半径内にランダム配置
                        float dist = rand3dTo1d(seed + 1.0f) * gEmitter.radius;
                        float3 pos = gEmitter.translate + dir * dist;
                        pos.y += rand3dTo1d(seed + 2.0f) * 0.4f; // ちょっと上方向に散らす

                        particle.translate = pos;

                        // 火の玉のサイズ
                        float s = 0.35f + rand3dTo1d(seed + 3.0f) * 0.25f;
                        particle.scale = float3(s, s, s);

                        // 外側に高速で飛ぶ
                        float speed = 6.0f + rand3dTo1d(seed + 4.0f) * 4.0f;
                        particle.velocity = dir * speed;

                        // 寿命短め（パンッと消える）
                        particle.lifeTime = 0.35f + rand3dTo1d(seed + 5.0f) * 0.25f;

                        // 暖色系（オレンジ～黄色）
                        float t = rand3dTo1d(seed + 6.0f);
                        float3 col = lerp(float3(1.0f, 0.4f, 0.0f), // オレンジ
                          float3(1.0f, 0.9f, 0.4f), // 黄っぽい
                          t);
                        particle.color = float4(col, 1.0f);
                    }
                    break;
                
                // ボスの登場砂埃パーティクルタイプ
                case GPU_PARTCILE_TYPE_BOSS_APPEAR_DUST: // ボス登場砂埃パーティクルタイプ
                    {
                        float3 rand3 = rand3dTo3d(seed);
                        float2 rand2 = rand3.xz * 2.0f - 1.0f;
                        float2 dir2 = normalize(rand2);
                        float dist = rand3dTo1d(seed + 1.0f) * gEmitter.radius;
                    
                        float3 position = gEmitter.translate;
                        position.x += dir2.x * dist;
                        position.z += dir2.y * dist;
                        position.y += rand3dTo1d(seed + 1.0f) * 0.2f; // 少し高さを持たせる
                    
                        particle.translate = position; // 位置
                    
                        // マイクラ風の小さめの四角
                        particle.scale = float3(0.4f, 0.4f, 0.4f);
                    
                        // ちょっと外側 + 上向きに飛ぶ
                        float horizontalSpeed = 1.5f + rand3dTo1d(seed + 3.0f) * 1.0f;
                        float3 dirXZ = float3(dir2.x, 0.0f, dir2.y);
                        float upSpeed = 2.0f + rand3dTo1d(seed + 4.0f) * 1.5f;
                    
                        particle.velocity = float3(dirXZ.x * horizontalSpeed, upSpeed, dirXZ.z * horizontalSpeed);
                    
                        // 寿命短め
                        particle.lifeTime = 0.5f + rand3dTo1d(seed + 5.0f) * 0.3;
                    
                        // 砂っぽい色
                        float color = lerp(0.3f, 0.6f, rand3dTo1d(seed + 6.0f));
                        particle.color = float4(color * 0.9f, color * 0.8f, color * 0.7f, 1.0f);
                    }
                    break;
                
                case GPU_PARTICLE_TYPE_BOSS_AURA: // ボスオーラパーティクルタイプ
                    {
                       // seed はループの頭で float3 seed = ...; とか作ってある前提
                        float3 r3 = rand3dTo3d(seed);

                        // エミッター中心から半径内でランダム
                        float2 r2 = r3.xz * 2.0f - 1.0f; // -1 ～ 1
                        float2 dir2 = normalize(r2);
                        float dist = rand3dTo1d(seed + 1.0f) * gEmitter.radius;

                        float3 pos = gEmitter.translate;
                        pos.x += dir2.x * dist;
                        pos.z += dir2.y * dist;
                        pos.y += rand3dTo1d(seed + 2.0f) * 0.6f; // 上下に少しバラける

                        particle.translate = pos;

                        // 小さめの四角
                        particle.scale = float3(0.1f, 0.1f, 0.1f);

                        // ほとんど動かない（ふわっと上に上がる程度）
                        float upSpeed = 0.5f + rand3dTo1d(seed + 3.0f) * 0.5f;
                        float swirl = (rand3dTo1d(seed + 4.0f) - 0.5f) * 0.3f;
                        float swirl2 = (rand3dTo1d(seed + 5.0f) - 0.5f) * 0.3f;

                        particle.velocity = float3(swirl, upSpeed, swirl2);

                        // 寿命は少し長め
                        particle.lifeTime = 1.0f + rand3dTo1d(seed + 6.0f) * 0.7f;

                        // オーラ色（好みで変えてOK）
                        // 例：緑がかったエフェクト
                        float base = 0.6f + rand3dTo1d(seed + 7.0f) * 0.3f;
                        particle.color = float4(0.2f, base, 0.3f, 1.0f);
                    }
                    break;
                
                case GPU_PARTICLE_TYPE_BOSS_RUSH_TRAIL: // ボス突進残像トレイル
                    {
                        float3 r3 = rand3dTo3d(seed);

                        // 発生位置：中心 + 少しだけブレ（radiusはC++側で細く）
                        float2 jitter = (r3.xz * 2.0f - 1.0f) * gEmitter.radius;
                        float3 pos = gEmitter.translate;
                        pos.x += jitter.x;
                        pos.z += jitter.y;
                        pos.y += (rand3dTo1d(seed + 2.0f) - 0.5f) * 0.15f;
                        particle.translate = pos;

                        // 細長い四角
                        float w = 0.25f + rand3dTo1d(seed + 4.0f) * 0.08f;
                        float h = 0.9f + rand3dTo1d(seed + 3.0f) * 0.7f;
                        particle.scale = float3(w, h, 1.0f);

                        // ほぼ止める（残像なので）
                        particle.velocity = float3(0, 0, 0);

                        // 短命
                        particle.lifeTime = 0.15f + rand3dTo1d(seed + 5.0f) * 0.15f;

                        // 薄い（フェードは Update側 or PS側で age から掛けるのが理想）
                        float a = 0.25f + rand3dTo1d(seed + 6.0f) * 0.15f;
                        particle.color = float4(1, 1, 1, a);
                    }
                    break;
                
                case GPU_PARTICLE_TYPE_BOSS_SHOCKWAVE: // 衝撃波パーティクルタイプ
                    {
                        float3 r3 = rand3dTo3d(seed);
                        // エミッター中心から半径内でランダム
                        float2 r2 = r3.xz * 2.0f - 1.0f; // -1 ～ 1
                        float2 dir2 = normalize(r2);
                        float dist = rand3dTo1d(seed + 1.0f) * gEmitter.radius;
                        float3 pos = gEmitter.translate;
                        pos.x += dir2.x * dist;
                        pos.z += dir2.y * dist;
                        pos.y += rand3dTo1d(seed + 2.0f) * 0.4f; // 上下に少しバラける
                        particle.translate = pos;
                        // 中くらいの四角
                        particle.scale = float3(0.5f, 0.5f, 0.5f);
                        // 外側に広がるように飛ぶ
                        float speed = 3.0f + rand3dTo1d(seed + 3.0f) * 20.0f;
                        float3 dirXZ = float3(dir2.x, 0.0f, dir2.y);
                        particle.velocity = dirXZ * speed;
                        // 寿命中くらい
                        particle.lifeTime = 0.4f + rand3dTo1d(seed + 4.0f) * 0.5f;
                        // 薄い灰色
                        float c = lerp(0.5f, 0.8f, rand3dTo1d(seed + 5.0f));
                        particle.color = float4(c, c, c, 1.0f);
                    }
                    break;

                case GPU_PARTICLE_TYPE_SPIN_ATTACK_SLASH: // 旋風攻撃パーティクルタイプ
                    {
                        float3 r3 = rand3dTo3d(seed);
                        // エミッター中心から半径内でランダム
                        float2 r2 = r3.xz * 2.0f - 1.0f; // -1 ～ 1
                        float2 dir2 = normalize(r2);
                        float dist = rand3dTo1d(seed + 1.0f) * gEmitter.radius;
                        float3 pos = gEmitter.translate;
                        pos.x += dir2.x * dist;
                        pos.z += dir2.y * dist;
                        pos.y += rand3dTo1d(seed + 2.0f) * 0.6f; // 上下に少しバラける
                        particle.translate = pos;
                        // 中くらいの四角
                        particle.scale = float3(0.3f, 0.3f, 0.3f);
                        // 外側に広がるように飛ぶ
                        float speed = 200.0f + rand3dTo1d(seed + 3.0f) * 50.0f;
                        float3 dirXZ = float3(dir2.x, 0.0f, dir2.y);
                        particle.velocity = dirXZ * speed;
                        // 寿命中くらい
                        particle.lifeTime = 0.25f + rand3dTo1d(seed + 4.0f) * 0.5f;
                        // 薄い青白い色
                        float c = lerp(0.5f, 0.8f, rand3dTo1d(seed + 5.0f));
                        particle.color = float4(c * 0.7f, c * 0.8f, c, 1.0f);
                    }
                    break;
                case GPU_PARTICLE_TYPE_BOSS_DEATH_SOUL:
                    {
                        float2 d2 = float2(rand3dTo1d(seed + 1.0f) * 2.0f - 1.0f,
                       rand3dTo1d(seed + 2.0f) * 2.0f - 1.0f);
                        d2 = normalize(d2);

                        float dist = rand3dTo1d(seed + 3.0f) * gEmitter.radius;

                        float3 pos = gEmitter.translate;
                        pos.x += d2.x * dist;
                        pos.z += d2.y * dist;
                        pos.y += rand3dTo1d(seed + 4.0f) * 0.3f;

                        particle.translate = pos;

                        float up = 0.8f + rand3dTo1d(seed + 5.0f) * 1.0f;
                        particle.velocity = float3(d2.x * 0.25f, up, d2.y * 0.25f);

                        float s = 0.10f + rand3dTo1d(seed + 6.0f) * 0.10f;
                        particle.scale = float3(s, s * 1.4f, s);

                        particle.currentTime = 0.0f;
                        particle.lifeTime = 1.2f + rand3dTo1d(seed + 7.0f) * 1.2f;

                        float t = rand3dTo1d(seed + 8.0f);
                        float3 c = lerp(float3(1, 1, 1), float3(0.45f, 0.75f, 1.0f), t);
                        particle.color = float4(c, 0.65f);
                        break;
                    }

                case GPU_PARTICLE_TYPE_BOSS_DEBRIS_DUST:
                    {
                        float2 d2 = float2(rand3dTo1d(seed + 1.0f) * 2.0f - 1.0f,
                       rand3dTo1d(seed + 2.0f) * 2.0f - 1.0f);
                        d2 = normalize(d2);

                        float dist = rand3dTo1d(seed + 3.0f) * gEmitter.radius;

                        float3 pos = gEmitter.translate;
                        pos.x += d2.x * dist;
                        pos.z += d2.y * dist;
                        pos.y += rand3dTo1d(seed + 4.0f) * 0.2f;

                        particle.translate = pos;

                        float sp = 0.4f + rand3dTo1d(seed + 5.0f) * 0.8f;
                        particle.velocity = float3(d2.x * sp, 0.2f + rand3dTo1d(seed + 6.0f) * 0.4f, d2.y * sp);

                        float s = 0.03f + rand3dTo1d(seed + 7.0f) * 0.05f;
                        particle.scale = float3(s, s, s);

                        particle.currentTime = 0.0f;
                        particle.lifeTime = 0.18f + rand3dTo1d(seed + 8.0f) * 0.25f;

                        float g = 0.45f + rand3dTo1d(seed + 9.0f) * 0.15f;
                        particle.color = float4(g, g, g, 0.5f);
                        break;
                    }
            }
            
            // 共通設定
            particle.currentTime = 0.0f;
            particle.type = gEmitter.type;
            particle.billboardMode = gEmitter.billboardMode;
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