struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 lightSpacePosition : POSITION0;
    float3 worldPosition : POSITION1;
};

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 lightViewProjection;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

StructuredBuffer<float4x4> gJointMatrices : register(t2);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    int4 jointIndices : BLENDINDICES0;
    float4 weights : BLENDWEIGHT0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    // Skinning deformation
    float4x4 skinningMatrix = 
        gJointMatrices[input.jointIndices.x] * input.weights.x +
        gJointMatrices[input.jointIndices.y] * input.weights.y +
        gJointMatrices[input.jointIndices.z] * input.weights.z +
        gJointMatrices[input.jointIndices.w] * input.weights.w;
        
    // Deform position and normal
    float4 deformedPosition = mul(input.position, skinningMatrix);
    float3 deformedNormal = normalize(mul(input.normal, (float3x3)skinningMatrix));
    
    // Standard transformations
    output.position = mul(deformedPosition, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(deformedNormal, (float3x3)gTransformationMatrix.World));
    
    float4 worldPos = mul(deformedPosition, gTransformationMatrix.World);
    output.lightSpacePosition = mul(worldPos, gTransformationMatrix.lightViewProjection);
    output.worldPosition = worldPos.xyz;
    
    return output;
}




