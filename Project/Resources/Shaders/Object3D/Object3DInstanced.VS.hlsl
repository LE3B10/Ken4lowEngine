#include "Object3d.hlsli"
#include "ShadowCommon.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

StructuredBuffer<TransformationMatrix> gInstanceTransforms : register(t5);
ConstantBuffer<ShadowParameter> gShadowParameter : register(b4);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    TransformationMatrix transform = gInstanceTransforms[instanceId];
    float4 worldPosition = mul(input.position, transform.World);

    output.position = mul(input.position, transform.WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, (float3x3) transform.WorldInverseTranspose));
    output.worldPosition = worldPosition.xyz;
    output.shadowPosition = mul(worldPosition, gShadowParameter.lightViewProjection);
    return output;
}
