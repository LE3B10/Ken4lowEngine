struct VertexShaderOutput
{
    float4 position : SV_POSITION; // 変換後の頂点位置
    float2 texcoord : TEXCOORD0; // テクスチャUV
    float4 color : COLOR0; // パーティクルカラー
    nointerpolation uint type : TEXCOORD1; // パーティクルタイプ
};