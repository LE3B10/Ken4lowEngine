// パーティクルデータ構造体
struct Particle
{
    float3 translate; // 座標移動 
    float3 scale; // スケール
    float lifeTime; // 生存時間 
    float3 velocity; // 速度
    float currentTime; // 現在の時間
    float4 color; // 色
};
