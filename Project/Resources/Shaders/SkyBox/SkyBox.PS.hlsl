#include "SkyBox.hlsli"

//ピクセルシェーダーの出力
struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// マテリアル
struct Material
{
    float4 color; // オブジェクトの色
    float4x4 uvTransform; // UVTransform
    uint textureIndex; // 使用するテクスチャのインデックス
};

// 使用するテクスチャの最大数 : SRV配列のサイズに依存
static const uint textureCount = 1024;

ConstantBuffer<Material> gMaterial : register(b0);

TextureCube<float4> gTexture[textureCount] : register(t0);
SamplerState gSampler : register(s0);

// ピクセルシェーダー (PS) のメイン関数 (メインエントリーポイント)
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4 textureColor = gTexture[gMaterial.textureIndex].Sample(gSampler, input.texcoord);
    output.color = textureColor * gMaterial.color;
	return output;
}