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
};

ConstantBuffer<Material> gMaterial : register(b0);

TextureCube<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// ピクセルシェーダー (PS) のメイン関数 (メインエントリーポイント)
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    output.color = textureColor * gMaterial.color;
	return output;
}