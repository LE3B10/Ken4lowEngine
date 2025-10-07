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

struct LightInfo
{
    uint gLightCount;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<Camera> gCamera : register(b1);
ConstantBuffer<LightInfo> gLightInfo : register(b2);

Texture2D<float4> gTexture : register(t0); // テクスチャ
TextureCube<float4> gEnvironmentTexture : register(t1); // 環境マップ
StructuredBuffer<PunctualLight> gPunctualLights : register(t2); // パンクチュアルライト

SamplerState gSampler : register(s0);

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
        PunctualLight L = gPunctualLights[i]; // ★ gLights → gPunctualLights

        float atten = 1.0;
        float3 Ldir = normalize(L.direction);
        
        if (L.lightType == 1)
        {
            // Directional
            float NdotL = saturate(dot(normal, Ldir));
            
            float3 lightColor = L.color.rgb * (L.intensity * atten);
        
            // 拡散
            diffSum += lightColor * NdotL;
        
            // 鏡面反射
            float3 halfVector = normalize(Ldir + viewDir);
            float NdotH = saturate(dot(normal, halfVector));
            float specular = pow(NdotH, max(gMaterial.shininess, 1.0f));
            specSum += lightColor * specular;
       
            lightSum = ambient + diffSum + specSum;
            
            lightSum += L.color.rgb * (L.intensity * NdotL);
        }
        else if (L.lightType == 2)
        {
            // Point
            float3 toL = L.position - position;
            float d = length(toL);
            Ldir = toL / max(d, 1e-4);

            float range = max(L.radius, 1e-3);
            atten = pow(saturate(1.0 - d / range), max(L.decay, 1e-3));

            float NdotL = saturate(dot(normal, Ldir));
            float3 lightColor = L.color.rgb * (L.intensity * atten);
        
            // 拡散
            diffSum += lightColor * NdotL;
        
            // 鏡面反射
            float3 halfVector = normalize(Ldir + viewDir);
            float NdotH = saturate(dot(normal, halfVector));
            float specular = pow(NdotH, max(gMaterial.shininess, 1.0f));
            specSum += lightColor * specular;
       
            lightSum = ambient + diffSum + specSum;
            lightSum += L.color.rgb * (L.intensity * atten * NdotL);
        }
        else if (L.lightType == 3)
        {
            // Spot
            // ① ライト→点 ベクトルにする（position - L.position）
            float3 toL = L.position - position;
            float d = length(toL);
            float3 Ldir = toL / max(d, 1e-4);

            // 距離減衰
            float range = max(L.distance, 1e-3);
            float atten = pow(saturate(1.0 - d / range), max(L.decay, 1e-3));

            // ② スポット角の評価：
            //    ct = dot(-dir, L) で「向いている先」と「ライト→点」を比較
            float3 dir = normalize(L.direction); // 「向いている先」
            float ct = dot(-dir, Ldir);

            // 内側>=外側（UI/CPUで担保）を前提に smoothstep(edge0=edgeOuter, edge1=edgeInner, x=ct)
            float spot = smoothstep(L.cosAngle, L.cosFalloffStart, ct);

            // Lambert（N は点の法線、Lambert は 点→光 のベクトルを使うので -Ldir）
            float NdotL = saturate(dot(normal, Ldir));
            float halfLambertFactor = saturate(pow(-NdotL * 0.5f + 0.5f, 2.0f)); // ←ここが逆
            float3 lightColor = L.color.rgb * (L.intensity * atten);
        
            // 拡散
            diffSum += lightColor * halfLambertFactor;
        
            // 鏡面反射
            float3 halfVector = normalize(Ldir + viewDir);
            float NdotH = saturate(dot(normal, halfVector));
            float specular = pow(saturate(dot(normal, halfVector)), gMaterial.shininess);
            specSum += L.color.rgb * (L.intensity * atten) * spot * specular;
       
            lightSum = ambient + diffSum + specSum;
            
            lightSum += L.color.rgb * (L.intensity * atten * spot * NdotL);
        }
        else
        {
            continue;
        }
    }

    // お好みでほんの少し環境光（なければ 0.0 でOK）
    lightSum = (gLightInfo.gLightCount == 0) ? 1.0.xxx : (lightSum + 0.02.xxx);

    // === 既存の出力処理（変数名そのまま） ===
    output.color = gMaterial.color * textureColor; // αもここで確保
    output.color.rgb *= lightSum; // ライティング適用（RGBのみ）

    // 既存の環境マップ合成（そのまま）
    output.color.rgb += environmentColor.rgb * gMaterial.reflectionRate;
    output.color.rgb = lerp(output.color.rgb, environmentColor.rgb, gMaterial.reflectionRate);
    
    // α値がほぼ0の場合にピクセルを破棄
    if (output.color.a < 0.001f)
    {
        discard;
    }

    return output;
}
