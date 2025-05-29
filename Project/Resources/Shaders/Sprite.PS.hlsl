#include "Sprite.hlsli"

struct Material
{
    float4 color;
    float4x4 uvTransform;
};

struct ReloadProgress
{
    int isReloading; // リロード中かどうか
    float reloadProgress; // リロードの進捗
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<ReloadProgress> gReloadProgress : register(b1);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // 通常のUV変換とテクスチャ取得
    float4 transformedUV = mul(float4(input.texcoord, 0, 1), gMaterial.uvTransform);
    float4 texColor = gTexture.Sample(gSampler, transformedUV.xy);

    // デフォルトの最終色（通常表示）
    output.color = texColor * gMaterial.color;

    // ---------- リロード時だけ扇形マスク処理 ----------
    if (gReloadProgress.isReloading == 1)
    {
        float2 center = float2(0.5f, 0.5f); // 中心をUVの真ん中と仮定
        float2 uv = input.texcoord;
        float2 delta = uv - center;

        float angle = atan2(delta.y, delta.x); // -π ～ π
        if (angle < 0)
            angle += 6.2831853; // 0 ～ 2π に補正

        float progressAngle = gReloadProgress.reloadProgress * 6.2831853; // 進行角度

        float dist = length(delta);
        if (angle > progressAngle || dist > 0.5f)
        {
            // 扇形外または円の外 → アルファ0で透明に
            output.color.a = 0.0f;
        }
    }

    return output;
}
