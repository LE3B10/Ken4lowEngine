// パーティクルタイプ定数
static const uint GPU_PARTICLE_TYPE_DEFAULT = 0; // デフォルトパーティクルタイプ : ☑実装済み
static const uint GPU_PARTICLE_TYPE_MUZZLEFLASH = 1; // マズルフラッシュパーティクルタイプ
static const uint GPU_PARTICLE_TYPE_BULLETTRACER = 2; // 弾道パーティクルタイプ
static const uint GPU_PARTICLE_TYPE_HITSPARK = 3; // 命中火花パーティクルタイプ
static const uint GPU_PARTICLE_TYPE_BLOOD = 4; // 血しぶきパーティクルタイプ
static const uint GPU_PARTICLE_TYPE_IMPACT_DUST = 5; // 地面衝撃パーティクルタイプ
static const uint GPU_PARTICLE_TYPE_IMPACT_METAL = 6; // 金属衝撃パーティクルタイプ
static const uint GPU_PARTICLE_TYPE_IMPACT_WOOD = 7; // 木材衝撃パーティクルタイプ
static const uint GPU_PARTICLE_TYPE_EXPLOSION_FIRE = 8; // 爆発火炎パーティクルタイプ : ☑実装済み
static const uint GPU_PARTICLE_TYPE_EXPLOSION_SMOKE = 9; // 爆発煙パーティクルタイプ
static const uint GPU_PARTICLE_TYPE_FOOT_DUST = 10; // 足元砂埃パーティクルタイプ
static const uint GPU_PARTICLE_TYPE_ENV_DUST = 11; // 環境砂埃パーティクルタイプ
static const uint GPU_PARTICLE_TYPE_PICKUP_GLOW = 12; // アイテム取得光パーティクルタイプ
static const uint GPU_PARTICLE_TYPE_SKILL_EFFECT = 13; // スキルエフェクトパーティクルタイプ
static const uint GPU_PARTCILE_TYPE_BOSS_APPEAR_DUST = 14; // ボス登場砂埃パーティクルタイプ : ☑実装済み
static const uint GPU_PARTICLE_TYPE_BOSS_AURA = 15; // ボスオーラパーティクルタイプ : ☑実装済み
static const uint GPU_PARTICLE_TYPE_BOSS_RUSH_TRAIL = 16; // ボス突進残像トレイル : ☑実装済み
static const uint GPU_PARTICLE_TYPE_BOSS_SHOCKWAVE = 17; // 衝撃波パーティクルタイプ : ☑実装済み
static const uint GPU_PARTICLE_TYPE_SPIN_ATTACK_SLASH = 18; // 旋風攻撃パーティクルタイプ : ☑実装済み
static const uint GPU_PARTICLE_TYPE_BOSS_DEATH_SOUL = 19; // ボス死亡魂パーティクルタイプ : ☑実装済み
static const uint GPU_PARTICLE_TYPE_BOSS_DEBRIS_DUST = 20; // ボス破片砂埃パーティクルタイプ : ☑実装済み
static const uint GPU_PARTICLE_TYPE_HEAL = 21; // 回復パーティクルタイプ : ☑実装済み

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
