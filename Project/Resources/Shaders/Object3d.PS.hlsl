#include "Object3d.hlsli"

//ピクセルシェーダーの出力
struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// マテリアル
struct Material
{
    float4 color;
    int enableLighting;
    float shininess;
    float4x4 uvTransform;
};

// 平行光源
struct DirectionalLight
{
    float4 color; //!< ライトの色
    float3 direction; //ライトの向き
    float intensity; //輝度
};

// カメラ
struct Camera
{
    float3 worldPosition;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// ピクセルシェーダー
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // テクスチャ座標を変換
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);

    // テクスチャの色を取得
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    
    // Lightingする場合
    if (gMaterial.enableLighting != 0)
    {
        // ライト方向と法線、カメラ方向を取得
        float3 lightDir = normalize(-gDirectionalLight.direction);
        float3 normal = normalize(input.normal);
        float3 viewDir = normalize(gCamera.worldPosition - input.worldPosition);
        
        // 環境光
        float3 ambientColor = gMaterial.color.rgb * gDirectionalLight.color.rgb * 0.02f; // 環境光を少し減らす
    
        /// ---------- 平行光源の処理 ---------- ///
        
        // ハーフランバート反射の計算
        float NdotL = dot(normal, lightDir); // 法線と光の角度
        float halfLambertFactor = saturate(pow(NdotL * 0.5f + 0.5f, 2.0f)); // ハーフランバート反射
    
        // 平行光源の拡散反射（Diffuse）
        float3 diffuseColor = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * gDirectionalLight.intensity * halfLambertFactor;

        // 平行光源の鏡面反射（Specular）
        float3 specularColor = float3(0.0f, 0.0f, 0.0f);
        if (gDirectionalLight.intensity > 0.0f && NdotL > 0.0f)
        {
            float3 halfVector = normalize(lightDir + viewDir);
            float NdotH = max(dot(normal, halfVector), 0.0f);
            float shininess = max(gMaterial.shininess, 50.0f); // 光沢度を調整
            specularColor = float3(1.0f, 1.0f, 1.0f) * pow(NdotH, shininess) * gDirectionalLight.intensity;
        }
        
        // 最終色の合成
        float3 finalColor = diffuseColor + specularColor * 1.2f;
        
        // ライティング結果を合成
        output.color.rgb = saturate(finalColor);
        
        // α値にはライティングを適用しない
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    {
        // ライティングを無効にした場合の処理
        output.color = gMaterial.color * textureColor;
    }
    
    // アルファ値がほぼ0の場合にピクセルを破棄
    if (output.color.a < 0.001f)
    {
        discard;
    }
    
    return output;
}
