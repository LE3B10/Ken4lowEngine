#include "FullScreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

struct GrayScaleParams
{
    float4 tintColor; // 色の変化を加えるためのRGB係数（例: 白→グレースケールそのまま）
};

ConstantBuffer<GrayScaleParams> gParams : register(b0);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    output.color = gTexture.Sample(gSampler, input.texcoord);
    float value = dot(output.color.rgb, float3(0.2125f, 0.7154f, 0.0721f));
    output.color.rgb = value * gParams.tintColor.rgb;
    output.color.a = gTexture.Sample(gSampler, input.texcoord).a;
    //output.color.rgb = float3(value, value, value);
    return output;
}