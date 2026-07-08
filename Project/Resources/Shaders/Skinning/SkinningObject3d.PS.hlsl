#include "SkinningObject3d.hlsli"
#include "../Object3D/LightingCommon.hlsli"
#include "../Object3D/PbrDirectLighting.hlsli"

//ピクセルシェーダーの出力
struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// マテリアル
struct Material
{
    float4 color; // オブジェクトの色
    float shininess; // 光沢度
    float pbrEnabled; // 1.0でPBR Direct Lightingを使用。旧padding領域でCB互換を維持する。
    float metallic; // Metallic/Roughness Texture未接続時の定数fallback
    float normalScale; // NormalMap未接続時も設定値だけ保持する
    float4x4 uvTransform; // UVTransform
    float reflectionRate; // 反射率
    float roughness; // 粗さ
    float usePointSampling; // Object3D と同じ定数バッファレイアウトを保つ
    float occlusionStrength; // AO Texture未接続時の定数fallback
};

// カメラ
struct Camera
{
    float3 worldPosition; // カメラの位置
    float padding0;
};

struct LightInfo
{
    uint gLightCount;
    float padding0;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<Camera> gCamera : register(b1);
ConstantBuffer<LightInfo> gLightInfo : register(b2);
ConstantBuffer<ShadowParameter> gShadowParameter : register(b4);
ConstantBuffer<LightingSettings> gLightingSettings : register(b5);

Texture2D<float4> gTexture : register(t0); // テクスチャ
TextureCube<float4> gEnvironmentTexture : register(t1); // 環境マップ
StructuredBuffer<PunctualLight> gPunctualLights : register(t2); // パンクチュアルライト
Texture2D<float> gShadowMap : register(t4); // シャドウマップ

SamplerState gSampler : register(s0);
SamplerComparisonState gShadowSampler : register(s1);

static const float kAlphaDiscardThreshold = 0.001f;

float ComputeFresnelSchlick(float cosTheta, float f0)
{
    return f0 + (1.0f - f0) * pow(1.0f - cosTheta, 5.0f);
}

// ピクセルシェーダー (PS) のメイン関数 (メインエントリーポイント)
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // SRGB SRV は Sample 時点で線形化されるため、Object3D と同じく手動 pow は行わない。
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    float3 worldPosition = input.worldPosition;
    float3 normal = normalize(input.normal);
    float3 viewDir = normalize(gCamera.worldPosition - worldPosition);

    float spotShadowFactor = 1.0f;
    if (gShadowParameter.shadowMode == 2)
    {
        float3 dominantSpotDir = float3(0.0f, -1.0f, 0.0f);
        [loop]
        for (uint i = 0; i < gLightInfo.gLightCount; ++i)
        {
            if (gPunctualLights[i].lightType == LIGHT_TYPE_SPOT)
            {
                dominantSpotDir = normalize(gPunctualLights[i].position - worldPosition);
                break;
            }
        }
        spotShadowFactor = CalculateShadow(worldPosition, normal, dominantSpotDir, gShadowParameter, gShadowMap, gShadowSampler);
    }

    if (gShadowParameter.shadowDebugMode == 1)
    {
        float4 shadowPosition = mul(float4(worldPosition, 1.0f), gShadowParameter.lightViewProjection);
        float3 proj = shadowPosition.xyz / max(shadowPosition.w, 1e-5f);
        float2 uv = float2(proj.x * 0.5f + 0.5f, -proj.y * 0.5f + 0.5f);
        float depth = gShadowMap.SampleLevel(gSampler, saturate(uv), 0.0f);
        output.color = float4(depth.xxx, 1.0f);
        return output;
    }
    if (gShadowParameter.shadowDebugMode == 2)
    {
        output.color = float4(spotShadowFactor.xxx, 1.0f);
        return output;
    }

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

    float3 reflectionDir = reflect(-viewDir, normal);
    float3 environmentColor = gEnvironmentTexture.Sample(gSampler, reflectionDir).rgb;
    float fresnel = ComputeFresnelSchlick(saturate(dot(normal, viewDir)), 0.02f);
    float envBlend = saturate(gMaterial.reflectionRate * 0.12f + fresnel * 0.03f);

    float3 shadedColor = 0.0.xxx;
    if (gMaterial.pbrEnabled > 0.5f)
    {
        // PBRはMaterial単位でONの時だけ使い、既存アニメーションモデルのLegacy表示を初期状態で残す。
        float2 metallicRoughness = ResolveMetallicRoughnessFallback(gMaterial.metallic, gMaterial.roughness);
        PbrSurface surface;
        surface.baseColor = baseColor;
        surface.metallic = metallicRoughness.x;
        surface.roughness = metallicRoughness.y;
        surface.occlusion = saturate(gMaterial.occlusionStrength);
        surface.emissive = 0.0.xxx;
        surface.normal = ResolvePbrNormalFallback(normal, gMaterial.normalScale);
        surface.viewDir = viewDir;

        float3 directPbr = DirectLightingPBR(gPunctualLights, gLightInfo.gLightCount, worldPosition, surface, gShadowParameter, gShadowMap, gShadowSampler);
        float3 ibl = EvaluatePbrIBLFallback(surface, gEnvironmentTexture, gSampler, gLightingSettings);
        shadedColor = directPbr + ibl + surface.emissive;
    }
    else
    {
        // Legacy経路は既存Phong/Blinn系の見た目とLight/Shadow挙動を守るため残す。
        shadedColor = baseColor * lighting;
        shadedColor = lerp(shadedColor, environmentColor, envBlend);
    }

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
