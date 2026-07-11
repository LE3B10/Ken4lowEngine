struct InstanceData
{
    float4x4 world;
    float4x4 worldInverseTranspose;
    float4 color;
};

struct PerView
{
    float4x4 viewProjection;
};

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

StructuredBuffer<InstanceData> gInstances : register(t0);
ConstantBuffer<PerView> gPerView : register(b0);

float4 main(VertexShaderInput input, uint instanceId : SV_InstanceID) : SV_POSITION
{
    // 全Instanceを同じComponent IDで描きつつ、各InstanceのWorld行列はGPU Bufferから取得する。
    const float4 worldPosition = mul(input.position, gInstances[instanceId].world);
    return mul(worldPosition, gPerView.viewProjection);
}
