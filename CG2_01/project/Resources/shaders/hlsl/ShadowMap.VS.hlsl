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

struct VertexShaderInput
{
    float4 position : POSITION0;
};

struct ShaderVertexOutput
{
    float4 position : SV_POSITION;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    // ★重要：WVPではなく「World行列 × ライト行列」を直接計算して使う
    float4x4 lightWVP = mul(gTransformationMatrix.World, gTransformationMatrix.lightViewProjection);
    output.position = mul(input.position, lightWVP);
    return output;
}