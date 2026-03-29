#include "Object3d.hlsli"
#include "LightingCommon.hlsli"

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

struct Material
{
    float4 color;
    float shininess;
    float4x4 uvTransform;
    float reflectionRate;
};

struct Camera
{
    float3 worldPosition;
};

struct LightInfo
{
    uint gLightCount;
};

struct DissolveSetting
{
    float threshold;
    float edgeThickness;
    float4 edgeColor;
    float3 padding;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<Camera> gCamera : register(b1);
ConstantBuffer<LightInfo> gLightInfo : register(b2);
ConstantBuffer<DissolveSetting> gDissolveSetting : register(b3);
ConstantBuffer<ShadowParameter> gShadowParameter : register(b4);

Texture2D<float4> gTexture : register(t0);
TextureCube<float4> gEnvironmentTexture : register(t1);
StructuredBuffer<PunctualLight> gPunctualLights : register(t2);
Texture2D<float4> gDissolveMaskTexture : register(t3);
Texture2D<float> gShadowMap : register(t4);

SamplerState gSampler : register(s0);
SamplerState gShadowSampler : register(s1);

static const float kAlphaDiscardThreshold = 0.001f;

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    textureColor.rgb = pow(textureColor.rgb, 2.2f);

    float3 worldPosition = input.worldPosition;
    float3 normal = normalize(input.normal);
    float3 viewDir = normalize(gCamera.worldPosition - worldPosition);

    float3 lighting = AccumulateLighting(
        gPunctualLights,
        gLightInfo.gLightCount,
        worldPosition,
        normal,
        viewDir,
        gShadowParameter,
        gMaterial.shininess,
        gShadowMap,
        gShadowSampler);

    float3 reflectionDir = reflect(-viewDir, normal);
    float3 environmentColor = gEnvironmentTexture.Sample(gSampler, reflectionDir).rgb;

    float maskValue = gDissolveMaskTexture.Sample(gSampler, input.texcoord).r;
    float edge = smoothstep(
        gDissolveSetting.threshold,
        gDissolveSetting.threshold + gDissolveSetting.edgeThickness,
        maskValue);

    float4 edgeColor = gDissolveSetting.edgeColor * (1.0f - edge);
    float dissolveBlend = 1.0f - step(maskValue, gDissolveSetting.threshold);

    output.color = gMaterial.color * lerp(textureColor, edgeColor, dissolveBlend);
    output.color.rgb *= lighting;
    output.color.rgb = lerp(output.color.rgb, environmentColor, gMaterial.reflectionRate);

    if (output.color.a < kAlphaDiscardThreshold)
    {
        discard;
    }

    return output;
}