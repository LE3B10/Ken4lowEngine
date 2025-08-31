// スキニングのコンピュートシェーダ

// 頂点情報
struct Vertex
{
    float4 position; // 座標
    float2 texcoord; // テクスチャ座標
    float3 normal; // 法線
};

// インフルエンス
struct VertexInfulence
{
    float4 weight; // ウェイト
    int4 index; // インデックス
};

// MatrixPaletteを追加
struct Well
{
    float4x4 skeletonSpaceMatrix;
    float4x4 skeletonSpaceInverseTransposeMatrix;
};

// 変換行列
struct TransformationAnimationMatrix
{
    float4x4 WVP; // ワールド・ビュー・プロジェクション行列
    float4x4 World; // ワールド行列
    float4x4 WorldInverseTranspose; // 法線変換用（法線行列）
};

// 設定
struct SkinningSettings
{
    uint numVertices; // 頂点数
    int isSkinning; // スキニングの有効するかどうか
};

RWStructuredBuffer<Vertex> gOutputVertices : register(u0); // 頂点出力

StructuredBuffer<Well> gMatrixPalette : register(t0); // マトリックスパレット
StructuredBuffer<Vertex> gInputVertices : register(t1); // 頂点入力
StructuredBuffer<VertexInfulence> gInfulences : register(t2); // インフルエンス

ConstantBuffer<TransformationAnimationMatrix> gTransformationAnimationMatrix : register(b0); // 変換行列
ConstantBuffer<SkinningSettings> gSetting : register(b1); // スキニングの設定

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint vertexIndex = DTid.x;
    
}