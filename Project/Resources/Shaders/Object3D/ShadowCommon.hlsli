#ifndef SHADOW_COMMON_HLSLI
#define SHADOW_COMMON_HLSLI

static const float kEpsilon = 1e-5f;
static const float kDefaultShadowStrength = 0.60f; // 真っ黒にしない
static const int kPCFRadius = 1; // 3x3 PCF

struct ShadowParameter
{
    float4x4 lightViewProjection;
    float shadowBias;
    float normalBias;
    float shadowStrength;
    uint shadowMode; // 0:Off 1:Directional 2:Spot
    float2 padding;
};

float CalculateShadowPCF(
    float3 worldPosition,
    float3 normal,
    float3 lightDir,
    ShadowParameter shadowParam,
    Texture2D<float> shadowMap,
    SamplerComparisonState shadowSampler)
{
    float NoL = saturate(dot(normal, lightDir));

    // 法線バイアス
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

    float compareDepth = proj.z - shadowParam.shadowBias;

    uint shadowWidth, shadowHeight;
    shadowMap.GetDimensions(shadowWidth, shadowHeight);

    float2 texelSize = 1.0f / float2((float) shadowWidth, (float) shadowHeight);

    float visibility = 0.0f;
    float sampleCount = 0.0f;

    [unroll]
    for (int y = -kPCFRadius; y <= kPCFRadius; ++y)
    {
        [unroll]
        for (int x = -kPCFRadius; x <= kPCFRadius; ++x)
        {
            float2 offset = float2((float) x, (float) y) * texelSize;
            visibility += shadowMap.SampleCmpLevelZero(shadowSampler, uv + offset, compareDepth);
            sampleCount += 1.0f;
        }
    }

    visibility /= sampleCount;

    // ShadowStrengthでDirectLightにのみ掛かる可視度を調整する。
    return lerp(1.0f - saturate(shadowParam.shadowStrength), 1.0f, visibility);
}

// 既存名を残したいならこれでラップ
float CalculateShadow(
    float3 worldPosition,
    float3 normal,
    float3 lightDir,
    ShadowParameter shadowParam,
    Texture2D<float> shadowMap,
    SamplerComparisonState shadowSampler)
{
    return CalculateShadowPCF(
        worldPosition,
        normal,
        lightDir,
        shadowParam,
        shadowMap,
        shadowSampler);
}

#endif