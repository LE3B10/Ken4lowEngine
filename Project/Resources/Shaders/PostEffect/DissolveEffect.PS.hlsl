#include "FullScreen.hlsli"

Texture2D<float4> gTexture : register(t0); // 元の画像
Texture2D<float> gMask : register(t1); // マスク画像
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

struct DissolveSetting
{
    float threshold; // 閾値
    float edgeThickness; // エッジの範囲（0.05など）
    float4 edgeColor; // エッジ部分の色
};

ConstantBuffer<DissolveSetting> gDissolveSetting : register(b0);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

   // マスク値取得
    float maskValue = gMask.Sample(gSampler, input.texcoord);

    // 元画像取得
    float4 texColor = gTexture.Sample(gSampler, input.texcoord);

    // エッジの強度計算
    float edge = 1.0f - smoothstep(gDissolveSetting.threshold, gDissolveSetting.threshold + gDissolveSetting.edgeThickness, maskValue);

    // エッジの色を加算
    texColor.rgb += edge * gDissolveSetting.edgeColor.rgb;

    // 完全に消す領域（しきい値以下）を除外
    if (maskValue < gDissolveSetting.threshold)
    {
        discard; // or output.color = float4(0,0,0,0);
    }

    // 出力
    output.color = texColor;
    return output;
}