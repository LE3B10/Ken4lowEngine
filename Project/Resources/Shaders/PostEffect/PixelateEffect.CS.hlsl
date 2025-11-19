// ピクセル化エフェクト Compute Shader

struct PixelateSettingCB
{
    float2 screenSize; // (width, height)
    float blockSize; // ピクセル単位
    float strength; // 0=なし,1=フル
};

Texture2D<float4> gInput : register(t0); // 入力テクスチャ
RWTexture2D<float4> gOutput : register(u0); // 出力テクスチャ

SamplerState gLinearSampler : register(s0);
SamplerState gPointSampler : register(s1);

ConstantBuffer<PixelateSettingCB> gSettingCB : register(b0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 coord = DTid.xy;

    // 画面外ガード
    if (coord.x >= (uint) gSettingCB.screenSize.x || coord.y >= (uint) gSettingCB.screenSize.y)
        return;

    float2 uv = (coord + 0.5) / gSettingCB.screenSize;

    // ブロック座標（BlockSize ピクセル単位で量子化）
    float2 block = floor(coord / gSettingCB.blockSize) * gSettingCB.blockSize + gSettingCB.blockSize * 0.5;
    float2 blockUV = block / gSettingCB.screenSize;

    float4 original = gInput.SampleLevel(gLinearSampler, uv, 0);
    float4 pixelated = gInput.SampleLevel(gPointSampler, blockUV, 0);

    float4 col = lerp(original, pixelated, saturate(gSettingCB.strength));
    gOutput[coord] = col;
}