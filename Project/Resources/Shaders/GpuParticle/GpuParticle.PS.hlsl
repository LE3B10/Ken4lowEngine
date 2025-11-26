#include "GpuParticle.hlsli"

// マテリアル構造体
struct Material
{
    float4 color; // マテリアルカラー
    float4x4 uvTransform; // UV変換行列
    uint drawType; // 描画タイプ
    float3 _pad; // パディング
};

//ピクセルシェーダーの出力
struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

ConstantBuffer<Material> gMaterial : register(b1);
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

//ピクセルシェーダー
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // この描画パスの対象(type)じゃない粒子は捨てる
    if (input.type != gMaterial.drawType)
    {
        discard;
    }
    
    //TextureをSamplingする
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // 出力色にサンプルしたテクスチャの色を適用
    output.color = gMaterial.color * textureColor * input.color;

    // output.colorのα値が0の時にPixelを棄却
    if (output.color.a == 0.0)
    {
        discard;
    }
    
    return output;
}
