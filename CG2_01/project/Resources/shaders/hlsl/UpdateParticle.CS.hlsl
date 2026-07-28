static const uint kMaxParticles = 1024;

struct Particle
{
    float3 translate;
    float3 velocity;
    float3 scale;
    float lifeTime;
    float currentTime;
    float4 color;
};

struct PerFrame
{
    float time;
    float deltaTime;
};

ConstantBuffer<PerFrame> gPerFrame : register(b1);
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;
    if (particleIndex >= kMaxParticles)
    {
        return;
    }

    Particle particle = gParticles[particleIndex];

    // alphaが0のParticleは未使用、または寿命切れなので更新しない。
    if (particle.color.a == 0.0f)
    {
        return;
    }

    particle.translate += particle.velocity * gPerFrame.deltaTime;
    particle.currentTime += gPerFrame.deltaTime;

    if (particle.currentTime >= particle.lifeTime)
    {
        particle.color.a = 0.0f;
        particle.scale = float3(0.0f, 0.0f, 0.0f);

        int freeListIndex;
        // 寿命切れになったParticleをFreeListへ戻し、次のEmitで再利用できるようにする。
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);

        if (freeListIndex + 1 < kMaxParticles)
        {
            gFreeList[freeListIndex + 1] = particleIndex;
        }
        else
        {
            // FreeListが満杯ならIndexだけ戻して、リストの破綻を防ぐ。
            int unused;
            InterlockedAdd(gFreeListIndex[0], -1, unused);
        }
    }
    else
    {
        float alpha = 1.0f - (particle.currentTime / particle.lifeTime);
        particle.color.a = saturate(alpha);
    }

    gParticles[particleIndex] = particle;
}
