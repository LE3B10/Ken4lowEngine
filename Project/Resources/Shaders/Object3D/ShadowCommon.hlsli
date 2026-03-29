#ifndef SHADOW_COMMON_HLSLI
#define SHADOW_COMMON_HLSLI

static const float kEpsilon = 1e-5f;
static const float kDefaultShadowStrength = 0.45f;

struct ShadowParameter
{
    float4x4 lightViewProjection;
    float shadowBias;
    float normalBias;
    float2 padding;
};

float CalculateShadow(
    float3 worldPosition,
    float3 normal,
    float3 lightDir,
    ShadowParameter shadowParam,
    Texture2D<float> shadowMap,
    SamplerState shadowSampler)
{
    float NoL = saturate(dot(normal, lightDir));

    float offsetAmount = shadowParam.normalBias * (1.0f - NoL);
    float3 biasedWorldPosition = worldPosition + normal * offsetAmount;

    float4 shadowPosition = mul(float4(biasedWorldPosition, 1.0f), shadowParam.lightViewProjection);

    if (abs(shadowPosition.w) < kEpsilon)
    {
        return 1.0f;
    }

    float3 proj = shadowPosition.xyz / shadowPosition.w;

    float2 uv;
    uv.x = proj.x * 0.5f + 0.5f;
    uv.y = -proj.y * 0.5f + 0.5f;

    if (uv.x < 0.0f || uv.x > 1.0f ||
        uv.y < 0.0f || uv.y > 1.0f ||
        proj.z < 0.0f || proj.z > 1.0f)
    {
        return 1.0f;
    }

    float currentDepth = proj.z - shadowParam.shadowBias;
    float shadowDepth = shadowMap.Sample(shadowSampler, uv).r;

    return (currentDepth <= shadowDepth) ? 1.0f : kDefaultShadowStrength;
}

#endif