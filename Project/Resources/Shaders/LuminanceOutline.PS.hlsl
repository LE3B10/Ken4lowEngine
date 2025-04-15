#include "FullScreen.hlsli"

struct LuminanceOutlineSetting
{
    float2 texelSize; // テクセルサイズ（1/画面解像度）
    float edgeStrength; // エッジの強さ
    float threshold; // 閾値
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// 3×3のインデックス
static const float2 kIndex3x3[3][3] =
{
    { { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f } },
    { { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } },
    { { -1.0f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } }
};

// Prewittフィルタの横方向のカーネル
static const float kPrewinttHorizontalKernel[3][3] =
{
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f }
};

// Prewittフィルタの縦方向のカーネル
static const float kPrewinttVerticalKernel[3][3] =
{
    { -1.0f / 6.0f, -1.0f / 6.0f, -1.0f / 6.0f },
    { 0.0f, 0.0f, 0.0f },
    { 1.0f / 6.0f, 1.0f / 6.0f, 1.0f / 6.0f }
};

// RGBを輝度に変換する関数
float Luminance(float3 v)
{
    return dot(v, float3(0.2125f, 0.7154f, 0.0721f));
}

Texture2D<float4> gTexture : register(t0);
Texture2D<float> gDepthTexture : register(t1); // 深度テクスチャ

SamplerState gSampler : register(s0);
SamplerState gDepthSampler : register(s1); // 深度サンプラー

ConstantBuffer<LuminanceOutlineSetting> gLuminanceOutlineSetting : register(b0);

// ピクセルシェーダー
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float2 difference = float2(0.0f, 0.0f);
    
    // Prewittフィルタを適用して輝度を計算
    for (int x = 0; x < 3; ++x)
    {
        for (int y = 0; y < 3; ++y)
        {
            float2 offset = kIndex3x3[x][y] * gLuminanceOutlineSetting.texelSize;
            float2 texcoord = input.texcoord + offset;
            float3 color = gTexture.Sample(gSampler, texcoord).rgb;
            float luminance = Luminance(color);

            difference += luminance * float2(
                kPrewinttHorizontalKernel[x][y],
                kPrewinttVerticalKernel[x][y]
            );
        }
    }

    //// 深度テクスチャから中心の深度を取得
    //float centerDepth = gDepthTexture.Sample(gDepthSampler, input.texcoord);

    //// Prewittフィルタを適用して深度の差分を計算
    //for (int x = 0; x < 3; ++x)
    //{
    //    for (int y = 0; y < 3; ++y)
    //    {
    //        float2 offset = kIndex3x3[x][y] * gLuminanceOutlineSetting.texelSize;
    //        float2 texcoord = input.texcoord + offset;
        
    //        float sampleDepth = gDepthTexture.Sample(gDepthSampler, texcoord);

    //        float depthDiff = abs(sampleDepth - centerDepth);

    //        difference += depthDiff * float2(
    //        kPrewinttHorizontalKernel[x][y],
    //        kPrewinttVerticalKernel[x][y]
    //    );
    //    }
    //}
    
    float weight = length(difference) * gLuminanceOutlineSetting.edgeStrength;

    // 閾値処理：指定以下ならゼロにカット
    weight = (weight > gLuminanceOutlineSetting.threshold) ? weight : 0.0f;
    weight = saturate(weight * 6.0f);

    output.color.rgb = (1.0f - weight) * gTexture.Sample(gSampler, input.texcoord).rgb;
    output.color.a = 1.0f;
    return output;
}
