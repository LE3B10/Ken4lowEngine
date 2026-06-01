#include "SkyBox.hlsli"

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

struct Material
{
    float4 color;
    float4x4 uvTransform;
    float4 topColor;
    float4 bottomColor;
    float4 horizonColor;
    uint textureIndex;
    uint skyType;
    float2 uvOffset;
    float cloudHeight;
    float cloudScale;
    uint textureAvailable;
    float padding;
};

static const uint textureCount = 1024;
ConstantBuffer<Material> gMaterial : register(b0);
TextureCube<float4> gTexture[textureCount] : register(t0);
Texture2D<float4> gCloudTexture[textureCount] : register(t1024);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float3 direction = normalize(input.texcoord);
    if (gMaterial.skyType == 0)
    {
        output.color = gMaterial.topColor * gMaterial.color;
    }
    else if (gMaterial.skyType == 1)
    {
        // 地平線付近を明るく保ち、写真に頼らないローポリ向けの空色を作る。
        float vertical = direction.y;
        float horizonBlend = saturate(abs(vertical) * 3.0f);
        float4 verticalColor = vertical >= 0.0f ? gMaterial.topColor : gMaterial.bottomColor;
        output.color = lerp(gMaterial.horizonColor, verticalColor, horizonBlend) * gMaterial.color;
    }
    else if (gMaterial.skyType == 2 && gMaterial.textureAvailable == 0)
    {
        output.color = gMaterial.horizonColor * gMaterial.color;
    }
    else
    {
        if (gMaterial.skyType == 3)
        {
            float2 cloudUv;
            cloudUv.x = atan2(direction.z, direction.x) / 6.2831853f + 0.5f;
            cloudUv.y = acos(clamp(direction.y, -1.0f, 1.0f)) / 3.1415927f;
            cloudUv = frac((cloudUv - 0.5f) * max(gMaterial.cloudScale, 0.001f) + 0.5f + gMaterial.uvOffset + float2(0.0f, gMaterial.cloudHeight * 0.0001f));
            output.color = gCloudTexture[gMaterial.textureIndex].Sample(gSampler, cloudUv) * gMaterial.color;
        }
        else
        {
            output.color = gTexture[gMaterial.textureIndex].Sample(gSampler, direction) * gMaterial.color;
        }
    }
    return output;
}
