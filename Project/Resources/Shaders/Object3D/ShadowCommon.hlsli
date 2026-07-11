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
    uint shadowMode; // 0:Off 1:Directional 2:Spot 3:PointCube 4:CSM
    uint shadowDebugMode; // 0:None 1:ShadowMap 2:ShadowFactor
    float padding;
};

// 既存ShadowParameter(b4)のレイアウトは変更せず、Phase 5の拡張値だけをb6へ分離する。
struct ExtendedShadowParameter
{
    float4x4 cascadeLightViewProjection[4];
    float4 cascadeSplits;
    float4 pointLightPositionAndFar;
    float4 cameraPositionAndPointNear;
    uint shadowTechnique; // 0:Legacy/Off 3:PointCube 4:CSM
    uint cascadeCount;
    uint shadowCasterLightIndex;
    uint padding0;
    float shadowBias;
    float normalBias;
    float shadowStrength;
    float padding1;
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

uint SelectShadowCascade(float cameraDistance, ExtendedShadowParameter shadowParam)
{
    uint cascadeIndex = 0;
    cascadeIndex += cameraDistance > shadowParam.cascadeSplits.x;
    cascadeIndex += cameraDistance > shadowParam.cascadeSplits.y;
    cascadeIndex += cameraDistance > shadowParam.cascadeSplits.z;
    return min(cascadeIndex, max(shadowParam.cascadeCount, 1u) - 1u);
}

float CalculateCsmShadow(
    float3 worldPosition,
    float3 normal,
    float3 lightDir,
    ExtendedShadowParameter shadowParam,
    Texture2DArray<float> csmShadowMaps,
    SamplerComparisonState shadowSampler)
{
    if (shadowParam.shadowTechnique != 4 || shadowParam.cascadeCount == 0)
    {
        return 1.0f;
    }

    const float cameraDistance = length(worldPosition - shadowParam.cameraPositionAndPointNear.xyz);
    const uint cascadeIndex = SelectShadowCascade(cameraDistance, shadowParam);
    const float NoL = saturate(dot(normal, lightDir));
    const float3 biasedPosition = worldPosition + normal * shadowParam.normalBias * (1.0f - NoL);
    const float4 shadowPosition = mul(float4(biasedPosition, 1.0f), shadowParam.cascadeLightViewProjection[cascadeIndex]);
    if (abs(shadowPosition.w) < kEpsilon) { return 1.0f; }
    const float3 projected = shadowPosition.xyz / shadowPosition.w;
    const float2 uv = float2(projected.x * 0.5f + 0.5f, -projected.y * 0.5f + 0.5f);
    if (any(uv < 0.0f) || any(uv > 1.0f) || projected.z < 0.0f || projected.z > 1.0f) { return 1.0f; }

    uint width, height, layers;
    csmShadowMaps.GetDimensions(width, height, layers);
    const float2 texelSize = 1.0f / max(float2(width, height), float2(1.0f, 1.0f));
    float visibility = 0.0f;
    [unroll]
    for (int y = -kPCFRadius; y <= kPCFRadius; ++y)
    {
        [unroll]
        for (int x = -kPCFRadius; x <= kPCFRadius; ++x)
        {
            visibility += csmShadowMaps.SampleCmpLevelZero(
                shadowSampler,
                float3(uv + float2(x, y) * texelSize, (float)cascadeIndex),
                projected.z - shadowParam.shadowBias);
        }
    }
    visibility /= 9.0f;
    return lerp(1.0f - saturate(shadowParam.shadowStrength), 1.0f, visibility);
}

float CalculatePointCubeShadow(
    float3 worldPosition,
    float3 normal,
    float3 lightDir,
    ExtendedShadowParameter shadowParam,
    TextureCube<float> pointShadowMap,
    SamplerComparisonState shadowSampler)
{
    if (shadowParam.shadowTechnique != 3) { return 1.0f; }

    const float3 lightPosition = shadowParam.pointLightPositionAndFar.xyz;
    const float farZ = max(shadowParam.pointLightPositionAndFar.w, 0.02f);
    const float nearZ = clamp(shadowParam.cameraPositionAndPointNear.w, 0.01f, farZ * 0.5f);
    const float NoL = saturate(dot(normal, lightDir));
    const float3 biasedPosition = worldPosition + normal * shadowParam.normalBias * (1.0f - NoL);
    const float3 fromLight = biasedPosition - lightPosition;
    const float distanceToLight = length(fromLight);
    if (distanceToLight <= nearZ || distanceToLight >= farZ) { return 1.0f; }

    const float compareDepth = saturate(distanceToLight / farZ) - shadowParam.shadowBias;
    const float3 sampleDirection = normalize(fromLight);

    uint shadowWidth, shadowHeight, mipLevels;
    pointShadowMap.GetDimensions(0, shadowWidth, shadowHeight, mipLevels);
    const float texelAngle = 2.0f / max((float)shadowWidth, 1.0f);
    const float3 referenceUp = abs(sampleDirection.y) > 0.98f
        ? float3(1.0f, 0.0f, 0.0f)
        : float3(0.0f, 1.0f, 0.0f);
    const float3 tangent = normalize(cross(referenceUp, sampleDirection));
    const float3 bitangent = normalize(cross(sampleDirection, tangent));

    float visibility = pointShadowMap.SampleCmpLevelZero(shadowSampler, sampleDirection, compareDepth);
    visibility += pointShadowMap.SampleCmpLevelZero(shadowSampler, normalize(sampleDirection + tangent * texelAngle), compareDepth);
    visibility += pointShadowMap.SampleCmpLevelZero(shadowSampler, normalize(sampleDirection - tangent * texelAngle), compareDepth);
    visibility += pointShadowMap.SampleCmpLevelZero(shadowSampler, normalize(sampleDirection + bitangent * texelAngle), compareDepth);
    visibility += pointShadowMap.SampleCmpLevelZero(shadowSampler, normalize(sampleDirection - bitangent * texelAngle), compareDepth);
    visibility /= 5.0f; // Cube Face境界を跨いでも同じ距離基準の5tap PCFとして評価する。

    return lerp(1.0f - saturate(shadowParam.shadowStrength), 1.0f, visibility);
}

#endif
