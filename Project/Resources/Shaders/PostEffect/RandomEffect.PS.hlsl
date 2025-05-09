#include "FullScreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

struct RandomSetting
{
    float time;
    int useMultiply;
    float2 padding; // 16バイトにアライメント
};
ConstantBuffer<RandomSetting> gRandomSetting : register(b0);

// 0〜1の乱数を返すヘルパー関数（texcoordベース）
float rand2dTo1d(float2 uv)
{
    return frac(sin(dot(uv.xy, float2(12.9898, 78.233))) * 43758.5453);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float2 uv = input.texcoord;

    // 時間付き乱数生成
    float random = rand2dTo1d(uv * gRandomSetting.time);

    // モード切り替え：乗算するか、ノイズそのまま出すか
    if (gRandomSetting.useMultiply != 0)
    {
        float4 texColor = gTexture.Sample(gSampler, uv);
        texColor.rgb *= random;
        output.color = texColor;
    }
    else
    {
        output.color = float4(random, random, random, 1.0f); // 砂嵐のみ
    }

    return output;
}