struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 lightSpacePosition : POSITION0;
    float3 worldPosition : POSITION1; // ワールド空間の位置を追加
};

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 lightViewProjection;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexSgaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexSgaderInput input)
{
    VertexShaderOutput output;
    output.position = mul(input.position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul((float3x3) gTransformationMatrix.World, input.normal));
    
    // その世界座標を「ライト視点の行列」で変換してピクセルシェーダーに送る
    float4 worldPos = mul(input.position, gTransformationMatrix.World);
    
    output.lightSpacePosition = mul(worldPos, gTransformationMatrix.lightViewProjection);
    output.worldPosition = worldPos.xyz;
    
    return output;
}