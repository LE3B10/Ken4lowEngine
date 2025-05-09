#include "FullScreen.hlsli"

struct DepthOutlineSetting
{
    float2 texelSize; // テクセルサイズ（1/画面解像度）
    float edgeStrength; // エッジの強さ
    float threshold; // 閾値
    float4x4 projectionInverse; // 逆投影行列（View空間Z復元に使用）
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

Texture2D<float> gDepthTexture : register(t1); // 深度テクスチャ
SamplerState gDepthSampler : register(s1); // 深度サンプラー

ConstantBuffer<DepthOutlineSetting> gDepthOutlineSetting : register(b0);

// ピクセルシェーダー
PixelShaderOutput main(VertexShaderOutput input)
{
    float gx = 0.0f;
    float gy = 0.0f;

    // Prewittフィルタでview空間Zの勾配を検出
    for (int x = 0; x < 3; ++x)
    {
        for (int y = 0; y < 3; ++y)
        {
            float2 offset = kIndex3x3[x][y] * gDepthOutlineSetting.texelSize;
            float2 texcoord = input.texcoord + offset;

            float ndcDepth = gDepthTexture.Sample(gDepthSampler, texcoord);

            float4 ndcPos = float4(0.0f, 0.0f, ndcDepth, 1.0f);
            float4 viewPos = mul(ndcPos, gDepthOutlineSetting.projectionInverse);
            viewPos /= viewPos.w;

            float viewZ = viewPos.z;

            gx += viewZ * kPrewittHorizontalKernel[x][y];
            gy += viewZ * kPrewittVerticalKernel[x][y];
        }
    }

    float edge = sqrt(gx * gx + gy * gy) * gDepthOutlineSetting.edgeStrength;

    // 閾値処理
    edge = (edge > gDepthOutlineSetting.threshold) ? edge : 0.0f;
    edge = saturate(edge);

    float3 originalColor = gTexture.Sample(gSampler, input.texcoord).rgb;
    float3 outlineColor = float3(0.0f, 0.0f, 0.0f); // 黒いアウトライン

    PixelShaderOutput output;
    output.color.rgb = lerp(originalColor, outlineColor, edge);
    output.color.a = 1.0f;
    return output;
}
