#include "GpuParticle.hlsli" //頂点シェーダーへの入力頂点構造
#include "GpuParticleData.hlsli" //パーティクルデータ構造体"

// ビルボードモードフラグ
static const uint BILLBOARD_NONE = 0; // ビルボードなし
static const uint BILLBOARD_CAMERA = 1 << 0; // カメラ方向ビルボード
static const uint BILLBOARD_YAXIS = 1 << 1; // Y軸回転ビルボード

// 頂点シェーダーの出力頂点構造
struct VertexShaderInput
{
    //POSITIONのことをセマンティクスという
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

// 頂点シェーダーの出力頂点構造 
struct PerView
{
    float4x4 viewProjectionMatrix; // ビュー射影行列
    float4x4 billboardMatrix; // ビルボード行列
    uint bollboardMode; // ビルボードモード
    float3 padding; // パディング
};

// ビルボードモードチェック関数
bool IsBillboardMode(uint mode, uint flag)
{
    return (mode & flag) != 0;
}

StructuredBuffer<Particle> gParticles : register(t0); // 読み取り可能なパーティクルバッファ
ConstantBuffer<PerView> gPerView : register(b0); // ビュー情報

// 頂点シェーダー 
VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    Particle particle = gParticles[instanceId]; // インスタンスIDに基づいてパーティクルデータを取得
    float4x4 worldMatrix;
    
    // ベース行列を分岐で決める（ここだけ追加）
    if (IsBillboardMode(particle.billboardMode, BILLBOARD_CAMERA))
    {
        // カメラビルボード
        worldMatrix = gPerView.billboardMatrix;
    }
    else if (IsBillboardMode(particle.billboardMode, BILLBOARD_YAXIS))
    {
        // Y軸ビルボード用
        worldMatrix = gPerView.billboardMatrix;
    }
    else
    {
        // ビルボードなし：ふつうのローカル→ワールド
        worldMatrix =
        float4x4(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        );
    }
    
    // スケールと座標移動をワールド行列に適用
    worldMatrix[0] *= particle.scale.x; // スケール適用
    worldMatrix[1] *= particle.scale.y; // スケール適用
    worldMatrix[2] *= particle.scale.z; // スケール適用
    worldMatrix[3].xyz += particle.translate; // 座標移動適用 // 頂点位置をワールドビュー射影変換
    
    output.position = mul(input.position, mul(worldMatrix, gPerView.viewProjectionMatrix)); // ワールドビュー射影変換 
    output.texcoord = input.texcoord; // テクスチャ座標をそのまま渡す 
    output.color = particle.color; // パーティクルの色を頂点シェーダー出力に渡す
    output.type = particle.type; // パーティクルのタイプを頂点シェーダー出力に渡す

    // 出力頂点構造を返す
    return output;
}