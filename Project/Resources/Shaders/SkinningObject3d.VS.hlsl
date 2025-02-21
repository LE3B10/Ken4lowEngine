#include "SkinningObject3d.hlsli"

//頂点シェーダーへの入力頂点構造
struct VertexShaderInput
{
    //POSITIONのことをセマンティクスという
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 weight : WEIGHT0;
    int4 index : INDEX0;
};

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

// MatrixPaletteを追加
struct Well
{
    float4x4 skeletonSpaceMatrix;
    float4x4 skeletonSpaceInveerseTransposeMatrix;
};

// Skinning計算の準備
struct Skinned
{
    float4 position;
    float3 normal;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
StructuredBuffer<Well> gMatrixPalette : register(t0);

// Skinningを行う関数
Skinned Skinning(VertexShaderInput input)
{
    Skinned skinned;
    
    // 位置の変換
    skinned.position = mul(input.position, gMatrixPalette[input.index.x].skeletonSpaceMatrix) * input.weight.x;
    skinned.position += mul(input.position, gMatrixPalette[input.index.y].skeletonSpaceMatrix) * input.weight.y;
    skinned.position += mul(input.position, gMatrixPalette[input.index.z].skeletonSpaceMatrix) * input.weight.z;
    skinned.position += mul(input.position, gMatrixPalette[input.index.w].skeletonSpaceMatrix) * input.weight.w;
    skinned.position.w = 1.0f; // 確実に1を入れる
    
    // 法線の変換
    skinned.normal = mul(input.normal, (float3x3) gMatrixPalette[input.index.x].skeletonSpaceInveerseTransposeMatrix) * input.weight.x;
    skinned.normal += mul(input.normal, (float3x3) gMatrixPalette[input.index.y].skeletonSpaceInveerseTransposeMatrix) * input.weight.y;
    skinned.normal += mul(input.normal, (float3x3) gMatrixPalette[input.index.z].skeletonSpaceInveerseTransposeMatrix) * input.weight.w;
    skinned.normal += mul(input.normal, (float3x3) gMatrixPalette[input.index.w].skeletonSpaceInveerseTransposeMatrix) * input.weight.z;
    skinned.normal = normalize(skinned.normal); // 正規化して戻してあげる
    
    return skinned;
}


//頂点シェーダー
VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    Skinned skinned = Skinning(input); // まずSkinning計算を行って、Skinning後の頂点情報を手に入れる。ここでの頂点もSkeletonSpace
    
    // Skinning1結果を使って変換
    output.position = mul(input.position, gTransformationMatrix.WVP);
    output.worldPosition = mul(skinned.position, gTransformationMatrix.World).xyz;
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, (float3x3) gTransformationMatrix.WorldInverseTranspose));
    return output;
}
