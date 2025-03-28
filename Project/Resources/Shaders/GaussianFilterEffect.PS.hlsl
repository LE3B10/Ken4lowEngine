#include "FullScreen.hlsli"

// 円周率
static const float PI = 3.1415926535897932f;

// 構造体
struct GaussianFilterSetting
{
    float intensity; // 強度
    float threshold; // 閾値
    float sigma; // ガウス関数の標準偏差
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// ガウシアンフィルタ
ConstantBuffer<GaussianFilterSetting> gGaussianFilterSetting : register(b0);
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// ガウス関数
float Gaussian(float x, float y, float sigma)
{
    float exponent = -(x * x + y * y) * rcp(2.0f * sigma * sigma);
    float denominator = 2.0f * PI * sigma * sigma;
    return exp(exponent) * rcp(denominator);
}

// 3×3のインデックス
static const float2 kIndex3x3[3][3] =
{
    { { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f } },
    { { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } },
    { { -1.0f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } }
};

// 5×5 のオフセット
static const float2 kIndex5x5[5][5] =
{
    { { -2.0f, -2.0f }, { -1.0f, -2.0f }, { 0.0f, -2.0f }, { 1.0f, -2.0f }, { 2.0f, -2.0f } },
    { { -2.0f, -1.0f }, { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f }, { 2.0f, -1.0f } },
    { { -2.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 2.0f, 0.0f } },
    { { -2.0f, 1.0f }, { -1.0f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 2.0f, 1.0f } },
    { { -2.0f, 2.0f }, { -1.0f, 2.0f }, { 0.0f, 2.0f }, { 1.0f, 2.0f }, { 2.0f, 2.0f } }
};

// 7×7 のインデックス
static const float2 kIndex7x7[7][7] =
{
    { { -3.0f, -3.0f }, { -2.0f, -3.0f }, { -1.0f, -3.0f }, { 0.0f, -3.0f }, { 1.0f, -3.0f }, { 2.0f, -3.0f }, { 3.0f, -3.0f } },
    { { -3.0f, -2.0f }, { -2.0f, -2.0f }, { -1.0f, -2.0f }, { 0.0f, -2.0f }, { 1.0f, -2.0f }, { 2.0f, -2.0f }, { 3.0f, -2.0f } },
    { { -3.0f, -1.0f }, { -2.0f, -1.0f }, { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f }, { 2.0f, -1.0f }, { 3.0f, -1.0f } },
    { { -3.0f, 0.0f }, { -2.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 2.0f, 0.0f }, { 3.0f, 0.0f } },
    { { -3.0f, 1.0f }, { -2.0f, 1.0f }, { -1.0f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 2.0f, 1.0f }, { 3.0f, 1.0f } },
    { { -3.0f, 2.0f }, { -2.0f, 2.0f }, { -1.0f, 2.0f }, { 0.0f, 2.0f }, { 1.0f, 2.0f }, { 2.0f, 2.0f }, { 3.0f, 2.0f } },
    { { -3.0f, 3.0f }, { -2.0f, 3.0f }, { -1.0f, 3.0f }, { 0.0f, 3.0f }, { 1.0f, 3.0f }, { 2.0f, 3.0f }, { 3.0f, 3.0f } }
};

// 9×9 のインデックス
static const float2 kIndex9x9[9][9] =
{
    { float2(-4.0f, -4.0f), float2(-3.0f, -4.0f), float2(-2.0f, -4.0f), float2(-1.0f, -4.0f), float2(0.0f, -4.0f), float2(1.0f, -4.0f), float2(2.0f, -4.0f), float2(3.0f, -4.0f), float2(4.0f, -4.0f) },
    { float2(-4.0f, -3.0f), float2(-3.0f, -3.0f), float2(-2.0f, -3.0f), float2(-1.0f, -3.0f), float2(0.0f, -3.0f), float2(1.0f, -3.0f), float2(2.0f, -3.0f), float2(3.0f, -3.0f), float2(4.0f, -3.0f) },
    { float2(-4.0f, -2.0f), float2(-3.0f, -2.0f), float2(-2.0f, -2.0f), float2(-1.0f, -2.0f), float2(0.0f, -2.0f), float2(1.0f, -2.0f), float2(2.0f, -2.0f), float2(3.0f, -2.0f), float2(4.0f, -2.0f) },
    { float2(-4.0f, -1.0f), float2(-3.0f, -1.0f), float2(-2.0f, -1.0f), float2(-1.0f, -1.0f), float2(0.0f, -1.0f), float2(1.0f, -1.0f), float2(2.0f, -1.0f), float2(3.0f, -1.0f), float2(4.0f, -1.0f) },
    { float2(-4.0f, 0.0f), float2(-3.0f, 0.0f), float2(-2.0f, 0.0f), float2(-1.0f, 0.0f), float2(0.0f, 0.0f), float2(1.0f, 0.0f), float2(2.0f, 0.0f), float2(3.0f, 0.0f), float2(4.0f, 0.0f) },
    { float2(-4.0f, 1.0f), float2(-3.0f, 1.0f), float2(-2.0f, 1.0f), float2(-1.0f, 1.0f), float2(0.0f, 1.0f), float2(1.0f, 1.0f), float2(2.0f, 1.0f), float2(3.0f, 1.0f), float2(4.0f, 1.0f) },
    { float2(-4.0f, 2.0f), float2(-3.0f, 2.0f), float2(-2.0f, 2.0f), float2(-1.0f, 2.0f), float2(0.0f, 2.0f), float2(1.0f, 2.0f), float2(2.0f, 2.0f), float2(3.0f, 2.0f), float2(4.0f, 2.0f) },
    { float2(-4.0f, 3.0f), float2(-3.0f, 3.0f), float2(-2.0f, 3.0f), float2(-1.0f, 3.0f), float2(0.0f, 3.0f), float2(1.0f, 3.0f), float2(2.0f, 3.0f), float2(3.0f, 3.0f), float2(4.0f, 3.0f) },
    { float2(-4.0f, 4.0f), float2(-3.0f, 4.0f), float2(-2.0f, 4.0f), float2(-1.0f, 4.0f), float2(0.0f, 4.0f), float2(1.0f, 4.0f), float2(2.0f, 4.0f), float2(3.0f, 4.0f), float2(4.0f, 4.0f) }
};

PixelShaderOutput main(VertexShaderOutput input)
{
    uint width, height;
    gTexture.GetDimensions(width, height);
    float2 uvStepSize = rcp(float2(width, height));
    
    PixelShaderOutput output;
    output.color.rgb = float3(0.0f, 0.0f, 0.0f);
    
    float kernel[9][9]; // 9x9カーネル
    float weight = 0.0f;

    // **カーネルを生成**
    for (int i = 0; i < 9; ++i)
    {
        for (int j = 0; j < 9; ++j)
        {
            float2 offset = kIndex9x9[i][j];
            kernel[i][j] = Gaussian(offset.x, offset.y, gGaussianFilterSetting.sigma);
            weight += kernel[i][j]; // 正規化用
        }
    }

    // **フィルタ適用**
    for (int k = 0; k < 9; ++k)
    {
        for (int l = 0; l < 9; ++l)
        {
            float2 offsetUV = input.texcoord + kIndex9x9[k][l] * uvStepSize;
            output.color.rgb += gTexture.Sample(gSampler, offsetUV).rgb * (kernel[k][l] / weight);
        }
    }
    output.color *= gGaussianFilterSetting.intensity;
    // 元の画像のアルファ値をそのまま使う
    output.color.a = gTexture.Sample(gSampler, input.texcoord).a;
    
    return output;
}