/// ---------- Config ---------- ///
static const uint kMaxParticleCount = 131072; // 2^17 : 最大パーティクル数

/// ---------- 描画種類 ---------- ///
static const uint GPU_PARTICLE_KIND_SHIFT = 16;
static const uint GPU_PARTICLE_KIND_MASK = 0x00FF0000;
static const uint GPU_PARTICLE_BB_MASK = 0x0000FFFF;

static const uint GPU_PARTICLE_KIND_SPRITE = 0; // Sprite / Billboard
static const uint GPU_PARTICLE_KIND_MESH = 1; // Mesh Particle (instanced mesh)
static const uint GPU_PARTICLE_KIND_RIBBON = 2; // Ribbon / Trail
static const uint GPU_PARTICLE_KIND_BEAM = 3; // Beam (start -> end)

/// ---------- ビルボードフラグ ---------- ///
static const uint BILLBOARD_NONE = 0; // ビルボードなし
static const uint BILLBOARD_CAMERA = 1u << 0; // カメラ方向ビルボード
static const uint BILLBOARD_YAXIS = 1u << 1; // Y軸回転ビルボード
// 拡張用
static const uint BILLBOARD_RIBBON = 1u << 2; // 速度方向に伸ばす疑似リボン（トレイル / トレーサー用）

// パック
uint GPUParticle_GetKind(uint packedBillboardMode)
{
    return (packedBillboardMode & GPU_PARTICLE_KIND_MASK) >> GPU_PARTICLE_KIND_SHIFT;
}
uint GPUParticle_GetBillboardFlags(uint packedBillboardMode)
{
    return (packedBillboardMode & GPU_PARTICLE_BB_MASK);
}
uint GPUParticle_PackBillboardMode(uint kind, uint billboardFlags)
{
    return ((kind << GPU_PARTICLE_KIND_SHIFT) & GPU_PARTICLE_KIND_MASK) | (billboardFlags & GPU_PARTICLE_BB_MASK);
}

// パーティクルタイプ定数
static const uint GPU_PARTICLE_TYPE_DEFAULT = 0; // デフォルトパーティクルタイプ
static const uint GPU_PARTICLE_TYPE_MUZZLEFLASH = 1; // マズルフラッシュパーティクルタイプ
static const uint GPU_PARTICLE_TYPE_BULLETTRACER = 2; // 弾道パーティクルタイプ
static const uint GPU_PARTICLE_TYPE_HITSPARK = 3; // 命中火花パーティクルタイプ
static const uint GPU_PARTICLE_TYPE_BLOOD = 4; // 血しぶきパーティクルタイプ
static const uint GPU_PARTICLE_TYPE_IMPACT_DUST = 5; // 地面衝撃パーティクルタイプ
static const uint GPU_PARTICLE_TYPE_IMPACT_METAL = 6; // 金属衝撃パーティクルタイプ
static const uint GPU_PARTICLE_TYPE_IMPACT_WOOD = 7; // 木材衝撃パーティクルタイプ
static const uint GPU_PARTICLE_TYPE_EXPLOSION_FIRE = 8; // 爆発火炎パーティクルタイプ
static const uint GPU_PARTICLE_TYPE_EXPLOSION_SMOKE = 9; // 爆発煙パーティクルタイプ
static const uint GPU_PARTICLE_TYPE_FOOT_DUST = 10; // 足元砂埃パーティクルタイプ
static const uint GPU_PARTICLE_TYPE_ENV_DUST = 11; // 環境砂埃パーティクルタイプ
static const uint GPU_PARTICLE_TYPE_PICKUP_GLOW = 12; // アイテム取得光パーティクルタイプ
static const uint GPU_PARTICLE_TYPE_SKILL_EFFECT = 13; // スキルエフェクトパーティクルタイプ
static const uint GPU_PARTICLE_TYPE_BOSS_APPEAR_DUST = 14; // ボス登場砂埃パーティクルタイプ
static const uint GPU_PARTICLE_TYPE_BOSS_AURA = 15; // ボスオーラパーティクルタイプ
static const uint GPU_PARTICLE_TYPE_BOSS_RUSH_TRAIL = 16; // ボス突進残像トレイル
static const uint GPU_PARTICLE_TYPE_BOSS_SHOCKWAVE = 17; // 衝撃波パーティクルタイプ
static const uint GPU_PARTICLE_TYPE_SPIN_ATTACK_SLASH = 18; // 旋風攻撃パーティクルタイプ
static const uint GPU_PARTICLE_TYPE_BOSS_DEATH_SOUL = 19; // ボス死亡魂パーティクルタイプ
static const uint GPU_PARTICLE_TYPE_BOSS_DEBRIS_DUST = 20; // ボス破片砂埃パーティクルタイプ
static const uint GPU_PARTICLE_TYPE_HEAL = 21; // 回復パーティクルタイプ

// パーティクルデータ構造体
struct Particle
{
    float3 translate; // 座標移動 
    float3 scale; // スケール
    float lifeTime; // 生存時間 
    float3 velocity; // 速度
    float currentTime; // 現在の時間
    uint type; // パーティクルタイプ
    uint billboardMode; // ビルボードモードフラグ
    float4 color; // 色
};

// エミッタースフィア構造体
struct EmitterCBData
{
    float3 translate; // 位置
    float radius; // 半径
    uint count; // 発生数
    float frequency; // 発生頻度
    float frequencyTime; // 発生頻度タイマー
    uint emit; // 発生フラグ
    uint type; // パーティクルタイプ : GPU_PARTICLE_TYPE_***
    uint billboardMode; // ビルボードモードフラグ
};

// 時間制御用定数
struct PerFrame
{
    float time; // 経過時間
    float deltaTime; // フレーム間の時間差
};

// ---------- Random helpers ----------
float3 GPURand3(float3 seed)
{
    seed = frac(seed * 0.1031f);
    seed += dot(seed, seed.yzx + 33.33f);
    return frac((seed.xxy + seed.yzz) * seed.zyx);
}
float GPURand1(float3 seed)
{
    return GPURand3(seed).x;
}