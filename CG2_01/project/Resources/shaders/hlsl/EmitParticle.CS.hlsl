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

struct EmitterSphere
{
    float3 translate;
    float radius;
    uint count;
    float frequency;
    float frequencyTime;
    uint emit;
};

struct PerFrame
{
    float time;
    float deltaTime;
};

float32_t rand3dTo1d(float32_t3 value)
{
    float32_t3 dotDir = float32_t3(12.9898f, 78.233f, 37.719f);
    float32_t smallValue = sin(dot(value, dotDir));
    float32_t random = frac(smallValue * 143758.5453f);
    return random;
}

float32_t3 rand3dTo3d(float32_t3 value)
{
    return float32_t3(
        rand3dTo1d(value),
        rand3dTo1d(value + float32_t3(31.34f, 11.17f, 47.53f)),
        rand3dTo1d(value + float32_t3(59.11f, 83.31f, 19.97f)));
}

class RandomGenerator
{
    float32_t3 seed;

    float32_t Generate1d()
    {
        seed = rand3dTo3d(seed);
        return rand3dTo1d(seed);
    }

    float32_t3 Generate3d()
    {
        seed = rand3dTo3d(seed);
        return seed;
    }
};

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);

[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (gEmitter.emit == 0)
    {
        return;
    }

    RandomGenerator generator;
    generator.seed = float32_t3(gPerFrame.time, gPerFrame.deltaTime, 0.0f);

    for (uint countIndex = 0; countIndex < gEmitter.count; ++countIndex)
    {
        int freeListIndex;
        // FreeListの末尾を1つ取り出す。複数threadでも重複しないようatomicで操作する。
        InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);

        if (freeListIndex < 0 || freeListIndex >= kMaxParticles)
        {
            // 空きが無かったので、先に減らしたFreeListIndexを戻す。
            int unused;
            InterlockedAdd(gFreeListIndex[0], 1, unused);
            continue;
        }

        // 空いているParticleスロットを再利用して、新しいParticleを書き込む。
        uint particleIndex = gFreeList[freeListIndex];

        float32_t3 randomDirection = generator.Generate3d() * 2.0f - 1.0f;
        float32_t3 randomOffset = randomDirection * gEmitter.radius;
        float32_t scale = 0.18f + generator.Generate1d() * 0.45f;
        float32_t3 velocity = normalize(randomDirection + float32_t3(0.1f, 0.9f, 0.0f));
        velocity *= 0.35f + generator.Generate1d() * 1.25f;

        Particle particle = (Particle)0;
        particle.translate = gEmitter.translate + randomOffset;
        particle.velocity = velocity;
        particle.scale = float32_t3(scale, scale, scale);
        particle.lifeTime = 1.2f + generator.Generate1d() * 0.8f;
        particle.currentTime = 0.0f;
        particle.color = float32_t4(1.0f, 0.15f + generator.Generate1d() * 0.45f, 0.05f, 1.0f);

        gParticles[particleIndex] = particle;
    }
}
