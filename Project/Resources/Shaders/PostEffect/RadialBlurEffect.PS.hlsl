#include "FullScreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct RadialBlurSetting
{
    float2 center;
    float blurStrength;
    float sampleCount;
};

ConstantBuffer<RadialBlurSetting> gRadialBlur : register(b0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float2 uv = input.texcoord;
    float2 dir = uv - gRadialBlur.center;

    float4 sum = float4(0, 0, 0, 0);

    // 放射状に中心に向かって複数サンプルを取る
    for (int i = 0; i < (int) gRadialBlur.sampleCount; ++i)
    {
        float scale = 1.0f - (i / gRadialBlur.sampleCount) * gRadialBlur.blurStrength;
        float2 sampleUV = gRadialBlur.center + dir * scale;
        sum += gTexture.Sample(gSampler, sampleUV);
    }

    output.color = sum / gRadialBlur.sampleCount;
    return output;
}
