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
    float4 color : COLOR0;
    nointerpolation float shape : TEXCOORD1;
};

struct Particle
{
    float3 translate;
    float3 velocity;
    float3 scale;
    float lifeTime;
    float currentTime;
    float4 color;
};

struct PerView
{
    float4x4 viewProjection;
    float4x4 billboardMatrix;
};

StructuredBuffer<Particle> gParticles : register(t0);
ConstantBuffer<PerView> gPerView : register(b0);

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    Particle particle = gParticles[instanceId];

    float4x4 worldMatrix = gPerView.billboardMatrix;
    worldMatrix[0].xyz *= particle.scale.x;
    worldMatrix[1].xyz *= particle.scale.y;
    worldMatrix[2].xyz *= particle.scale.z;
    worldMatrix[3].xyz = particle.translate;
    worldMatrix[3].w = 1.0f;

    float4 worldPosition = mul(input.position, worldMatrix);
    output.position = mul(worldPosition, gPerView.viewProjection);
    output.texcoord = input.texcoord;
    output.color = particle.color;
    output.shape = 0.0f;
    return output;
}
