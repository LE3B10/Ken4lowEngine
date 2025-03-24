#include "FullScreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    output.color = gTexture.Sample(gSampler, input.texcoord);
    
    // 周囲を0に、中心になるほど明るくなるような計算
    float2 correct = input.texcoord * (1.0f - input.texcoord.xy);
   
    // correctだけで計算すると中心の最大値が0.0625で暗すぎるので、それを補正
    float vignette = correct.x * correct.y * 16.0f;
    
    // それっぽくするために、RGBの平均値を使う
    vignette = saturate(pow(vignette, 0.8f));
    
    // 色に掛ける
    output.color.rgb *= vignette;
    
    return output;
}