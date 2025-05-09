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
static const float kPrewittHorizontalKernel[3][3] =
{
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f }
};

// Prewittフィルタの縦方向のカーネル
static const float kPrewittVerticalKernel[3][3] =
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
SamplerState gSampler : register(s0);

ConstantBuffer<LuminanceOutlineSetting> gLuminanceOutlineSetting : register(b0);

// ピクセルシェーダー
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float2 difference = float2(0.0f, 0.0f);
    
    float gx = 0.0f;
    float gy = 0.0f;
    
    // Prewittフィルタを適用して輝度を計算
    for (int x = 0; x < 3; ++x)
    {
        for (int y = 0; y < 3; ++y)
        {
            float2 offset = kIndex3x3[x][y] * gLuminanceOutlineSetting.texelSize;
            float2 texcoord = input.texcoord + offset;
            float luminance = Luminance(gTexture.Sample(gSampler, texcoord).rgb);

            gx += luminance * kPrewittHorizontalKernel[x][y];
            gy += luminance * kPrewittVerticalKernel[x][y];
        }
    }
    float weight = sqrt(gx * gx + gy * gy) * gLuminanceOutlineSetting.edgeStrength;

   // 閾値処理＋サチュレーション
    weight = (weight > gLuminanceOutlineSetting.threshold) ? weight : 0.0f;
    weight = saturate(weight);

    float3 originalColor = gTexture.Sample(gSampler, input.texcoord).rgb;
    output.color.rgb = (1.0f - weight) * originalColor;
    output.color.a = 1.0f;
    return output;
}
