#include "FullScreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

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

// 3×3の均等カーネル（ボックスフィルタ）
static const float kKernel3x3[3][3] =
{
    { 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f },
    { 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f },
    { 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f }
};

// 5×5のインデックス
static const float2 kIndex5x5[5][5] =
{
    { { -2.0f, -2.0f }, { -1.0f, -2.0f }, { 0.0f, -2.0f }, { 1.0f, -2.0f }, { 2.0f, -2.0f } },
    { { -2.0f, -1.0f }, { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f }, { 2.0f, -1.0f } },
    { { -2.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 2.0f, 0.0f } },
    { { -2.0f, 1.0f }, { -1.0f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 2.0f, 1.0f } },
    { { -2.0f, 2.0f }, { -1.0f, 2.0f }, { 0.0f, 2.0f }, { 1.0f, 2.0f }, { 2.0f, 2.0f } }
};

// 5×5 の均等カーネル（ボックスフィルタ）
static const float kKernel5x5[5][5] =
{
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f }
};

// 5×5 のガウスカーネル
static const float kGaussianKernel5x5[5][5] =
{
    { 1.0f / 273.0f, 4.0f / 273.0f, 7.0f / 273.0f, 4.0f / 273.0f, 1.0f / 273.0f },
    { 4.0f / 273.0f, 16.0f / 273.0f, 26.0f / 273.0f, 16.0f / 273.0f, 4.0f / 273.0f },
    { 7.0f / 273.0f, 26.0f / 273.0f, 41.0f / 273.0f, 26.0f / 273.0f, 7.0f / 273.0f },
    { 4.0f / 273.0f, 16.0f / 273.0f, 26.0f / 273.0f, 16.0f / 273.0f, 4.0f / 273.0f },
    { 1.0f / 273.0f, 4.0f / 273.0f, 7.0f / 273.0f, 4.0f / 273.0f, 1.0f / 273.0f }
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

// 7×7 の均等カーネル（ボックスフィルタ）
static const float kKernel7x7[7][7] =
{
    { 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f },
    { 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f },
    { 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f },
    { 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f },
    { 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f },
    { 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f },
    { 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f, 1.0f / 49.0f }
};

// 7×7 のガウスカーネル
static const float kGaussianKernel7x7[7][7] =
{
    { 0.0006, 0.003, 0.007, 0.012, 0.007, 0.003, 0.0006 },
    { 0.003, 0.013, 0.032, 0.048, 0.032, 0.013, 0.003 },
    { 0.007, 0.032, 0.077, 0.115, 0.077, 0.032, 0.007 },
    { 0.012, 0.048, 0.115, 0.180, 0.115, 0.048, 0.012 },
    { 0.007, 0.032, 0.077, 0.115, 0.077, 0.032, 0.007 },
    { 0.003, 0.013, 0.032, 0.048, 0.032, 0.013, 0.003 },
    { 0.0006, 0.003, 0.007, 0.012, 0.007, 0.003, 0.0006 }
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

// 9×9 の均等カーネル（ボックスフィルタ）
static const float kKernel9x9[9][9] =
{
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f },
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f },
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f },
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f },
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f },
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f },
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f },
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f },
    { 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f, 1.0f / 81.0f }
};

// 9×9 のガウスカーネル
static const float kGaussianKernel9x9[9][9] =
{
    { 0.0002, 0.001, 0.003, 0.007, 0.011, 0.007, 0.003, 0.001, 0.0002 },
    { 0.001, 0.005, 0.015, 0.030, 0.040, 0.030, 0.015, 0.005, 0.001 },
    { 0.003, 0.015, 0.040, 0.080, 0.110, 0.080, 0.040, 0.015, 0.003 },
    { 0.007, 0.030, 0.080, 0.140, 0.180, 0.140, 0.080, 0.030, 0.007 },
    { 0.011, 0.040, 0.110, 0.180, 0.220, 0.180, 0.110, 0.040, 0.011 },
    { 0.007, 0.030, 0.080, 0.140, 0.180, 0.140, 0.080, 0.030, 0.007 },
    { 0.003, 0.015, 0.040, 0.080, 0.110, 0.080, 0.040, 0.015, 0.003 },
    { 0.001, 0.005, 0.015, 0.030, 0.040, 0.030, 0.015, 0.005, 0.001 },
    { 0.0002, 0.001, 0.003, 0.007, 0.011, 0.007, 0.003, 0.001, 0.0002 }
};

struct SmoothingSetting
{
    int kernelType; // カーネルサイズ（3, 5, 7, 9）
};

ConstantBuffer<SmoothingSetting> gSmoothingSetting : register(b0);

PixelShaderOutput main(VertexShaderOutput input)
{
    uint width, height;
    gTexture.GetDimensions(width, height);
    float2 uvStepSize = rcp(float2(width, height));
    
    PixelShaderOutput output;
    output.color.rgb = float3(0.0f, 0.0f, 0.0f);
    
    if (gSmoothingSetting.kernelType == 0)
    {
        // None（フィルターなし）
        output.color = gTexture.Sample(gSampler, input.texcoord);
        return output;
    }
    else if (gSmoothingSetting.kernelType == 1)
    {
        // Box3x3
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                float2 offsetUV = input.texcoord + kIndex3x3[i][j] * uvStepSize;
                output.color.rgb += gTexture.Sample(gSampler, offsetUV).rgb * kKernel3x3[i][j];
            }
        }
    }
    else if (gSmoothingSetting.kernelType == 2)
    {
        // Box5x5
        for (int i = 0; i < 5; ++i)
        {
            for (int j = 0; j < 5; ++j)
            {
                float2 offsetUV = input.texcoord + kIndex5x5[i][j] * uvStepSize;
                output.color.rgb += gTexture.Sample(gSampler, offsetUV).rgb * kKernel5x5[i][j];
            }
        }
    }
    else if (gSmoothingSetting.kernelType == 3)
    {
        // Gaussian5x5
        for (int i = 0; i < 5; ++i)
        {
            for (int j = 0; j < 5; ++j)
            {
                float2 offsetUV = input.texcoord + kIndex5x5[i][j] * uvStepSize;
                output.color.rgb += gTexture.Sample(gSampler, offsetUV).rgb * kGaussianKernel5x5[i][j];
            }
        }
    }
    else if (gSmoothingSetting.kernelType == 4)
    {
        // Box7x7
        for (int i = 0; i < 7; ++i)
        {
            for (int j = 0; j < 7; ++j)
            {
                float2 offsetUV = input.texcoord + kIndex7x7[i][j] * uvStepSize;
                output.color.rgb += gTexture.Sample(gSampler, offsetUV).rgb * kKernel7x7[i][j];
            }
        }
    }
    else if (gSmoothingSetting.kernelType == 5)
    {
        // Gaussian7x7
        for (int i = 0; i < 7; ++i)
        {
            for (int j = 0; j < 7; ++j)
            {
                float2 offsetUV = input.texcoord + kIndex7x7[i][j] * uvStepSize;
                output.color.rgb += gTexture.Sample(gSampler, offsetUV).rgb * kGaussianKernel7x7[i][j];
            }
        }
    }
    else if (gSmoothingSetting.kernelType == 6)
    {
        // Box9x9
        for (int i = 0; i < 9; ++i)
        {
            for (int j = 0; j < 9; ++j)
            {
                float2 offsetUV = input.texcoord + kIndex9x9[i][j] * uvStepSize;
                output.color.rgb += gTexture.Sample(gSampler, offsetUV).rgb * kKernel9x9[i][j];
            }
        }
    }
    else if (gSmoothingSetting.kernelType == 7)
    {
        // Gaussian9x9
        for (int i = 0; i < 9; ++i)
        {
            for (int j = 0; j < 9; ++j)
            {
                float2 offsetUV = input.texcoord + kIndex9x9[i][j] * uvStepSize;
                output.color.rgb += gTexture.Sample(gSampler, offsetUV).rgb * kGaussianKernel9x9[i][j];
            }
        }
    }
    
    output.color.a = gTexture.Sample(gSampler, input.texcoord).a;
    return output;
    
    // 元の画像のアルファ値をそのまま使う
    output.color.a = gTexture.Sample(gSampler, input.texcoord).a;
    
    return output;
}