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
    float3 padding0;
    float4x4 uvTransform;
    float reflectionRate;
    float roughness;
    float usePointSampling;
    float padding1;
};

struct Camera
{
    float3 worldPosition;
    float padding0;
};

struct LightInfo
{
    uint gLightCount;
    float padding0;
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
ConstantBuffer<LightingSettings> gLightingSettings : register(b5);

Texture2D<float4> gTexture : register(t0);
TextureCube<float4> gEnvironmentTexture : register(t1);
StructuredBuffer<PunctualLight> gPunctualLights : register(t2);
Texture2D<float4> gDissolveMaskTexture : register(t3);
Texture2D<float> gShadowMap : register(t4);

SamplerState gSampler : register(s0);
SamplerComparisonState gShadowSampler : register(s1);
SamplerState gPointSampler : register(s2);

static const float kAlphaDiscardThreshold = 0.001f;

float ComputeFresnelSchlick(float cosTheta, float f0)
{
    return f0 + (1.0f - f0) * pow(1.0f - cosTheta, 5.0f);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    // SRGB SRV は Sample 時点で線形化されるため、手動の pow による二重変換を避ける。
    // 低解像度テクスチャでは Point、通常モデルは Linear を使い分ける。
    float4 textureColor = (gMaterial.usePointSampling > 0.5f)
        ? gTexture.Sample(gPointSampler, transformedUV.xy)
        : gTexture.Sample(gSampler, transformedUV.xy);

    float3 worldPosition = input.worldPosition;
    float3 normal = normalize(input.normal);
    float3 viewDir = normalize(gCamera.worldPosition - worldPosition);

    float3 lighting = AccumulateLighting(gPunctualLights,
        gLightInfo.gLightCount,
        worldPosition,
        normal,
        viewDir,
        gShadowParameter,
        gMaterial.shininess,
        gShadowMap,
        gShadowSampler,
        gLightingSettings);
        
    float3 baseColor = gMaterial.color.rgb * textureColor.rgb;

    // 環境反射
    float3 reflectionDir = reflect(-viewDir, normal);
    float3 environmentColor = gEnvironmentTexture.Sample(gSampler, reflectionDir).rgb;

    // フレネルを少しだけ足す
    float fresnel = ComputeFresnelSchlick(saturate(dot(normal, viewDir)), 0.02f);
    float envBlend = saturate(gMaterial.reflectionRate * 0.12f + fresnel * 0.03f);
    float3 reflectionColor = environmentColor;

    // Dissolve
    float maskValue = gDissolveMaskTexture.Sample(gSampler, input.texcoord).r;
    float edge = smoothstep(
        gDissolveSetting.threshold,
        gDissolveSetting.threshold + gDissolveSetting.edgeThickness,
        maskValue);

    float4 edgeColor = gDissolveSetting.edgeColor * (1.0f - edge);
    float dissolveBlend = 1.0f - step(maskValue, gDissolveSetting.threshold);

    float3 shadedColor = lerp(baseColor, edgeColor.rgb, dissolveBlend);
    shadedColor *= lighting;

    // 反射を最後に混ぜる
    shadedColor = lerp(shadedColor, reflectionColor, envBlend);

    // Fog/ToneMap/Contrastを最後に適用して白飛びを抑える。
    shadedColor = ApplyFog(shadedColor, worldPosition, gCamera.worldPosition, gLightingSettings);
    shadedColor = ApplySimpleToneMapping(shadedColor, gLightingSettings);
    shadedColor = ApplyContrast(shadedColor, gLightingSettings);

    output.color.rgb = shadedColor;
    output.color.a = gMaterial.color.a * textureColor.a;

    if (output.color.a < kAlphaDiscardThreshold)
    {
        discard;
    }

    return output;
}
