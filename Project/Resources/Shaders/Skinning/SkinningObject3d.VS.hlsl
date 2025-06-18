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

struct TransformationAnimationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

struct SkinningSettings
{
    int isSkinning;
};

// MatrixPaletteを追加
struct Well
{
    float4x4 skeletonSpaceMatrix;
    float4x4 skeletonSpaceInverseTransposeMatrix;
};

// Skinning計算の準備
struct Skinned
{
    float4 position;
    float3 normal;
};

ConstantBuffer<TransformationAnimationMatrix> gTransformationAnimationMatrix : register(b0);
ConstantBuffer<SkinningSettings> gSkinningSettings : register(b1);

StructuredBuffer<Well> gMatrixPalette : register(t1);

// Skinningを行う関数
Skinned Skinning(VertexShaderInput input)
{
    Skinned skinned;
    
    if (gSkinningSettings.isSkinning == 0)
    {
        // スキニングしない：元の頂点と法線をそのまま使用
        skinned.position = input.position;
        skinned.normal = input.normal;
        return skinned;
    }
    
    // 位置の変換
    skinned.position = mul(input.position, gMatrixPalette[input.index.x].skeletonSpaceMatrix) * input.weight.x;
    skinned.position += mul(input.position, gMatrixPalette[input.index.y].skeletonSpaceMatrix) * input.weight.y;
    skinned.position += mul(input.position, gMatrixPalette[input.index.z].skeletonSpaceMatrix) * input.weight.z;
    skinned.position += mul(input.position, gMatrixPalette[input.index.w].skeletonSpaceMatrix) * input.weight.w;
    skinned.position /= skinned.position.w;
    
    // 法線の変換
    skinned.normal = mul(input.normal, (float3x3) gMatrixPalette[input.index.x].skeletonSpaceInverseTransposeMatrix) * input.weight.x;
    skinned.normal += mul(input.normal, (float3x3) gMatrixPalette[input.index.y].skeletonSpaceInverseTransposeMatrix) * input.weight.y;
    skinned.normal += mul(input.normal, (float3x3) gMatrixPalette[input.index.z].skeletonSpaceInverseTransposeMatrix) * input.weight.z;
    skinned.normal += mul(input.normal, (float3x3) gMatrixPalette[input.index.w].skeletonSpaceInverseTransposeMatrix) * input.weight.w;
    skinned.normal = normalize(skinned.normal); // 正規化して戻してあげる
    
    return skinned;
}

//頂点シェーダー
VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    Skinned skinned = Skinning(input); // まずSkinning計算を行って、Skinning後の頂点情報を手に入れる。ここでの頂点もSkeletonSpace
    
    // Skinning1結果を使って変換
    output.position = mul(skinned.position, gTransformationAnimationMatrix.WVP); // 正しい変換順序
    output.worldPosition = mul(skinned.position, gTransformationAnimationMatrix.World).xyz;
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(skinned.normal, (float3x3) gTransformationAnimationMatrix.WorldInverseTranspose));
    return output;
}
