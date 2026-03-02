#include "Object3d.hlsli"

//ピクセルシェーダーの出力
struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// マテリアル
struct Material
{
    float4 color; // オブジェクトの色 : bytes 16
    float shininess; // 光沢度 : bytes 4 
    float4x4 uvTransform; // UVTransform : bytes 64
    float reflectionRate; // 反射率 : bytes 4
    // 合計 88 bytes
};

// カメラ
struct Camera
{
    float3 worldPosition; // カメラの位置
};

// パンクチュアルライトの定数バッファ
struct PunctualLight
{
    uint lightType; // ライトの種類（0：ライトなし、1：平行光源、2：点光源、3：スポットライト）
    float4 color; // ライトの色 （全ライト共通）
    float intensity; // 輝度 （全ライト共通）
    float3 position; // ライトの位置 （点光源、スポットライト用）
    float radius; // ライトの届く最大距離 （点光源用）
    float decay; // 減衰率 （点光源、スポットライト用）
    float3 direction; // スポットライトの方向 （平行光源、スポットライト用）
    float distance; // ライトの届く最大距離 （スポットライト用）
    float cosFalloffStart; // 開始角度 （スポットライト用）
    float cosAngle; // スポットライトの余弦 （スポットライト用）
};

// ライト情報
struct LightInfo
{
    uint gLightCount;
};

// Dissolveの設定
struct DissolveSetting
{
    float threshold; // 閾値
    float edgeThickness; // エッジの厚み
    float4 edgeColor; // エッジの色
    float3 padding; // アラインメント
};

struct ShadowParameter
{
    float4x4 lightViewProjection;
    float shadowBias;
    float3 padding;
};

ConstantBuffer<Material> gMaterial : register(b0); // マテリアル情報
ConstantBuffer<Camera> gCamera : register(b1); // カメラ情報
ConstantBuffer<LightInfo> gLightInfo : register(b2); // ライト情報
ConstantBuffer<DissolveSetting> gDissolveSetting : register(b3); // Dissolve設定
ConstantBuffer<ShadowParameter> gShadowParameter : register(b4);

Texture2D<float4> gTexture : register(t0); // テクスチャ
TextureCube<float4> gEnvironmentTexture : register(t1); // 環境マップ
StructuredBuffer<PunctualLight> gPunctualLights : register(t2); // パンクチュアルライト
Texture2D<float4> gDissolveMaskTexture : register(t3); // Dissolveマスクテクスチャ
Texture2D<float> gShadowMap : register(t4); // シャドウマップ

SamplerState gSampler : register(s0);
SamplerState gShadowSampler : register(s1);

float CalculateShadow(float4 shadowPosition, float3 normal, float3 lightDir)
{
    if (abs(shadowPosition.w) < 1e-5f)
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

    float NoL = saturate(dot(normal, lightDir));
    float bias = max(gShadowParameter.shadowBias, (1.0f - NoL) * 0.002f);

    float currentDepth = proj.z - bias;
    float shadowDepth = gShadowMap.Sample(gShadowSampler, uv).r;

    return (currentDepth <= shadowDepth) ? 1.0f : 0.45f;
}

// ピクセルシェーダー (PS) のメイン関数 (メインエントリーポイント)
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // UV設定
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy); // テクスチャの色
    textureColor.rgb = pow(textureColor.rgb, 2.2f); // ガンマ補正済みのテクスチャの場合、リニア空間に変換
    
    // 座標
    float3 position = input.worldPosition; // ワールド座標
    
    // ライトの法線
    float3 normal = normalize(input.normal); // 法線の正規化
    
    // カメラ方向
    float3 viewDir = normalize(gCamera.worldPosition - position); // 視線方向（カメラ方向）
    
    // 環境マップ用
    float3 reflectionDir = reflect(-viewDir, normal); // 反射ベクトル
    float3 environmentColor = gEnvironmentTexture.Sample(gSampler, reflectionDir).rgb; // 環境マップの色
    
    // === 複数ライト合成（拡散） ===
    float3 lightSum = 0.0.xxx;

    // === 拡散・鏡面の累積 ===
    float3 diffSum = 0.0.xxx;
    float3 specSum = 0.0.xxx;
    
    // ライト0本のときに暗くならないように：ベース係数は1、ライトがあるときは弱いアンビエント
    float3 ambient = /*(gLightInfo.gLightCount == 0) ? 1.0.xxx : 0.02.xxx;*/float3(0.0, 0.0, 0.0);
    
    [loop]
    for (uint i = 0; i < gLightInfo.gLightCount; ++i)
    {
        PunctualLight L = gPunctualLights[i]; // gLights → gPunctualLights
        
        if (L.lightType == 1)
        {
            // Directional
            // L.direction は「光が進む方向」を想定
            float3 lightDir = normalize(-L.direction);
            float NdotL = saturate(dot(normal, lightDir));

            float3 lightColor = L.color.rgb * L.intensity;

            float3 diff = lightColor * NdotL;

            float3 halfVector = normalize(lightDir + viewDir);
            float NdotH = saturate(dot(normal, halfVector));
            float specular = pow(NdotH, max(gMaterial.shininess, 1.0f));
            float3 spec = lightColor * specular;

            float shadow = CalculateShadow(input.shadowPosition, normal, lightDir);

            diffSum += diff * shadow;
            specSum += spec * shadow;
        }
        else if (L.lightType == 2)
        {
            // Point
            float3 toL = L.position - position;
            float d = length(toL);
            float3 lightDir = toL / max(d, 1e-4f);

            float range = max(L.radius, 1e-3f);
            float atten = pow(saturate(1.0f - d / range), max(L.decay, 1e-3f));

            float NdotL = saturate(dot(normal, lightDir));
            float3 lightColor = L.color.rgb * (L.intensity * atten);

            float3 diff = lightColor * NdotL;

            float3 halfVector = normalize(lightDir + viewDir);
            float NdotH = saturate(dot(normal, halfVector));
            float specular = pow(NdotH, max(gMaterial.shininess, 1.0f));
            float3 spec = lightColor * specular;

            diffSum += diff;
            specSum += spec;
        }
        else if (L.lightType == 3)
        {
            // Spot
            float3 toL = L.position - position;
            float d = length(toL);
            float3 lightDir = toL / max(d, 1e-4f);

            float range = max(L.distance, 1e-3f);
            float atten = pow(saturate(1.0f - d / range), max(L.decay, 1e-3f));

            float3 dir = normalize(L.direction);
            float ct = dot(-dir, lightDir);
            float spot = smoothstep(L.cosAngle, L.cosFalloffStart, ct);

            float NdotL = saturate(dot(normal, lightDir));
            float3 lightColor = L.color.rgb * (L.intensity * atten * spot);

            float3 diff = lightColor * NdotL;

            float3 halfVector = normalize(lightDir + viewDir);
            float NdotH = saturate(dot(normal, halfVector));
            float specular = pow(NdotH, max(gMaterial.shininess, 1.0f));
            float3 spec = lightColor * specular;

            diffSum += diff;
            specSum += spec;
        }
        else
        {
            continue;
        }
    }
    
    // Dissolve処理
    float maskValue = gDissolveMaskTexture.Sample(gSampler, input.texcoord).r; // マスクテクスチャのサンプリング
    
    // エッジカラーを計算
    float edge = smoothstep(gDissolveSetting.threshold, gDissolveSetting.threshold + gDissolveSetting.edgeThickness, maskValue);
    float4 edgeCol = gDissolveSetting.edgeColor * (1.0 - edge); // エッジカラー
    
    // ライトが0の場合の補正
    lightSum = (gLightInfo.gLightCount == 0) ? 1.0.xxx : (lightSum + 0.02.xxx);
    
    lightSum = ambient + diffSum + specSum;
    
    // 出力処理
    output.color = gMaterial.color * lerp(textureColor, edgeCol, 1.0 - step(maskValue, gDissolveSetting.threshold));; // αもここで確保
    output.color.rgb *= lightSum; // ライティング適用（RGBのみ）
    
    // 環境マップ合成
    output.color.rgb = lerp(output.color.rgb, environmentColor.rgb, gMaterial.reflectionRate);
    
    // α値がほぼ0の場合にピクセルを破棄
    if (output.color.a < 0.001f)
    {
        discard;
    }

    return output;
}
