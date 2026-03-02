#include "Object3d.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

struct ShadowParameter
{
    float4x4 lightViewProjection;
    float shadowBias;
    float3 padding; // アラインメントのためのパディング
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
ConstantBuffer<ShadowParameter> gShadowParameter : register(b4);

//頂点シェーダーへの入力頂点構造
struct VertexShaderInput
{
    //POSITIONのことをセマンティクスという
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

//頂点シェーダー
VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    //入力された頂点座標を出職データに代入
    output.position = mul(input.position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, (float3x3) gTransformationMatrix.WorldInverseTranspose));

    float4 worldPosition = mul(input.position, gTransformationMatrix.World);
    
    output.worldPosition = worldPosition.xyz;
    output.shadowPosition = mul(float4(output.worldPosition, 1.0f), gShadowParameter.lightViewProjection);
    
    return output;
}
