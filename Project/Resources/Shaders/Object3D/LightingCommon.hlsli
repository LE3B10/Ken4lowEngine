#ifndef LIGHTING_COMMON_HLSLI
#define LIGHTING_COMMON_HLSLI

#include "ShadowCommon.hlsli"

static const float kMinLightLength = 1e-4f;
static const float kMinRange = 1e-3f;
static const float3 kAmbientColor = float3(0.10f, 0.10f, 0.10f);

static const uint LIGHT_TYPE_NONE = 0;
static const uint LIGHT_TYPE_DIRECTIONAL = 1;
static const uint LIGHT_TYPE_POINT = 2;
static const uint LIGHT_TYPE_SPOT = 3;

struct PunctualLight
{
    uint lightType;
    float4 color;
    float intensity;
    float3 position;
    float radius;
    float decay;
    float3 direction;
    float distance;
    float cosFalloffStart;
    float cosAngle;
};

struct LightingTerms
{
    float3 diffuse;
    float3 specular;
};

float3 ComputeSpecular(float3 normal, float3 lightDir, float3 viewDir, float shininess)
{
    float3 halfVector = normalize(lightDir + viewDir);
    float NdotH = saturate(dot(normal, halfVector));

    // ちょっとだけ扱いやすくする
    float specPower = max(shininess, 4.0f);
    float specular = pow(NdotH, specPower);

    return specular.xxx;
}

LightingTerms MakeEmptyLightingTerms()
{
    LightingTerms terms;
    terms.diffuse = 0.0.xxx;
    terms.specular = 0.0.xxx;
    return terms;
}

LightingTerms EvaluateDirectionalLight(
    PunctualLight light,
    float3 worldPosition,
    float3 normal,
    float3 viewDir,
    ShadowParameter shadowParam,
    float shininess,
    Texture2D<float> shadowMap,
    SamplerComparisonState shadowSampler)
{
    LightingTerms terms = MakeEmptyLightingTerms();

    float3 lightDir = normalize(-light.direction);
    float NdotL = saturate(dot(normal, lightDir));
    float3 lightColor = light.color.rgb * light.intensity;

    float shadow = CalculateShadow(
        worldPosition,
        normal,
        lightDir,
        shadowParam,
        shadowMap,
        shadowSampler);

    terms.diffuse = lightColor * NdotL * shadow;
    terms.specular = lightColor * ComputeSpecular(normal, lightDir, viewDir, shininess) * shadow;
    return terms;
}

LightingTerms EvaluatePointLight(
    PunctualLight light,
    float3 worldPosition,
    float3 normal,
    float3 viewDir,
    float shininess)
{
    LightingTerms terms = MakeEmptyLightingTerms();

    float3 toL = light.position - worldPosition;
    float d = length(toL);
    float3 lightDir = toL / max(d, kMinLightLength);

    float range = max(light.radius, kMinRange);
    float atten = pow(saturate(1.0f - d / range), max(light.decay, kMinRange));

    float NdotL = saturate(dot(normal, lightDir));
    float3 lightColor = light.color.rgb * (light.intensity * atten);

    terms.diffuse = lightColor * NdotL;
    terms.specular = lightColor * ComputeSpecular(normal, lightDir, viewDir, shininess);
    return terms;
}

LightingTerms EvaluateSpotLight(
    PunctualLight light,
    float3 worldPosition,
    float3 normal,
    float3 viewDir,
    float shininess)
{
    LightingTerms terms = MakeEmptyLightingTerms();

    float3 toL = light.position - worldPosition;
    float d = length(toL);
    float3 lightDir = toL / max(d, kMinLightLength);

    float range = max(light.distance, kMinRange);
    float atten = pow(saturate(1.0f - d / range), max(light.decay, kMinRange));

    float3 dir = normalize(light.direction);
    float ct = dot(-dir, lightDir);
    float spot = smoothstep(light.cosAngle, light.cosFalloffStart, ct);

    float NdotL = saturate(dot(normal, lightDir));
    float3 lightColor = light.color.rgb * (light.intensity * atten * spot);

    terms.diffuse = lightColor * NdotL;
    terms.specular = lightColor * ComputeSpecular(normal, lightDir, viewDir, shininess);
    return terms;
}

float3 AccumulateLighting(
    StructuredBuffer<PunctualLight> punctualLights,
    uint lightCount,
    float3 worldPosition,
    float3 normal,
    float3 viewDir,
    ShadowParameter shadowParam,
    float shininess,
    Texture2D<float> shadowMap,
    SamplerComparisonState shadowSampler)
{
    float3 diffuseSum = 0.0.xxx;
    float3 specularSum = 0.0.xxx;
    uint activeLightCount = 0;

    [loop]
    for (uint i = 0; i < lightCount; ++i)
    {
        PunctualLight light = punctualLights[i];
        LightingTerms terms = MakeEmptyLightingTerms();

        if (light.lightType == LIGHT_TYPE_DIRECTIONAL)
        {
            terms = EvaluateDirectionalLight(
                light,
                worldPosition,
                normal,
                viewDir,
                shadowParam,
                shininess,
                shadowMap,
                shadowSampler);
            activeLightCount++;
        }
        else if (light.lightType == LIGHT_TYPE_POINT)
        {
            terms = EvaluatePointLight(
                light,
                worldPosition,
                normal,
                viewDir,
                shininess);
            activeLightCount++;
        }
        else if (light.lightType == LIGHT_TYPE_SPOT)
        {
            terms = EvaluateSpotLight(
                light,
                worldPosition,
                normal,
                viewDir,
                shininess);
            activeLightCount++;
        }
        else
        {
            continue;
        }

        diffuseSum += terms.diffuse;
        specularSum += terms.specular;
    }

    return (activeLightCount == 0) ? 1.0.xxx : (kAmbientColor + diffuseSum + specularSum);
}

#endif