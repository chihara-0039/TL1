struct VertexShaderInput
{
    float4 position : POSITION0;
};

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
};

struct InstanceData
{
    float4x4 World;
    float4 Color;
    float Shininess;
    float Metallic;
    float Emissive;
    float3 padding;
};

struct ViewProjectionMatrix
{
    float4x4 ViewProjection;
    float4x4 lightViewProjection;
};

ConstantBuffer<ViewProjectionMatrix> gViewProjection : register(b0);
StructuredBuffer<InstanceData> gInstances : register(t2);

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    float4x4 worldMatrix = gInstances[instanceId].World;
    float4x4 lightWVP = mul(worldMatrix, gViewProjection.lightViewProjection);
    output.position = mul(input.position, lightWVP);
    return output;
}
