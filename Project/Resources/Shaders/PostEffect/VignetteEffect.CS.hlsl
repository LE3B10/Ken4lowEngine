// Vinetteのコンピュートシェーダー

struct VignetteSetting
{
    float power; // vignetteの強さ（exponent）
    float range; // vignetteの範囲（中心からどこまで影響を与えるか）
};

RWTexture2D<float4> gOutputTexture : register(u0); // 出力テクスチャ
Texture2D<float4> gInputTexture : register(t0); // 入力テクスチャ
ConstantBuffer<VignetteSetting> gVignetteSetting : register(b0); // 定数バッファ

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 texSize;
    gOutputTexture.GetDimensions(texSize.x, texSize.y);

    float2 uv = DTid.xy / (float2) texSize;
    float2 center = float2(0.5, 0.5);
    float dist = distance(uv, center);

    // vignetteの計算
    float vignette = pow(1.0 - smoothstep(0.0, gVignetteSetting.range, dist), gVignetteSetting.power);

    float4 color = gInputTexture.Load(int3(DTid.xy, 0));
    gOutputTexture[DTid.xy] = color * vignette;
}