#include "FullScreen.hlsli"

// 円周率
static const float PI = 3.1415926535897932f;

// 構造体
struct GaussianFilterSetting
{
    int kernelType; // 3,5,7,9 のいずれか（ピクセルシェーダーで分岐）
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
    
    float kernel3x3[3][3]; // 3x3カーネル
    float kernel5x5[5][5]; // 5x5カーネル
    float kernel7x7[7][7]; // 7x7カーネル
    float kernel9x9[9][9]; // 9x9カーネル
    float weight = 0.0f;
    
    // カーネルタイプが0の場合は、フィルタを適用しない
    if (gGaussianFilterSetting.kernelType == 0)
    {
        output.color = gTexture.Sample(gSampler, input.texcoord);
        return output;
    }
    
    // ガウシアンフィルタ適用
    else if (gGaussianFilterSetting.kernelType == 1) // 3x3
    {
        // **カーネルを生成**
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                float2 offset = kIndex3x3[i][j];
                kernel3x3[i][j] = Gaussian(offset.x, offset.y, gGaussianFilterSetting.sigma);
                weight += kernel3x3[i][j]; // 正規化用
            }
        }
        
        for (int k = 0; k < 3; ++k)
        {
            for (int l = 0; l < 3; ++l)
            {
                float2 offsetUV = input.texcoord + kIndex3x3[k][l] * uvStepSize;
                float3 srcColor = gTexture.Sample(gSampler, offsetUV).rgb;

                // 輝度（luminance）を計算
                float luminance = dot(srcColor, float3(0.299f, 0.587f, 0.114f));

                if (luminance > gGaussianFilterSetting.threshold)
                {
                    output.color.rgb += srcColor * (kernel3x3[k][l] / weight);
                }
            }
        }
    }
    else if (gGaussianFilterSetting.kernelType == 2) // 5x5
    {
        // **カーネルを生成**
        for (int i = 0; i < 5; ++i)
        {
            for (int j = 0; j < 5; ++j)
            {
                float2 offset = kIndex5x5[i][j];
                kernel5x5[i][j] = Gaussian(offset.x, offset.y, gGaussianFilterSetting.sigma);
                weight += kernel5x5[i][j]; // 正規化用
            }
        }
        
        for (int k = 0; k < 5; ++k)
        {
            for (int l = 0; l < 5; ++l)
            {
                float2 offsetUV = input.texcoord + kIndex5x5[k][l] * uvStepSize;
                float3 srcColor = gTexture.Sample(gSampler, offsetUV).rgb;

                // 輝度（luminance）を計算
                float luminance = dot(srcColor, float3(0.299f, 0.587f, 0.114f));

                if (luminance > gGaussianFilterSetting.threshold)
                {
                    output.color.rgb += srcColor * (kernel5x5[k][l] / weight);
                }
            }
        }
    }
    else if (gGaussianFilterSetting.kernelType == 3) // 7x7
    {
        // **カーネルを生成**
        for (int i = 0; i < 7; ++i)
        {
            for (int j = 0; j < 7; ++j)
            {
                float2 offset = kIndex7x7[i][j];
                kernel7x7[i][j] = Gaussian(offset.x, offset.y, gGaussianFilterSetting.sigma);
                weight += kernel7x7[i][j]; // 正規化用
            }
        }
        
        for (int k = 0; k < 7; ++k)
        {
            for (int l = 0; l < 7; ++l)
            {
                float2 offsetUV = input.texcoord + kIndex7x7[k][l] * uvStepSize;
                float3 srcColor = gTexture.Sample(gSampler, offsetUV).rgb;

                // 輝度（luminance）を計算
                float luminance = dot(srcColor, float3(0.299f, 0.587f, 0.114f));

                if (luminance > gGaussianFilterSetting.threshold)
                {
                    output.color.rgb += srcColor * (kernel7x7[k][l] / weight);
                }
            }
        }
    }
    else if (gGaussianFilterSetting.kernelType == 4) // 9x9
    {
        // **カーネルを生成**
        for (int i = 0; i < 9; ++i)
        {
            for (int j = 0; j < 9; ++j)
            {
                float2 offset = kIndex9x9[i][j];
                kernel9x9[i][j] = Gaussian(offset.x, offset.y, gGaussianFilterSetting.sigma);
                weight += kernel9x9[i][j]; // 正規化用
            }
        }
        
        for (int k = 0; k < 9; ++k)
        {
            for (int l = 0; l < 9; ++l)
            {
                float2 offsetUV = input.texcoord + kIndex9x9[k][l] * uvStepSize;
                float3 srcColor = gTexture.Sample(gSampler, offsetUV).rgb;

                // 輝度（luminance）を計算
                float luminance = dot(srcColor, float3(0.299f, 0.587f, 0.114f));

                if (luminance > gGaussianFilterSetting.threshold)
                {
                    output.color.rgb += srcColor * (kernel9x9[k][l] / weight);
                }
            }
        }
    }

    output.color.rgb *= gGaussianFilterSetting.intensity;

    // アルファチャンネルは元画像から
    output.color.a = gTexture.Sample(gSampler, input.texcoord).a;

    return output;
}