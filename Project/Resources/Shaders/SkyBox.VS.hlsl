#include "SkyBox.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
};

//頂点シェーダーへの入力頂点構造
struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    output.position = mul(input.position, gTransformationMatrix.WVP).xyww;
    output.texcoord = input.position.xyz;
    return output;
}