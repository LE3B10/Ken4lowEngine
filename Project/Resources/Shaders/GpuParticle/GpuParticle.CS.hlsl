// GPUパーティクルシステムのコンピュートシェーダー

#include "GpuParticleData.hlsli" // パーティクルデータ構造体

static const uint kMaxParticleCount = 1024; // 最大パーティクル数65536

RWStructuredBuffer<Particle> gParticleBuffer : register(u0); // 書き込み可能なパーティクルバッファ
RWStructuredBuffer<int> gFreeListIndex : register(u1); // フリーリストインデックスバッファ
RWStructuredBuffer<uint> gFreeList : register(u2); // フリーリストバッファ

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x; // パーティクルのインデックス
    
    // パーティクルインデックスが有効範囲内か確認
    if (particleIndex >= kMaxParticleCount)
        return;
    
    // フリーカウンタをリセット
    if (particleIndex == 0)
        gFreeListIndex[0] = 0;
    
    // パーティクルデータをバッファから取得
    Particle particle = gParticleBuffer[particleIndex];
    
    particle = (Particle) 0; // 初期化
    
    particle.translate = float3(0.0f, 2.0f, 0.0f);
    particle.scale = 0.0f.xxx;
    particle.velocity = float3(0.0f, 0.0f, 0.0f);
    particle.color = float4(0.0, 0.0, 0.0, 0.0); // 色をリセット
    particle.lifeTime = 0.0f; // 生存時間をリセット
    particle.currentTime = 0.0f; // 現在の時間をリセット
    
    // パーティクルデータをバッファに書き戻す
    gParticleBuffer[particleIndex] = particle;
    
    // フリーリストにインデックスを追加
    gFreeList[particleIndex] = particleIndex;
    
    // 先頭スレッドでフリーリストインデックスをインクリメント
    if (particleIndex == 0)
    {
        // 配列末尾が最初の空きインデックスになるように設定
        gFreeListIndex[0] = (int) (kMaxParticleCount - 1);
    }
}