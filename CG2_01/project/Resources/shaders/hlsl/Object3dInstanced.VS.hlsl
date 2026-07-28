struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 lightSpacePosition : POSITION0;
    float3 worldPosition : POSITION1;
    
    // インスタンスデータ
    float4 color : COLOR0;
    float shininess : SHININESS0;
    float metallic : METALLIC0;
    float emissive : EMISSIVE0;
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
StructuredBuffer<InstanceData> gInstances : register(t2); // SRV register t2

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    
    float4x4 worldMatrix = gInstances[instanceId].World;
    
    float4 worldPos = mul(input.position, worldMatrix);
    output.position = mul(worldPos, gViewProjection.ViewProjection);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul((float3x3)worldMatrix, input.normal));
    
    output.lightSpacePosition = mul(worldPos, gViewProjection.lightViewProjection);
    output.worldPosition = worldPos.xyz;
    
    // マテリアルパラメータをパス
    output.color = gInstances[instanceId].Color;
    output.shininess = gInstances[instanceId].Shininess;
    output.metallic = gInstances[instanceId].Metallic;
    output.emissive = gInstances[instanceId].Emissive;
    
    return output;
}
