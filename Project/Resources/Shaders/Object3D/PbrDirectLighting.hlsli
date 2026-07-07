#ifndef PBR_DIRECT_LIGHTING_HLSLI
#define PBR_DIRECT_LIGHTING_HLSLI

// PBR Direct Lightingの準備用関数群。
// IBLは環境マップ/BRDF LUT/roughness mip設計が別途必要なため、ここではDirect Lightのみを扱う。
// Legacy MaterialとPBR Materialは既存モデル互換を守るため共存させ、MaterialCBDataへはまだ接続しない。

static const float kPbrPI = 3.14159265f;

struct PbrSurface
{
    float3 baseColor;
    float metallic;
    float roughness;
    float3 normal;
    float3 viewDir;
};

float DistributionGGX(float3 normal, float3 halfVector, float roughness)
{
    float a = max(roughness * roughness, 0.001f);
    float a2 = a * a;
    float NdotH = saturate(dot(normal, halfVector));
    float denom = (NdotH * NdotH) * (a2 - 1.0f) + 1.0f;
    return a2 / max(kPbrPI * denom * denom, 1e-5f);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return NdotV / max(NdotV * (1.0f - k) + k, 1e-5f);
}

float GeometrySmith(float3 normal, float3 viewDir, float3 lightDir, float roughness)
{
    float NdotV = saturate(dot(normal, viewDir));
    float NdotL = saturate(dot(normal, lightDir));
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

float3 FresnelSchlickPbr(float cosTheta, float3 f0)
{
    return f0 + (1.0.xxx - f0) * pow(1.0f - saturate(cosTheta), 5.0f);
}

float3 EvaluatePbrDirectLight(PbrSurface surface, float3 lightDir, float3 lightColor)
{
    float3 normal = normalize(surface.normal);
    float3 viewDir = normalize(surface.viewDir);
    float3 halfVector = normalize(viewDir + lightDir);
    float roughness = clamp(surface.roughness, 0.04f, 1.0f);
    float metallic = saturate(surface.metallic);
    float NdotL = saturate(dot(normal, lightDir));
    float NdotV = saturate(dot(normal, viewDir));

    float3 f0 = lerp(0.04.xxx, surface.baseColor, metallic);
    float NDF = DistributionGGX(normal, halfVector, roughness);
    float G = GeometrySmith(normal, viewDir, lightDir, roughness);
    float3 F = FresnelSchlickPbr(saturate(dot(halfVector, viewDir)), f0);

    float3 specular = (NDF * G * F) / max(4.0f * NdotV * NdotL, 1e-5f);
    float3 kD = (1.0.xxx - F) * (1.0f - metallic);
    float3 diffuse = kD * surface.baseColor / kPbrPI;

    return (diffuse + specular) * lightColor * NdotL;
}

#endif
