#ifndef LIGHTING_COMMON_HLSLI
#define LIGHTING_COMMON_HLSLI

#include "ShadowCommon.hlsli"

static const float kMinLightLength = 1e-4f;
static const float kMinRange = 1e-3f;

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

struct LightingSettings
{
    float4 ambientColor;
    float4 fogColor;
    float exposure;
    float contrast;
    float fogStart;
    float fogEnd;
    uint enableFog;
    float specularStrength;
    uint debugEnableAmbient;
    uint debugEnableDirectional;
    uint debugEnablePoint;
    uint debugEnableSpot;
    uint debugEnableEmissive;
    uint debugShowShadowFactor;
    uint debugShowLightContribution;
    float padding;
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
    // 通常Lambertで面の向きによる明暗差を出す。
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
    float rangeMask = step(d, range);
    float atten = pow(saturate(1.0f - d / range), max(light.decay, kMinRange)) * rangeMask;

    // ライト範囲外はDirect Lightを0にしてAmbientだけ残す。
    float NdotL = saturate(dot(normal, lightDir));
    float3 lightColor = light.color.rgb * (light.intensity * atten);

    terms.diffuse = lightColor * NdotL;
    terms.specular = lightColor * ComputeSpecular(normal, lightDir, viewDir, shininess) * NdotL;
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
    float rangeMask = step(d, range);
    float atten = pow(saturate(1.0f - d / range), max(light.decay, kMinRange)) * rangeMask;

    float3 dir = normalize(light.direction);
    float ct = dot(-dir, lightDir);
    float spot = smoothstep(light.cosAngle, light.cosFalloffStart, ct) * step(light.cosAngle, ct);

    // スポット光もHalf Lambertではなく通常Lambertを基本にする。
    float NdotL = saturate(dot(normal, lightDir));
    float3 lightColor = light.color.rgb * (light.intensity * atten * spot);

    terms.diffuse = lightColor * NdotL;
    terms.specular = lightColor * ComputeSpecular(normal, lightDir, viewDir, shininess) * NdotL;
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
    SamplerComparisonState shadowSampler,
    LightingSettings lightingSettings)
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
            if (lightingSettings.debugEnableDirectional == 0) { continue; }
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
            if (lightingSettings.debugEnablePoint == 0) { continue; }
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
            if (lightingSettings.debugEnableSpot == 0) { continue; }
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

    // CPUから渡したAmbientをそのまま使い、強すぎる環境光を調整できるようにする。
    float3 ambient = (lightingSettings.debugEnableAmbient != 0) ? (lightingSettings.ambientColor.rgb * lightingSettings.ambientColor.a) : 0.0.xxx;

    // 旧挙動の最低明度(1.0)を撤廃し、ライト0本なら真っ暗(=0)になるようにする。
    return (ambient + diffuseSum + specularSum * lightingSettings.specularStrength);
}

float3 ApplySimpleToneMapping(float3 color, LightingSettings lightingSettings)
{
    // 露出後に1.0を超えた成分だけReinhard風に圧縮して白飛びを抑える。
    float3 exposed = max(color * lightingSettings.exposure, 0.0.xxx);
    float3 overbright = max(exposed - 1.0.xxx, 0.0.xxx);
    return exposed / (1.0.xxx + overbright);
}

float3 ApplyContrast(float3 color, LightingSettings lightingSettings)
{
    // 中間輝度0.5を基準にコントラストを調整する。
    return saturate((color - 0.5.xxx) * lightingSettings.contrast + 0.5.xxx);
}

float3 ApplyFog(float3 color, float3 worldPosition, float3 cameraPosition, LightingSettings lightingSettings)
{
    if (lightingSettings.enableFog == 0)
    {
        return color;
    }

    // 距離フォグは任意機能として遠景の白飛び/奥行き調整に使う。
    float distanceToCamera = distance(worldPosition, cameraPosition);
    float fogRange = max(lightingSettings.fogEnd - lightingSettings.fogStart, 1e-3f);
    float fogFactor = saturate((distanceToCamera - lightingSettings.fogStart) / fogRange);
    return lerp(color, lightingSettings.fogColor.rgb, fogFactor * lightingSettings.fogColor.a);
}

#endif
