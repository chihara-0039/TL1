#include "ParticleManager.h"
#include "StageMap.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <random>

using namespace Microsoft::WRL;

namespace {
    constexpr uint32_t kGPUParticleEmitCount = 10;
    constexpr float kGPUParticleEmitFrequency = 0.5f;

    enum class GPUParticleComputeRoot : uint32_t {
        ParticlesUAV = 0,
        FreeListIndexUAV,
        EmitterCBV,
        PerFrameCBV,
        FreeListUAV,
        Count
    };

    constexpr uint32_t ToRootIndex(GPUParticleComputeRoot root) {
        return static_cast<uint32_t>(root);
    }

    D3D12_HEAP_PROPERTIES MakeHeapProperties(D3D12_HEAP_TYPE type) {
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = type;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;
        return heapProps;
    }

    D3D12_RESOURCE_DESC MakeBufferDesc(UINT64 width, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE) {
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = width;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.SampleDesc.Count = 1;
        desc.Flags = flags;
        return desc;
    }

    UINT64 AlignConstantBufferSize(size_t size) {
        return (static_cast<UINT64>(size) + 0xff) & ~0xffull;
    }

    ComPtr<ID3D12Resource> CreateBufferResource(
        ID3D12Device* device,
        D3D12_HEAP_TYPE heapType,
        UINT64 byteSize,
        D3D12_RESOURCE_STATES initialState,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE) {

        auto heapProps = MakeHeapProperties(heapType);
        auto desc = MakeBufferDesc(byteSize, flags);

        ComPtr<ID3D12Resource> resource;
        HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            initialState,
            nullptr,
            IID_PPV_ARGS(&resource));
        assert(SUCCEEDED(hr));
        return resource;
    }
}

// 乱数生成器
static std::random_device seed_gen;
static std::mt19937_64 engine(seed_gen());

void ParticleManager::Initialize(DirectXCommon* dxCommon, TextureManager* textureManager) {
    assert(dxCommon);
    dxCommon_ = dxCommon;
    textureManager_ = textureManager;

    // 1. テクスチャ読み込み (デフォルト)
    textureHandle_ = textureManager_->LoadTexture("Resources/UI/inventory/white.png");
    CreateParticleGroup(kDefaultParticleGroupName, textureHandle_);

    // 2. パイプライン生成
    CreateRootSignature();
    CreatePipelineState();

    // 3. メッシュ生成
    CreateMesh();
    CreateRingMesh();
    CreateCylinderMesh();

    // 4. インスタンシング用バッファ生成
    {
        auto device = dxCommon_->GetDevice();
        UINT size = sizeof(InstanceData) * kMaxParticles;

        D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
        D3D12_RESOURCE_DESC resDesc = {};
        resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resDesc.Width = size;
        resDesc.Height = 1; resDesc.DepthOrArraySize = 1; resDesc.MipLevels = 1;
        resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; resDesc.SampleDesc.Count = 1;

        HRESULT hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&instancingBuffer_));
        assert(SUCCEEDED(hr));

        instancingBuffer_->Map(0, nullptr, (void**)&instancingDataMapped_);

        instancingBufferView_.BufferLocation = instancingBuffer_->GetGPUVirtualAddress();
        instancingBufferView_.SizeInBytes = size;
        instancingBufferView_.StrideInBytes = sizeof(InstanceData);

        hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&cloudInstancingBuffer_));
        assert(SUCCEEDED(hr));
        cloudInstancingBuffer_->Map(0, nullptr, (void**)&cloudInstancingDataMapped_);
        cloudInstancingBufferView_.BufferLocation = cloudInstancingBuffer_->GetGPUVirtualAddress();
        cloudInstancingBufferView_.SizeInBytes = size;
        cloudInstancingBufferView_.StrideInBytes = sizeof(InstanceData);

        hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&ringInstancingBuffer_));
        assert(SUCCEEDED(hr));

        ringInstancingBuffer_->Map(0, nullptr, (void**)&ringInstancingDataMapped_);

        ringInstancingBufferView_.BufferLocation = ringInstancingBuffer_->GetGPUVirtualAddress();
        ringInstancingBufferView_.SizeInBytes = size;
        ringInstancingBufferView_.StrideInBytes = sizeof(InstanceData);

        hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&cylinderInstancingBuffer_));
        assert(SUCCEEDED(hr));

        cylinderInstancingBuffer_->Map(0, nullptr, (void**)&cylinderInstancingDataMapped_);

        cylinderInstancingBufferView_.BufferLocation = cylinderInstancingBuffer_->GetGPUVirtualAddress();
        cylinderInstancingBufferView_.SizeInBytes = size;
        cylinderInstancingBufferView_.StrideInBytes = sizeof(InstanceData);
    }

    CreateGPUParticleResources();
    CreateGPUParticlePipeline();
}

void ParticleManager::SetStormActive(bool active, const Vector3& center) {
    if (stormActive_ == active && (!active ||
        (stormCenter_.x == center.x && stormCenter_.y == center.y && stormCenter_.z == center.z))) {
        return;
    }
    stormActive_ = active;
    stormCenter_ = center;
    stormCloudEmitTimer_ = 0.18f;
    stormRainEmitTimer_ = 0.0f;
    stormWindEmitTimer_ = 0.0f;
    stormLightningTimer_ = 0.35f;
    stormLightningBurstTimer_ = 0.0f;
    stormLightningBurstsRemaining_ = 0;
    stormLightningFlash_ = false;

    if (!active) {
        Particles().remove_if([](const Particle& particle) {
            return particle.type == Particle::Type::StormCloud ||
                   particle.type == Particle::Type::StormRain ||
                   particle.type == Particle::Type::StormWind;
        });
    }
}

bool ParticleManager::ConsumeStormLightningFlash() {
    const bool flashed = stormLightningFlash_;
    stormLightningFlash_ = false;
    return flashed;
}

void ParticleManager::Update(float deltaTime, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix, const Vector3& playerPos, StageMap* stageMap) {
    // GPU Particle用Emitterの更新。
    // CPU側では「このフレームに射出してよいか」だけ判定し、実際のParticle生成はCSで行う。
    gpuParticlePerFrame_.time += deltaTime;
    gpuParticlePerFrame_.deltaTime = deltaTime;
    gpuParticleEmitter_.frequencyTime += deltaTime;
    gpuParticleEmitter_.emit = 0;
    if (gpuParticleEmitter_.frequencyTime >= gpuParticleEmitter_.frequency) {
        gpuParticleEmitter_.frequencyTime -= gpuParticleEmitter_.frequency;
        gpuParticleEmitter_.emit = 1;
    }

    // 0. 天候エミッターの処理
    if (weatherEmitter_.active && weatherEmitter_.emitRate > 0.0f) {
        weatherEmitter_.emitTimer += deltaTime;
        float emitInterval = 1.0f / weatherEmitter_.emitRate;
        
        while (weatherEmitter_.emitTimer >= emitInterval) {
            weatherEmitter_.emitTimer -= emitInterval;
            
            if (Particles().size() < kMaxParticles) {
                std::uniform_real_distribution<float> distX(-weatherEmitter_.size.x / 2.0f, weatherEmitter_.size.x / 2.0f);
                std::uniform_real_distribution<float> distY(-weatherEmitter_.size.y / 2.0f, weatherEmitter_.size.y / 2.0f);
                std::uniform_real_distribution<float> distZ(-weatherEmitter_.size.z / 2.0f, weatherEmitter_.size.z / 2.0f);
                
                std::uniform_real_distribution<float> randV(-1.0f, 1.0f);
                
                Particle p;
                p.transform.scale = weatherEmitter_.particleSize;
                p.transform.rotate = { 0.0f, 0.0f, 0.0f };
                
                // プレイヤーの周囲に発生させる
                p.transform.translate = {
                    playerPos.x + weatherEmitter_.center.x + distX(engine),
                    playerPos.y + weatherEmitter_.center.y + distY(engine),
                    playerPos.z + weatherEmitter_.center.z + distZ(engine)
                };
                
                p.velocity = {
                    weatherEmitter_.velocity.x + randV(engine) * weatherEmitter_.velocityRandom.x,
                    weatherEmitter_.velocity.y + randV(engine) * weatherEmitter_.velocityRandom.y,
                    weatherEmitter_.velocity.z + randV(engine) * weatherEmitter_.velocityRandom.z
                };
                
                p.type = Particle::Type::Fall;
                p.color = weatherEmitter_.color;
                p.initialAlpha = p.color.w;
                p.lifeTime = 0.0f;
                p.maxTime = weatherEmitter_.particleLife;
                Particles().push_back(p);
            }
        }
    }

    // 通常天候の背景雲も、Tempest Stormと同じ雲用パーティクルで構成する。
    if (ambientCloudEmitter_.active && ambientCloudEmitter_.emitRate > 0.0f) {
        ambientCloudEmitter_.emitTimer += deltaTime;
        const float cloudInterval = 1.0f / (std::max)(ambientCloudEmitter_.emitRate, 0.01f);
        std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
        std::uniform_real_distribution<float> zeroOne(0.0f, 1.0f);
        while (ambientCloudEmitter_.emitTimer >= cloudInterval) {
            ambientCloudEmitter_.emitTimer -= cloudInterval;
            const float baseX = ambientCloudEmitter_.center.x + unit(engine) * ambientCloudEmitter_.areaX;
            const float baseZ = ambientCloudEmitter_.center.z + unit(engine) * ambientCloudEmitter_.areaZ;
            constexpr int kWispsPerCloud = 3;
            for (int i = 0; i < kWispsPerCloud && Particles().size() < kMaxParticles; ++i) {
                const float scale = ambientCloudEmitter_.size * (0.72f + zeroOne(engine) * 0.72f);
                Particle cloud;
                cloud.type = Particle::Type::StormCloud;
                cloud.transform.translate = {
                    baseX + unit(engine) * 2.4f * ambientCloudEmitter_.size,
                    ambientCloudEmitter_.minimumHeight + zeroOne(engine) * 3.5f + unit(engine) * 0.45f,
                    baseZ + unit(engine) * 1.8f * ambientCloudEmitter_.size
                };
                cloud.transform.scale = {
                    (3.0f + zeroOne(engine) * 2.6f) * scale,
                    (0.85f + zeroOne(engine) * 0.65f) * scale,
                    1.0f
                };
                cloud.transform.rotate = { 0.0f, 0.0f, unit(engine) * 0.08f };
                cloud.velocity = { ambientCloudEmitter_.speed * (0.72f + zeroOne(engine) * 0.55f), 0.0f, 0.0f };
                cloud.color = ambientCloudEmitter_.color;
                cloud.initialAlpha = cloud.color.w;
                cloud.lifeTime = 0.0f;
                cloud.maxTime = ambientCloudEmitter_.life * (0.82f + zeroOne(engine) * 0.42f);
                Particles().push_back(cloud);
            }
        }
    }

    // 暴風雷: 雲・雨・風を継続生成し、間欠的に枝分かれ雷を落とす。
    if (stormActive_) {
        auto pushStormParticle = [&](Particle::Type type, const Vector3& position, const Vector3& scale,
                                     const Vector3& velocity, const Vector4& color, float life, float rotateZ = 0.0f) {
            if (Particles().size() >= kMaxParticles) return;
            Particle particle;
            particle.type = type;
            particle.transform.translate = position;
            particle.transform.scale = scale;
            particle.transform.rotate = { 0.0f, 0.0f, rotateZ };
            particle.velocity = velocity;
            particle.color = color;
            particle.initialAlpha = color.w;
            particle.lifeTime = 0.0f;
            particle.maxTime = life;
            Particles().push_back(particle);
        };

        std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
        std::uniform_real_distribution<float> zeroOne(0.0f, 1.0f);
        const StormEffectSettings& storm = stormSettings_;

        stormCloudEmitTimer_ += deltaTime;
        const float cloudInterval = 1.0f / (std::max)(storm.cloudEmitRate, 0.1f);
        while (stormCloudEmitTimer_ >= cloudInterval) {
            stormCloudEmitTimer_ -= cloudInterval;
            constexpr int kCloudWispsPerEmission = 3;
            const float baseCloudX = storm.randomizeCloudPosition ? unit(engine) * storm.cloudAreaX : 0.0f;
            const float baseCloudZ = storm.randomizeCloudPosition ? unit(engine) * storm.cloudAreaZ : 0.0f;
            const float baseCloudY = (std::max)(stormCenter_.y + storm.cloudHeight, stormMinimumCloudHeight_) + zeroOne(engine) * 1.4f;
            for (int wisp = 0; wisp < kCloudWispsPerEmission; ++wisp) {
                const float cloudScale = storm.randomizeCloudSize ? 0.78f + zeroOne(engine) * 0.95f : 1.0f;
                const float localSpreadX = unit(engine) * storm.cloudSize * (0.42f + zeroOne(engine) * 0.34f);
                const float localSpreadY = unit(engine) * storm.cloudSize * 0.18f;
                const float localSpreadZ = unit(engine) * storm.cloudSize * (0.30f + zeroOne(engine) * 0.30f);
                Vector4 mistColor = storm.cloudColor;
                mistColor.w *= 0.54f + zeroOne(engine) * 0.28f;
                pushStormParticle(Particle::Type::StormCloud,
                    {
                        stormCenter_.x + baseCloudX + localSpreadX,
                        baseCloudY + localSpreadY,
                        stormCenter_.z + baseCloudZ + localSpreadZ
                    },
                    {
                        (3.2f + zeroOne(engine) * 3.4f) * cloudScale * storm.cloudSize,
                        (1.0f + zeroOne(engine) * 1.15f) * cloudScale * storm.cloudSize,
                        1.0f
                    },
                    {
                        0.0025f + zeroOne(engine) * 0.0065f,
                        unit(engine) * 0.0009f,
                        unit(engine) * 0.0022f
                    },
                    mistColor,
                    storm.cloudLife * (0.85f + zeroOne(engine) * 0.55f),
                    unit(engine) * 0.26f);
            }
        }

        stormRainEmitTimer_ += deltaTime;
        const float rainInterval = 1.0f / (std::max)(storm.rainEmitRate, 0.1f);
        while (stormRainEmitTimer_ >= rainInterval) {
            stormRainEmitTimer_ -= rainInterval;
            const float rainX = storm.randomizeRainPosition ? unit(engine) * storm.rainAreaX : 0.0f;
            const float rainZ = storm.randomizeRainPosition ? unit(engine) * storm.rainAreaZ : 0.0f;
            const float rainSpeedVariation = storm.randomizeRainSpeed ? 0.78f + zeroOne(engine) * 0.44f : 1.0f;
            const float rainSpawnBaseY = (std::max)(stormCenter_.y + 2.0f, stormMinimumCloudHeight_ - 5.0f);
            pushStormParticle(Particle::Type::StormRain,
                { stormCenter_.x + rainX, rainSpawnBaseY + zeroOne(engine) * 5.5f, stormCenter_.z + rainZ },
                { 0.018f, (0.42f + zeroOne(engine) * 0.48f) * storm.rainLength, 1.0f },
                { (0.027f + zeroOne(engine) * 0.009f) * storm.rainSpeed * rainSpeedVariation, -0.132f * storm.rainSpeed * rainSpeedVariation, unit(engine) * 0.004f },
                storm.rainColor,
                0.9f + zeroOne(engine) * 0.65f, -0.14f);
        }

        stormWindEmitTimer_ += deltaTime;
        const float windInterval = 1.0f / (std::max)(storm.windEmitRate, 0.1f);
        while (stormWindEmitTimer_ >= windInterval) {
            stormWindEmitTimer_ -= windInterval;
            pushStormParticle(Particle::Type::StormWind,
                { stormCenter_.x - storm.rainAreaX + zeroOne(engine) * 2.0f, stormCenter_.y + 0.4f + zeroOne(engine) * 3.8f, stormCenter_.z + unit(engine) * storm.rainAreaZ },
                { 0.026f + zeroOne(engine) * 0.025f, (1.2f + zeroOne(engine) * 2.2f) * storm.windLength, 1.0f },
                { (0.075f + zeroOne(engine) * 0.055f) * storm.windSpeed, 0.002f + unit(engine) * 0.003f, unit(engine) * 0.003f },
                storm.windColor,
                1.1f + zeroOne(engine) * 0.7f, 1.5707963f);
        }

        stormLightningTimer_ -= deltaTime;
        stormLightningBurstTimer_ -= deltaTime;
        bool emitLightningGroup = false;
        if (stormLightningBurstsRemaining_ > 0 && stormLightningBurstTimer_ <= 0.0f) {
            emitLightningGroup = true;
            --stormLightningBurstsRemaining_;
            stormLightningBurstTimer_ = (std::max)(storm.lightningBurstInterval, 0.02f);
        } else if (stormLightningTimer_ <= 0.0f) {
            emitLightningGroup = true;
            const int configuredBurstCount = std::clamp(storm.lightningBurstCount, 1, 12);
            const int burstCount = storm.randomizeLightningBurstCount
                ? (std::min)(configuredBurstCount,
                    1 + static_cast<int>(zeroOne(engine) * static_cast<float>(configuredBurstCount)))
                : configuredBurstCount;
            stormLightningBurstsRemaining_ = burstCount - 1;
            stormLightningBurstTimer_ = (std::max)(storm.lightningBurstInterval, 0.02f);

            const float minInterval = (std::min)(storm.lightningIntervalMin, storm.lightningIntervalMax);
            const float maxInterval = (std::max)(storm.lightningIntervalMin, storm.lightningIntervalMax);
            const float frequency = (std::max)(storm.lightningFrequency, 0.05f);
            const float nextInterval = storm.randomizeLightningInterval
                ? minInterval + zeroOne(engine) * (maxInterval - minInterval)
                : (minInterval + maxInterval) * 0.5f;
            stormLightningTimer_ = nextInterval / frequency + storm.lightningBurstInterval * static_cast<float>(burstCount - 1);
        }

        if (emitLightningGroup) {
            const int simultaneousCount = std::clamp(storm.lightningSimultaneousCount, 1, 8);
            for (int strikeIndex = 0; strikeIndex < simultaneousCount; ++strikeIndex) {
                HitEffectSettings lightning;
            // One shared energy value keeps each strike visually coherent: small bolts are
            // short/thin/dim, while rare large bolts grow longer, thicker and brighter.
            const float energy = storm.randomizeLightningSize ? zeroOne(engine) : 0.5f;
            const auto rangedScale = [energy](float a, float b, float curve = 1.0f) {
                const float lo = (std::min)(a, b);
                const float hi = (std::max)(a, b);
                return lo + (hi - lo) * std::pow(energy, curve);
            };
            const float sizeScale = storm.randomizeLightningSize
                ? rangedScale(storm.lightningSizeRandomMin, storm.lightningSizeRandomMax) : 1.0f;
            const float lengthScale = storm.randomizeLightningSize
                ? rangedScale(storm.lightningLengthRandomMin, storm.lightningLengthRandomMax, 0.85f) : 1.0f;
            const float widthScale = storm.randomizeLightningSize
                ? rangedScale(storm.lightningWidthRandomMin, storm.lightningWidthRandomMax, 1.35f) : 1.0f;
            const float coreScale = storm.randomizeLightningSize
                ? rangedScale(storm.lightningCoreRandomMin, storm.lightningCoreRandomMax, 1.15f) : 1.0f;
            lightning.brightness = 2.4f * (0.68f + energy * 0.48f);
            lightning.lifeScale = 1.15f;
            lightning.size = storm.lightningStrikeSize * sizeScale;
            lightning.slashCount = 1;
            lightning.slashSpread = 0.2f;
            lightning.sparkCount = std::clamp(static_cast<int>(18.0f * coreScale), 5, 48);
            lightning.sparkSpeed = 1.8f;
            lightning.sparkLength = 1.4f * coreScale;
            lightning.ringPower = 0.0f;
            lightning.corePower = 0.18f * coreScale;
            lightning.crossPower = 0.0f;
            lightning.pillarPower = 0.0f;
            lightning.lightningCount = storm.lightningCount;
            lightning.lightningSegments = storm.lightningSegments;
            lightning.lightningLength = storm.lightningLength * lengthScale;
            lightning.lightningSpread = storm.lightningSpread;
            lightning.lightningPower = storm.lightningPower * (0.72f + energy * 0.53f);
            lightning.lightningWidth = storm.lightningWidth * widthScale;
            lightning.lightningGlowWidth = storm.lightningGlowWidth;
            lightning.lightningGlowOpacity = storm.lightningGlowOpacity;
            lightning.lightningBranchCount = storm.lightningBranchCount;
            if (storm.randomizeLightningBranchCount && storm.lightningBranchCount > 0) {
                lightning.lightningBranchCount = (std::min)(storm.lightningBranchCount,
                    1 + static_cast<int>(zeroOne(engine) * static_cast<float>(storm.lightningBranchCount)));
            }
            lightning.lightningBranchLength = storm.lightningBranchLength;
            lightning.lightningBranchSpread = storm.lightningBranchSpread;
            lightning.lightningBranchWidth = storm.lightningBranchWidth;
            lightning.lightningMode = 3;
            lightning.lightningDirection = -1.5707963f;
            lightning.lightningDirectionSpread = 0.16f;
            lightning.randomizeDirection = storm.randomizeLightningDirection;
            lightning.lightningColor = storm.lightningColor;
            lightning.lightningGlowColor = storm.lightningGlowColor;
            lightning.coreColor = storm.lightningColor;
            lightning.sparkColor = storm.lightningColor;
            lightning.sparkSecondaryColor = { 0.88f, 0.94f, 1.0f, 1.0f };

            const float groupOffsetX = strikeIndex > 0 ? unit(engine) * storm.lightningSimultaneousSpread : 0.0f;
            const float groupOffsetZ = strikeIndex > 0 ? unit(engine) * storm.lightningSimultaneousSpread : 0.0f;
            const float lightningCloudY = (std::max)(
                stormCenter_.y + storm.cloudHeight + 1.2f,
                stormMinimumCloudHeight_ + 1.2f);
            const Vector3 lightningOrigin = {
                stormCenter_.x + (storm.randomizeLightningPosition ? unit(engine) * storm.lightningAreaX : 0.0f) + groupOffsetX,
                lightningCloudY + zeroOne(engine) * 0.8f,
                stormCenter_.z + (storm.randomizeLightningPosition ? unit(engine) * storm.lightningAreaZ : 0.0f) + groupOffsetZ
            };
            stormLightningPosition_ = { lightningOrigin.x, stormCenter_.y + 0.45f, lightningOrigin.z };
            stormLightningPowerScale_ = 0.35f + energy * 1.15f;
            EmitHitEffect(lightningOrigin, lightning);
            stormLightningFlash_ = true;
            }
        }
    }

    // 1. パーティクル更新。グループごとに寿命・座標・衝突を処理する。
    for (auto& [groupName, group] : particleGroups_) {
        (void)groupName;
        for (auto it = group.particles.begin(); it != group.particles.end();) {
            it->lifeTime += deltaTime;
            if (it->lifeTime >= it->maxTime) {
                it = group.particles.erase(it);
                continue;
            }

            float oldY = it->transform.translate.y;

            it->transform.translate.x += it->velocity.x * deltaTime * 60.0f;
            it->transform.translate.y += it->velocity.y * deltaTime * 60.0f;
            it->transform.translate.z += it->velocity.z * deltaTime * 60.0f;

            if (it->type == Particle::Type::StormRain) {
                bool hitGround = false;
                float impactY = stormCenter_.y + 0.14f;

                if (stageMap) {
                    const int blockX = static_cast<int>(std::floor(it->transform.translate.x + 0.5f));
                    const int blockZ = static_cast<int>(std::floor(it->transform.translate.z + 0.5f));
                    const int oldBlockY = static_cast<int>(std::floor(oldY + 0.5f));
                    const int newBlockY = static_cast<int>(std::floor(it->transform.translate.y + 0.5f));
                    for (int y = oldBlockY; y >= newBlockY; --y) {
                        const MapCell* cell = stageMap->GetCell(blockX, y, blockZ);
                        if (cell && cell->type != BlockType::None) {
                            hitGround = true;
                            impactY = static_cast<float>(y) + 0.6f;
                            break;
                        }
                    }
                } else if (it->transform.translate.y <= stormCenter_.y + 0.12f) {
                    hitGround = true;
                }

                if (hitGround) {
                    Vector3 splashPosition = it->transform.translate;
                    splashPosition.y = impactY;
                    EmitStormRainSplash(splashPosition, it->color);
                    it = group.particles.erase(it);
                    continue;
                }
            }

            if (it->type == Particle::Type::Ring) {
                float t = it->lifeTime / it->maxTime;
                float ringScale = 0.25f + 1.8f * t;
                it->transform.scale = { ringScale, ringScale, 1.0f };
            } else if (it->type == Particle::Type::Cylinder) {
                float t = it->lifeTime / it->maxTime;
                float radiusScale = 0.7f + 0.6f * t;
                float heightScale = 0.8f + 0.4f * t;
                it->transform.scale = { radiusScale, heightScale, radiusScale };
            }

            // 当たり判定 (StageMapとの衝突判定)
            if (it->type == Particle::Type::Fall && stageMap) {
                int bx = (int)std::floor(it->transform.translate.x + 0.5f);
                int bz = (int)std::floor(it->transform.translate.z + 0.5f);
                int oldBy = (int)std::floor(oldY + 0.5f);
                int newBy = (int)std::floor(it->transform.translate.y + 0.5f);

                bool hit = false;
                int hitY = newBy;
                // 落下前の位置から落下後の位置までの間のセルを確認（すり抜け防止）
                for (int y = oldBy; y >= newBy; --y) {
                    const MapCell* cell = stageMap->GetCell(bx, y, bz);
                    if (cell != nullptr && cell->type != BlockType::None) {
                        hit = true;
                        hitY = y;
                        break;
                    }
                }

                if (hit) {
                    // 天候ごとに専用の着地表現を生成して元のパーティクルを消滅させる。
                    Vector3 splashPos = it->transform.translate;
                    splashPos.y = (float)hitY + 0.6f;
                    switch (weatherEmitter_.impactEffect) {
                    case WeatherImpactEffect::Rain:
                        // Tempest Stormと同じ控えめな雨粒の飛沫を使う。
                        EmitStormRainSplash(splashPos, it->color);
                        break;
                    case WeatherImpactEffect::Snow:
                        EmitSnowImpact(splashPos, it->color);
                        break;
                    case WeatherImpactEffect::None:
                        break;
                    }
                    it = group.particles.erase(it);
                    continue;
                }
            }

            // フェードアウト
            float normalizedLife = it->lifeTime / it->maxTime;
            float alpha = it->type == Particle::Type::StormCloud
                ? std::sin(normalizedLife * 3.14159265f)
                : 1.0f - normalizedLife;
            it->color.w = it->initialAlpha * alpha;

            ++it;
        }
    }

    // 2. データ書き込み
    planeInstanceCount_ = 0;
    cloudInstanceCount_ = 0;
    ringInstanceCount_ = 0;
    cylinderInstanceCount_ = 0;
    Matrix4x4 cameraMatrix = Math::Inverse(viewMatrix);
    Matrix4x4 billboardMat = Math::MakeIdentity4x4();
    billboardMat.m[0][0] = cameraMatrix.m[0][0]; billboardMat.m[0][1] = cameraMatrix.m[0][1]; billboardMat.m[0][2] = cameraMatrix.m[0][2];
    billboardMat.m[1][0] = cameraMatrix.m[1][0]; billboardMat.m[1][1] = cameraMatrix.m[1][1]; billboardMat.m[1][2] = cameraMatrix.m[1][2];
    billboardMat.m[2][0] = cameraMatrix.m[2][0]; billboardMat.m[2][1] = cameraMatrix.m[2][1]; billboardMat.m[2][2] = cameraMatrix.m[2][2];

    if (perViewData_) {
        perViewData_->viewProjection = Math::Multiply(viewMatrix, projectionMatrix);
        perViewData_->billboardMatrix = billboardMat;
    }

    for (auto& [groupName, group] : particleGroups_) {
        (void)groupName;
        group.planeInstanceStart = planeInstanceCount_;
        group.cloudInstanceStart = cloudInstanceCount_;
        group.ringInstanceStart = ringInstanceCount_;
        group.cylinderInstanceStart = cylinderInstanceCount_;
        group.planeInstanceCount = 0;
        group.cloudInstanceCount = 0;
        group.ringInstanceCount = 0;
        group.cylinderInstanceCount = 0;

        for (const auto& particle : group.particles) {
            uint32_t* indexPtr = &planeInstanceCount_;
            uint32_t* groupCountPtr = &group.planeInstanceCount;
            InstanceData* instancingData = instancingDataMapped_;
            bool useBillboard = true;

            if (particle.type == Particle::Type::StormCloud) {
                indexPtr = &cloudInstanceCount_;
                groupCountPtr = &group.cloudInstanceCount;
                instancingData = cloudInstancingDataMapped_;
            } else if (particle.type == Particle::Type::Ring) {
                indexPtr = &ringInstanceCount_;
                groupCountPtr = &group.ringInstanceCount;
                instancingData = ringInstancingDataMapped_;
            } else if (particle.type == Particle::Type::Cylinder) {
                indexPtr = &cylinderInstanceCount_;
                groupCountPtr = &group.cylinderInstanceCount;
                instancingData = cylinderInstancingDataMapped_;
                useBillboard = false;
            }

            uint32_t& index = *indexPtr;

            if (index >= kMaxParticles) continue;

            Matrix4x4 scaleMat = Math::Matrix4x4MakeScaleMatrix(particle.transform.scale);
            Matrix4x4 rotateMat = Math::MakeRotateZMatrix(particle.transform.rotate.z);
            Matrix4x4 transMat = Math::MakeTranslateMatrix(particle.transform.translate);
            Matrix4x4 worldMat = useBillboard
                ? Math::Multiply(scaleMat, Math::Multiply(rotateMat, Math::Multiply(billboardMat, transMat)))
                : Math::MakeAffineMatrix(particle.transform.scale, particle.transform.rotate, particle.transform.translate);
            Matrix4x4 wvp = Math::Multiply(worldMat, Math::Multiply(viewMatrix, projectionMatrix));

            instancingData[index].WVP = wvp;
            instancingData[index].color = particle.color;
            instancingData[index].shape = particle.type == Particle::Type::StormCloud
                ? 2.0f
                : particle.type == Particle::Type::Lightning ? 1.0f : 0.0f;
            index++;
            (*groupCountPtr)++;
        }
    }
}


void ParticleManager::Draw() {
    if (planeInstanceCount_ == 0 && cloudInstanceCount_ == 0 && ringInstanceCount_ == 0 && cylinderInstanceCount_ == 0) {
        if (drawGPUParticleSphere_) {
            DrawGPUParticles();
        }
        return;
    }

    auto commandList = dxCommon_->GetCommandList();

    // ★ここがNULLだと落ちる。CreatePipelineStateが失敗しているとここでエラーになる
    assert(pipelineState_ != nullptr && "PipelineState not created!");

    commandList->SetPipelineState(pipelineState_.Get());
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (const auto& [groupName, group] : particleGroups_) {
        (void)groupName;
        const uint32_t groupInstanceCount =
            group.planeInstanceCount +
            group.cloudInstanceCount +
            group.ringInstanceCount +
            group.cylinderInstanceCount;
        if (groupInstanceCount == 0) {
            continue;
        }

        const uint32_t textureHandle = group.textureHandle != 0 ? group.textureHandle : textureHandle_;
        auto srvHandle = textureManager_->GetSrvHandleGPU(textureHandle);
        commandList->SetGraphicsRootDescriptorTable(0, srvHandle);

        if (group.cloudInstanceCount > 0) {
            commandList->SetPipelineState(cloudPipelineState_.Get());
            commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
            commandList->IASetVertexBuffers(1, 1, &cloudInstancingBufferView_);
            commandList->DrawInstanced(planeVertexCount_, group.cloudInstanceCount, 0, group.cloudInstanceStart);
        }

        if (group.planeInstanceCount > 0) {
            commandList->SetPipelineState(pipelineState_.Get());
            commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
            commandList->IASetVertexBuffers(1, 1, &instancingBufferView_);
            commandList->DrawInstanced(planeVertexCount_, group.planeInstanceCount, 0, group.planeInstanceStart);
        }

        if (group.ringInstanceCount > 0) {
            commandList->SetPipelineState(primitivePipelineState_.Get());
            commandList->IASetVertexBuffers(0, 1, &ringVertexBufferView_);
            commandList->IASetVertexBuffers(1, 1, &ringInstancingBufferView_);
            commandList->DrawInstanced(ringVertexCount_, group.ringInstanceCount, 0, group.ringInstanceStart);
        }

        if (group.cylinderInstanceCount > 0) {
            commandList->SetPipelineState(primitivePipelineState_.Get());
            commandList->IASetVertexBuffers(0, 1, &cylinderVertexBufferView_);
            commandList->IASetVertexBuffers(1, 1, &cylinderInstancingBufferView_);
            commandList->DrawInstanced(cylinderVertexCount_, group.cylinderInstanceCount, 0, group.cylinderInstanceStart);
        }
    }

    if (drawGPUParticleSphere_) {
        DrawGPUParticles();
    }
}

void ParticleManager::CreateRootSignature() {
    D3D12_DESCRIPTOR_RANGE range = {};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    range.BaseShaderRegister = 0;

    D3D12_ROOT_PARAMETER rootParams[3] = {};
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[0].DescriptorTable.pDescriptorRanges = &range;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    rootParams[1].Descriptor.ShaderRegister = 0;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[2].Descriptor.ShaderRegister = 0;
    rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.ShaderRegister = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = _countof(rootParams);
    desc.pParameters = rootParams;
    desc.NumStaticSamplers = 1;
    desc.pStaticSamplers = &sampler;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> blob, errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errorBlob);
    assert(SUCCEEDED(hr));
    hr = dxCommon_->GetDevice()->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
}

void ParticleManager::CreatePipelineState() {
    // --- ★修正ポイント: InputLayoutをHLSLと完全に一致させる ---
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        // Slot 0: メッシュデータ
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

        // Slot 1: インスタンスデータ (WVP行列を4つのfloat4に分割して定義)
        // HLSL側: INSTANCE_WVP0, 1, 2, 3 に対応
        { "INSTANCE_WVP", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0,  D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_WVP", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_WVP", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_WVP", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },

        // Color
        { "INSTANCE_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_SHAPE", 0, DXGI_FORMAT_R32_FLOAT, 1, 80, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
    };

    // シェーダーコンパイル (パスにhlsl/を追加済み)
    auto vsBlob = dxCommon_->CompileShader(L"Resources/shaders/hlsl/Particle.VS.hlsl", L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(L"Resources/shaders/hlsl/Particle.PS.hlsl", L"ps_6_0");
    auto primitivePsBlob = dxCommon_->CompileShader(L"Resources/shaders/hlsl/ParticlePrimitive.PS.hlsl", L"ps_6_0");
    assert(vsBlob != nullptr && "VS Compile Failed");
    assert(psBlob != nullptr && "PS Compile Failed");
    assert(primitivePsBlob != nullptr && "Primitive PS Compile Failed");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

    // ブレンド設定 (加算合成)
    auto& blend = psoDesc.BlendState.RenderTarget[0];
    blend.BlendEnable = TRUE;
    blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend.DestBlend = D3D12_BLEND_ONE;
    blend.BlendOp = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlendAlpha = D3D12_BLEND_ZERO;
    blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;

    // ★重要: ここで失敗すると pipelineState_ がNULLになり、描画時に落ちる
    HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to create GraphicsPipelineState for Particles!\n");
        assert(false);
    }

    // 黒雲は加算合成だと黒が消えるため、通常のアルファ合成で別描画する。
    blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&cloudPipelineState_));
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to create GraphicsPipelineState for Storm Clouds!\n");
        assert(false);
    }

    // リング・柱・通常パーティクルは従来どおり加算合成。
    blend.DestBlend = D3D12_BLEND_ONE;
    blend.DestBlendAlpha = D3D12_BLEND_ZERO;

    psoDesc.PS = { primitivePsBlob->GetBufferPointer(), primitivePsBlob->GetBufferSize() };
    hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&primitivePipelineState_));
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to create GraphicsPipelineState for Particle Primitives!\n");
        assert(false);
    }

    auto gpuVsBlob = dxCommon_->CompileShader(L"Resources/shaders/hlsl/GPUParticle.VS.hlsl", L"vs_6_0");
    assert(gpuVsBlob != nullptr && "GPU Particle VS Compile Failed");
    psoDesc.InputLayout = { inputLayout, 3 };
    psoDesc.VS = { gpuVsBlob->GetBufferPointer(), gpuVsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

    hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&gpuParticlePipelineState_));
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to create GraphicsPipelineState for GPU Particles!\n");
        assert(false);
    }
}

void ParticleManager::CreateGPUParticleResources() {
    auto device = dxCommon_->GetDevice();

    gpuParticleResource_ = CreateBufferResource(
        device,
        D3D12_HEAP_TYPE_DEFAULT,
        sizeof(GPUParticle) * kMaxGPUParticles,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    gpuParticleResourceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

    gpuParticleFreeListIndexResource_ = CreateBufferResource(
        device,
        D3D12_HEAP_TYPE_DEFAULT,
        sizeof(int32_t),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    gpuParticleFreeListResource_ = CreateBufferResource(
        device,
        D3D12_HEAP_TYPE_DEFAULT,
        sizeof(uint32_t) * kMaxGPUParticles,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    perViewResource_ = CreateBufferResource(
        device,
        D3D12_HEAP_TYPE_UPLOAD,
        AlignConstantBufferSize(sizeof(PerView)),
        D3D12_RESOURCE_STATE_GENERIC_READ);
    perViewResource_->Map(0, nullptr, reinterpret_cast<void**>(&perViewData_));
    perViewData_->viewProjection = Math::MakeIdentity4x4();
    perViewData_->billboardMatrix = Math::MakeIdentity4x4();

    gpuParticleEmitterResource_ = CreateBufferResource(
        device,
        D3D12_HEAP_TYPE_UPLOAD,
        AlignConstantBufferSize(sizeof(GPUParticleEmitterSphere)),
        D3D12_RESOURCE_STATE_GENERIC_READ);
    gpuParticleEmitterResource_->Map(0, nullptr, reinterpret_cast<void**>(&gpuParticleEmitterData_));
    gpuParticleEmitter_.translate = { 0.0f, 0.0f, 0.0f };
    gpuParticleEmitter_.radius = 1.0f;
    gpuParticleEmitter_.count = kGPUParticleEmitCount;
    gpuParticleEmitter_.frequency = kGPUParticleEmitFrequency;
    *gpuParticleEmitterData_ = gpuParticleEmitter_;

    gpuParticlePerFrameResource_ = CreateBufferResource(
        device,
        D3D12_HEAP_TYPE_UPLOAD,
        AlignConstantBufferSize(sizeof(GPUParticlePerFrame)),
        D3D12_RESOURCE_STATE_GENERIC_READ);
    gpuParticlePerFrameResource_->Map(0, nullptr, reinterpret_cast<void**>(&gpuParticlePerFrameData_));
    *gpuParticlePerFrameData_ = gpuParticlePerFrame_;
}

void ParticleManager::CreateGPUParticlePipeline() {
    D3D12_ROOT_PARAMETER rootParams[ToRootIndex(GPUParticleComputeRoot::Count)] = {};

    auto setRootDescriptor = [&](GPUParticleComputeRoot root, D3D12_ROOT_PARAMETER_TYPE type, UINT shaderRegister) {
        D3D12_ROOT_PARAMETER& param = rootParams[ToRootIndex(root)];
        param.ParameterType = type;
        param.Descriptor.ShaderRegister = shaderRegister;
        param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    };

    setRootDescriptor(GPUParticleComputeRoot::ParticlesUAV, D3D12_ROOT_PARAMETER_TYPE_UAV, 0);
    setRootDescriptor(GPUParticleComputeRoot::FreeListIndexUAV, D3D12_ROOT_PARAMETER_TYPE_UAV, 1);
    setRootDescriptor(GPUParticleComputeRoot::EmitterCBV, D3D12_ROOT_PARAMETER_TYPE_CBV, 0);
    setRootDescriptor(GPUParticleComputeRoot::PerFrameCBV, D3D12_ROOT_PARAMETER_TYPE_CBV, 1);
    setRootDescriptor(GPUParticleComputeRoot::FreeListUAV, D3D12_ROOT_PARAMETER_TYPE_UAV, 2);

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = _countof(rootParams);
    rootSignatureDesc.pParameters = rootParams;

    ComPtr<ID3DBlob> blob, errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }

    hr = dxCommon_->GetDevice()->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&initializeParticleRootSignature_));
    assert(SUCCEEDED(hr));

    auto csBlob = dxCommon_->CompileShader(L"Resources/shaders/hlsl/InitializeParticle.CS.hlsl", L"cs_6_0");
    assert(csBlob != nullptr && "InitializeParticle CS Compile Failed");

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = initializeParticleRootSignature_.Get();
    psoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };
    hr = dxCommon_->GetDevice()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&initializeParticlePipelineState_));
    assert(SUCCEEDED(hr));

    auto emitCsBlob = dxCommon_->CompileShader(L"Resources/shaders/hlsl/EmitParticle.CS.hlsl", L"cs_6_0");
    assert(emitCsBlob != nullptr && "EmitParticle CS Compile Failed");

    psoDesc.CS = { emitCsBlob->GetBufferPointer(), emitCsBlob->GetBufferSize() };
    hr = dxCommon_->GetDevice()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&emitParticlePipelineState_));
    assert(SUCCEEDED(hr));

    auto updateCsBlob = dxCommon_->CompileShader(L"Resources/shaders/hlsl/UpdateParticle.CS.hlsl", L"cs_6_0");
    assert(updateCsBlob != nullptr && "UpdateParticle CS Compile Failed");

    psoDesc.CS = { updateCsBlob->GetBufferPointer(), updateCsBlob->GetBufferSize() };
    hr = dxCommon_->GetDevice()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&updateParticlePipelineState_));
    assert(SUCCEEDED(hr));
}

void ParticleManager::InitializeGPUParticles() {
    if (gpuParticlesInitialized_ || !gpuParticleResource_ || !initializeParticleRootSignature_ || !initializeParticlePipelineState_) {
        return;
    }

    auto commandList = dxCommon_->GetCommandList();
    TransitionGPUParticleResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    commandList->SetComputeRootSignature(initializeParticleRootSignature_.Get());
    commandList->SetPipelineState(initializeParticlePipelineState_.Get());
    commandList->SetComputeRootUnorderedAccessView(ToRootIndex(GPUParticleComputeRoot::ParticlesUAV), gpuParticleResource_->GetGPUVirtualAddress());
    commandList->SetComputeRootUnorderedAccessView(ToRootIndex(GPUParticleComputeRoot::FreeListIndexUAV), gpuParticleFreeListIndexResource_->GetGPUVirtualAddress());
    commandList->SetComputeRootUnorderedAccessView(ToRootIndex(GPUParticleComputeRoot::FreeListUAV), gpuParticleFreeListResource_->GetGPUVirtualAddress());
    commandList->Dispatch(1, 1, 1);
    TransitionGPUParticleResource(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    gpuParticlesInitialized_ = true;
}

void ParticleManager::EmitGPUParticles() {
    if (!gpuParticlesInitialized_ || !emitParticlePipelineState_ || !gpuParticleResource_ ||
        !gpuParticleFreeListIndexResource_ || !gpuParticleFreeListResource_ ||
        !gpuParticleEmitterResource_ || !gpuParticlePerFrameResource_) {
        return;
    }

    if (gpuParticleEmitterData_) {
        *gpuParticleEmitterData_ = gpuParticleEmitter_;
    }
    if (gpuParticlePerFrameData_) {
        *gpuParticlePerFrameData_ = gpuParticlePerFrame_;
    }

    if (gpuParticleEmitter_.emit == 0) {
        return;
    }

    auto commandList = dxCommon_->GetCommandList();
    TransitionGPUParticleResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    commandList->SetComputeRootSignature(initializeParticleRootSignature_.Get());
    commandList->SetPipelineState(emitParticlePipelineState_.Get());
    commandList->SetComputeRootUnorderedAccessView(ToRootIndex(GPUParticleComputeRoot::ParticlesUAV), gpuParticleResource_->GetGPUVirtualAddress());
    commandList->SetComputeRootUnorderedAccessView(ToRootIndex(GPUParticleComputeRoot::FreeListIndexUAV), gpuParticleFreeListIndexResource_->GetGPUVirtualAddress());
    commandList->SetComputeRootConstantBufferView(ToRootIndex(GPUParticleComputeRoot::EmitterCBV), gpuParticleEmitterResource_->GetGPUVirtualAddress());
    commandList->SetComputeRootConstantBufferView(ToRootIndex(GPUParticleComputeRoot::PerFrameCBV), gpuParticlePerFrameResource_->GetGPUVirtualAddress());
    commandList->SetComputeRootUnorderedAccessView(ToRootIndex(GPUParticleComputeRoot::FreeListUAV), gpuParticleFreeListResource_->GetGPUVirtualAddress());
    commandList->Dispatch(1, 1, 1);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = gpuParticleResource_.Get();
    commandList->ResourceBarrier(1, &barrier);
}

void ParticleManager::UpdateGPUParticles() {
    if (!gpuParticlesInitialized_ || !updateParticlePipelineState_ || !gpuParticleResource_ ||
        !gpuParticleFreeListIndexResource_ || !gpuParticleFreeListResource_ ||
        !gpuParticleEmitterResource_ || !gpuParticlePerFrameResource_) {
        return;
    }

    if (gpuParticlePerFrameData_) {
        *gpuParticlePerFrameData_ = gpuParticlePerFrame_;
    }

    auto commandList = dxCommon_->GetCommandList();
    TransitionGPUParticleResource(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    commandList->SetComputeRootSignature(initializeParticleRootSignature_.Get());
    commandList->SetPipelineState(updateParticlePipelineState_.Get());
    commandList->SetComputeRootUnorderedAccessView(ToRootIndex(GPUParticleComputeRoot::ParticlesUAV), gpuParticleResource_->GetGPUVirtualAddress());
    commandList->SetComputeRootUnorderedAccessView(ToRootIndex(GPUParticleComputeRoot::FreeListIndexUAV), gpuParticleFreeListIndexResource_->GetGPUVirtualAddress());
    commandList->SetComputeRootConstantBufferView(ToRootIndex(GPUParticleComputeRoot::EmitterCBV), gpuParticleEmitterResource_->GetGPUVirtualAddress());
    commandList->SetComputeRootConstantBufferView(ToRootIndex(GPUParticleComputeRoot::PerFrameCBV), gpuParticlePerFrameResource_->GetGPUVirtualAddress());
    commandList->SetComputeRootUnorderedAccessView(ToRootIndex(GPUParticleComputeRoot::FreeListUAV), gpuParticleFreeListResource_->GetGPUVirtualAddress());
    commandList->Dispatch(1, 1, 1);
}

void ParticleManager::DrawGPUParticles() {
    if (!gpuParticlePipelineState_ || !gpuParticleResource_ || !perViewResource_) {
        return;
    }

    InitializeGPUParticles();
    EmitGPUParticles();
    UpdateGPUParticles();

    auto commandList = dxCommon_->GetCommandList();
    TransitionGPUParticleResource(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    commandList->SetPipelineState(gpuParticlePipelineState_.Get());
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->SetGraphicsRootDescriptorTable(0, textureManager_->GetSrvHandleGPU(textureHandle_));
    commandList->SetGraphicsRootShaderResourceView(1, gpuParticleResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(2, perViewResource_->GetGPUVirtualAddress());
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->DrawInstanced(planeVertexCount_, kMaxGPUParticles, 0, 0);
}

void ParticleManager::TransitionGPUParticleResource(D3D12_RESOURCE_STATES stateAfter) {
    if (!gpuParticleResource_ || gpuParticleResourceState_ == stateAfter) {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = gpuParticleResource_.Get();
    barrier.Transition.StateBefore = gpuParticleResourceState_;
    barrier.Transition.StateAfter = stateAfter;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
    gpuParticleResourceState_ = stateAfter;
}

void ParticleManager::CreateMesh() {
    VertexData vertices[] = {
        {{-0.5f,  0.5f, 0, 1}, {0.0f, 0.0f}, {0, 0, -1}},
        {{ 0.5f,  0.5f, 0, 1}, {1.0f, 0.0f}, {0, 0, -1}},
        {{-0.5f, -0.5f, 0, 1}, {0.0f, 1.0f}, {0, 0, -1}},
        {{-0.5f, -0.5f, 0, 1}, {0.0f, 1.0f}, {0, 0, -1}},
        {{ 0.5f,  0.5f, 0, 1}, {1.0f, 0.0f}, {0, 0, -1}},
        {{ 0.5f, -0.5f, 0, 1}, {1.0f, 1.0f}, {0, 0, -1}},
    };

    planeVertexCount_ = static_cast<uint32_t>(_countof(vertices));
    UINT size = sizeof(vertices);

    D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = size;
    resDesc.Height = 1; resDesc.DepthOrArraySize = 1; resDesc.MipLevels = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; resDesc.SampleDesc.Count = 1;

    HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexBuffer_));
    assert(SUCCEEDED(hr));

    VertexData* data = nullptr;
    vertexBuffer_->Map(0, nullptr, (void**)&data);
    memcpy(data, vertices, size);
    vertexBuffer_->Unmap(0, nullptr);

    vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = size;
    vertexBufferView_.StrideInBytes = sizeof(VertexData);
}

void ParticleManager::CreateRingMesh() {
    constexpr uint32_t kRingDivide = 64;
    constexpr float kOuterRadius = 0.5f;
    constexpr float kInnerRadius = 0.34f;
    constexpr float kPi = 3.14159265f;

    std::vector<VertexData> vertices;
    vertices.reserve(kRingDivide * 6);

    for (uint32_t index = 0; index < kRingDivide; ++index) {
        float t0 = static_cast<float>(index) / static_cast<float>(kRingDivide);
        float t1 = static_cast<float>(index + 1) / static_cast<float>(kRingDivide);
        float angle0 = t0 * 2.0f * kPi;
        float angle1 = t1 * 2.0f * kPi;

        float sin0 = std::sin(angle0);
        float cos0 = std::cos(angle0);
        float sin1 = std::sin(angle1);
        float cos1 = std::cos(angle1);

        VertexData outer0 = { { sin0 * kOuterRadius, cos0 * kOuterRadius, 0.0f, 1.0f }, { 0.5f, 0.5f }, { 0, 0, -1 } };
        VertexData outer1 = { { sin1 * kOuterRadius, cos1 * kOuterRadius, 0.0f, 1.0f }, { 0.5f, 0.5f }, { 0, 0, -1 } };
        VertexData inner0 = { { sin0 * kInnerRadius, cos0 * kInnerRadius, 0.0f, 1.0f }, { 0.5f, 0.5f }, { 0, 0, -1 } };
        VertexData inner1 = { { sin1 * kInnerRadius, cos1 * kInnerRadius, 0.0f, 1.0f }, { 0.5f, 0.5f }, { 0, 0, -1 } };

        vertices.push_back(outer0);
        vertices.push_back(outer1);
        vertices.push_back(inner0);
        vertices.push_back(inner0);
        vertices.push_back(outer1);
        vertices.push_back(inner1);
    }

    ringVertexCount_ = static_cast<uint32_t>(vertices.size());
    UINT size = static_cast<UINT>(sizeof(VertexData) * vertices.size());

    D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = size;
    resDesc.Height = 1; resDesc.DepthOrArraySize = 1; resDesc.MipLevels = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; resDesc.SampleDesc.Count = 1;

    HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&ringVertexBuffer_));
    assert(SUCCEEDED(hr));

    VertexData* data = nullptr;
    ringVertexBuffer_->Map(0, nullptr, (void**)&data);
    memcpy(data, vertices.data(), size);
    ringVertexBuffer_->Unmap(0, nullptr);

    ringVertexBufferView_.BufferLocation = ringVertexBuffer_->GetGPUVirtualAddress();
    ringVertexBufferView_.SizeInBytes = size;
    ringVertexBufferView_.StrideInBytes = sizeof(VertexData);
}

void ParticleManager::CreateCylinderMesh() {
    constexpr uint32_t kCylinderDivide = 64;
    constexpr float kTopRadius = 0.5f;
    constexpr float kBottomRadius = 0.5f;
    constexpr float kHeight = 1.0f;
    constexpr float kPi = 3.14159265f;

    std::vector<VertexData> vertices;
    vertices.reserve(kCylinderDivide * 6);

    for (uint32_t index = 0; index < kCylinderDivide; ++index) {
        float t0 = static_cast<float>(index) / static_cast<float>(kCylinderDivide);
        float t1 = static_cast<float>(index + 1) / static_cast<float>(kCylinderDivide);
        float angle0 = t0 * 2.0f * kPi;
        float angle1 = t1 * 2.0f * kPi;

        float sin0 = std::sin(angle0);
        float cos0 = std::cos(angle0);
        float sin1 = std::sin(angle1);
        float cos1 = std::cos(angle1);

        VertexData top0 = { { sin0 * kTopRadius, kHeight, cos0 * kTopRadius, 1.0f }, { t0, 0.0f }, { sin0, 0, cos0 } };
        VertexData top1 = { { sin1 * kTopRadius, kHeight, cos1 * kTopRadius, 1.0f }, { t1, 0.0f }, { sin1, 0, cos1 } };
        VertexData bottom0 = { { sin0 * kBottomRadius, 0.0f, cos0 * kBottomRadius, 1.0f }, { t0, 1.0f }, { sin0, 0, cos0 } };
        VertexData bottom1 = { { sin1 * kBottomRadius, 0.0f, cos1 * kBottomRadius, 1.0f }, { t1, 1.0f }, { sin1, 0, cos1 } };

        vertices.push_back(top0);
        vertices.push_back(top1);
        vertices.push_back(bottom0);
        vertices.push_back(bottom0);
        vertices.push_back(top1);
        vertices.push_back(bottom1);
    }

    cylinderVertexCount_ = static_cast<uint32_t>(vertices.size());
    UINT size = static_cast<UINT>(sizeof(VertexData) * vertices.size());

    D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = size;
    resDesc.Height = 1; resDesc.DepthOrArraySize = 1; resDesc.MipLevels = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; resDesc.SampleDesc.Count = 1;

    HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&cylinderVertexBuffer_));
    assert(SUCCEEDED(hr));

    VertexData* data = nullptr;
    cylinderVertexBuffer_->Map(0, nullptr, (void**)&data);
    memcpy(data, vertices.data(), size);
    cylinderVertexBuffer_->Unmap(0, nullptr);

    cylinderVertexBufferView_.BufferLocation = cylinderVertexBuffer_->GetGPUVirtualAddress();
    cylinderVertexBufferView_.SizeInBytes = size;
    cylinderVertexBufferView_.StrideInBytes = sizeof(VertexData);
}

void ParticleManager::EmitStormRainSplash(const Vector3& pos, const Vector4& color) {
    constexpr int kDropCount = 3;
    std::uniform_real_distribution<float> angleDistribution(0.0f, 6.2831853f);
    std::uniform_real_distribution<float> speedDistribution(0.010f, 0.024f);
    std::uniform_real_distribution<float> lifeDistribution(0.10f, 0.20f);

    for (int i = 0; i < kDropCount && Particles().size() < kMaxParticles; ++i) {
        const float angle = angleDistribution(engine);
        const float speed = speedDistribution(engine);
        Particle splash;
        splash.type = Particle::Type::Splash;
        splash.transform.translate = pos;
        splash.transform.scale = { 0.025f, 0.055f, 1.0f };
        splash.transform.rotate = { 0.0f, 0.0f, -angle };
        splash.velocity = {
            std::cos(angle) * speed,
            0.018f + speed * 0.45f,
            std::sin(angle) * speed
        };
        splash.color = { color.x * 1.25f, color.y * 1.25f, color.z * 1.25f, (std::max)(color.w, 0.32f) };
        splash.initialAlpha = splash.color.w;
        splash.lifeTime = 0.0f;
        splash.maxTime = lifeDistribution(engine);
        Particles().push_back(splash);
    }
}

void ParticleManager::EmitSnowImpact(const Vector3& pos, const Vector4& color) {
    std::uniform_real_distribution<float> angleDistribution(0.0f, 6.2831853f);
    std::uniform_real_distribution<float> offsetDistribution(-0.07f, 0.07f);
    std::uniform_real_distribution<float> lifeDistribution(0.28f, 0.52f);

    // 着地した雪がふわっと広がる薄い雪煙。
    if (Particles().size() < kMaxParticles) {
        Particle puff;
        puff.type = Particle::Type::Splash;
        puff.transform.translate = { pos.x, pos.y + 0.025f, pos.z };
        puff.transform.scale = { 0.12f, 0.055f, 1.0f };
        puff.transform.rotate = { 0.0f, 0.0f, angleDistribution(engine) };
        puff.velocity = { 0.0f, 0.0f, 0.0f };
        puff.color = { color.x, color.y, color.z, 0.22f };
        puff.initialAlpha = puff.color.w;
        puff.lifeTime = 0.0f;
        puff.maxTime = 0.42f;
        Particles().push_back(puff);
    }

    // 数枚だけ跳ね、すぐに速度を失う小さな雪片。
    constexpr int kFlakeCount = 4;
    for (int i = 0; i < kFlakeCount && Particles().size() < kMaxParticles; ++i) {
        const float angle = angleDistribution(engine);
        Particle flake;
        flake.type = Particle::Type::Splash;
        flake.transform.translate = {
            pos.x + offsetDistribution(engine),
            pos.y + 0.035f,
            pos.z + offsetDistribution(engine)
        };
        flake.transform.scale = { 0.035f, 0.035f, 1.0f };
        flake.transform.rotate = { 0.0f, 0.0f, angle };
        flake.velocity = { std::cos(angle) * 0.006f, 0.006f, std::sin(angle) * 0.006f };
        flake.color = { color.x, color.y, color.z, (std::max)(color.w, 0.5f) };
        flake.initialAlpha = flake.color.w;
        flake.lifeTime = 0.0f;
        flake.maxTime = lifeDistribution(engine);
        Particles().push_back(flake);
    }
}

void ParticleManager::EmitSplash(const Vector3& pos, const Vector4& color) {
    constexpr int kSplashCount = 8;
    constexpr float kPi = 3.14159265f;

    std::uniform_real_distribution<float> distRotate(-kPi, kPi);
    std::uniform_real_distribution<float> distScale(0.4f, 1.5f);
    std::uniform_real_distribution<float> distOffset(-0.08f, 0.08f);
    std::uniform_real_distribution<float> distLife(0.18f, 0.28f);
    std::uniform_real_distribution<float> distSplashSpeed(0.018f, 0.060f);
    std::uniform_real_distribution<float> distUnit(-1.0f, 1.0f);

    if (Particles().size() < kMaxParticles) {
        Particle cylinder;
        cylinder.type = Particle::Type::Cylinder;
        cylinder.transform.translate = { pos.x, pos.y - 0.05f, pos.z };
        cylinder.transform.scale = { 0.7f, 0.8f, 0.7f };
        cylinder.transform.rotate = { 0.0f, distRotate(engine), 0.0f };
        cylinder.velocity = { 0.0f, 0.0f, 0.0f };
        cylinder.color = { color.x * 0.55f, color.y * 0.75f, 1.0f, 0.28f };
        cylinder.initialAlpha = cylinder.color.w;
        cylinder.lifeTime = 0.0f;
        cylinder.maxTime = 0.45f;
        Particles().push_back(cylinder);
    }

    if (Particles().size() < kMaxParticles) {
        Particle ring;
        ring.type = Particle::Type::Ring;
        ring.transform.translate = { pos.x, pos.y + 0.22f, pos.z };
        ring.transform.scale = { 0.25f, 0.25f, 1.0f };
        ring.transform.rotate = { 0.0f, 0.0f, distRotate(engine) };
        ring.velocity = { 0.0f, 0.0f, 0.0f };
        ring.color = { color.x, color.y, color.z, 0.75f };
        ring.initialAlpha = ring.color.w;
        ring.lifeTime = 0.0f;
        ring.maxTime = 0.35f;
        Particles().push_back(ring);
    }

    for (int i = 0; i < kSplashCount; ++i) {
        if (Particles().size() >= kMaxParticles) break;

        const float speed = distSplashSpeed(engine);
        Vector3 direction = {};
        float lengthSquared = 0.0f;
        do {
            direction = { distUnit(engine), distUnit(engine), distUnit(engine) };
            lengthSquared = direction.x * direction.x + direction.y * direction.y + direction.z * direction.z;
        } while (lengthSquared < 0.0001f || lengthSquared > 1.0f);
        const float inverseLength = 1.0f / std::sqrt(lengthSquared);
        direction.x *= inverseLength;
        direction.y *= inverseLength;
        direction.z *= inverseLength;
        Particle p;
        p.type = Particle::Type::Splash;
        p.transform.translate = {
            pos.x + distOffset(engine),
            pos.y + 0.2f + distOffset(engine),
            pos.z + distOffset(engine)
        };
        p.transform.scale = { 0.05f, distScale(engine), 1.0f };
        p.transform.rotate = { distRotate(engine), distRotate(engine), distRotate(engine) };

        p.velocity = {
            direction.x * speed,
            direction.y * speed,
            direction.z * speed
        };
        p.color = color;
        p.initialAlpha = p.color.w;
        p.lifeTime = 0.0f;
        p.maxTime = distLife(engine);

        Particles().push_back(p);
    }
}

void ParticleManager::EmitHitEffect(const Vector3& pos) {
    EmitHitEffect(pos, HitEffectSettings{});
}

void ParticleManager::EmitHitEffect(const Vector3& pos, const HitEffectSettings& settings) {
    constexpr float kPi = 3.14159265f;
    std::uniform_real_distribution<float> distAngle(-0.18f, 0.18f);
    std::uniform_real_distribution<float> distOffset(-0.12f, 0.12f);
    std::uniform_real_distribution<float> distSparkLife(0.13f, 0.28f);
    std::uniform_real_distribution<float> distSparkSpeed(0.035f * settings.sparkSpeed, 0.105f * settings.sparkSpeed);
    std::uniform_real_distribution<float> distSparkTone(0.0f, 1.0f);
    std::uniform_real_distribution<float> distDirection(-1.0f, 1.0f);
    const float size = settings.size < 0.1f ? 0.1f : settings.size;
    const float brightness = settings.brightness < 0.0f ? 0.0f : settings.brightness;
    const float lifeScale = settings.lifeScale < 0.05f ? 0.05f : settings.lifeScale;
    const float alphaScale = brightness < 1.0f ? brightness : 1.0f;
    const float sparkLength = settings.sparkLength < 0.1f ? 0.1f : settings.sparkLength;
    const float scatterRadius = settings.scatterRadius < 0.0f ? 0.0f : settings.scatterRadius;
    const float corePower = settings.corePower < 0.0f ? 0.0f : settings.corePower;
    const float crossPower = settings.crossPower < 0.0f ? 0.0f : settings.crossPower;
    const float pillarPower = settings.pillarPower < 0.0f ? 0.0f : settings.pillarPower;
    const float lightningLength = settings.lightningLength < 0.1f ? 0.1f : settings.lightningLength;
    const float lightningSpread = settings.lightningSpread < 0.0f ? 0.0f : settings.lightningSpread;
    const float lightningPower = settings.lightningPower < 0.0f ? 0.0f : settings.lightningPower;

    auto tintColor = [&](const Vector4& color, float r, float g, float b, float a) {
        return Vector4{
            color.x * r * brightness,
            color.y * g * brightness,
            color.z * b * brightness,
            color.w * a * alphaScale
        };
    };

    auto pushParticle = [&](const Particle& particle) {
        if (Particles().size() < kMaxParticles) {
            Particles().push_back(particle);
        }
    };

    auto pushPlane = [&](const Vector3& translate, const Vector3& scale, float rotateZ, const Vector4& color, float lifeTime, const Vector3& velocity = { 0.0f, 0.0f, 0.0f }, Particle::Type type = Particle::Type::Splash) {
        Particle particle;
        particle.type = type;
        particle.transform.translate = translate;
        particle.transform.scale = scale;
        particle.transform.rotate = { 0.0f, 0.0f, rotateZ };
        particle.velocity = velocity;
        particle.color = color;
        particle.initialAlpha = color.w;
        particle.lifeTime = 0.0f;
        particle.maxTime = lifeTime;
        pushParticle(particle);
    };

    // 斬撃が当たった瞬間の白い芯
    Particle core;
    core.type = Particle::Type::Splash;
    core.transform.translate = pos;
    core.transform.scale = { 1.15f * size * corePower, 1.15f * size * corePower, 1.0f };
    core.transform.rotate = { 0.0f, 0.0f, 0.0f };
    core.velocity = { 0.0f, 0.0f, 0.0f };
    core.color = tintColor(settings.coreColor, 1.35f, 1.25f, 1.15f, 1.0f);
    core.initialAlpha = core.color.w;
    core.lifeTime = 0.0f;
    core.maxTime = 0.13f * lifeScale;
    if (corePower > 0.01f) {
        pushParticle(core);
    }

    // ビームサーベルの軌跡。少しずつ角度と位置をずらして厚みを出す。
    int slashCount = std::clamp(settings.slashCount, 1, 32);
    for (int i = 0; i < slashCount; ++i) {
        float t = slashCount <= 1 ? 0.5f : static_cast<float>(i) / static_cast<float>(slashCount - 1);
        float angle = settings.slashAngle - settings.slashSpread * 0.5f + t * settings.slashSpread;
        if (settings.randomizeDirection) {
            angle += distAngle(engine) * 0.35f;
        }
        float side = t - 0.5f;
        const float positionJitter = settings.randomizePosition ? distOffset(engine) * size * 0.35f : 0.0f;
        const float scaleJitter = settings.randomizeScale ? 0.88f + distSparkTone(engine) * 0.24f : 1.0f;
        const float lifeJitter = settings.randomizeLifetime ? 0.85f + distSparkTone(engine) * 0.30f : 1.0f;
        Vector3 slashPos = {
            pos.x + side * 0.30f * size + positionJitter,
            pos.y + (0.05f + std::sin(t * kPi) * 0.12f) * size + positionJitter,
            pos.z + side * 0.10f * size + positionJitter
        };
        Vector4 slashColor = (!settings.randomizeColor || i % 2 == 0)
            ? tintColor(settings.slashColor, 1.0f, 1.0f, 1.0f, 0.82f)
            : tintColor(settings.slashColor, 0.58f, 0.80f, 1.0f, 0.62f);
        pushPlane(slashPos, { (0.11f + 0.04f * std::sin(t * kPi)) * size * scaleJitter, (2.85f - 0.45f * std::abs(side)) * size * scaleJitter, 1.0f }, angle, slashColor, (0.18f + 0.04f * t) * lifeScale * lifeJitter);
    }

    int lightningCount = std::clamp(settings.lightningCount, 0, 12);
    if (lightningCount > 0 && lightningPower > 0.01f) {
        std::uniform_real_distribution<float> distLightningAngle(-kPi, kPi);
        std::uniform_real_distribution<float> distLightningRadius(0.0f, 1.0f);
        std::uniform_real_distribution<float> distLightningLife(0.07f, 0.16f);

        const int lightningSegments = std::clamp(settings.lightningSegments, 2, 16);
        for (int i = 0; i < lightningCount; ++i) {
            float angle = 0.0f;
            switch (settings.lightningMode) {
            case 1: // Slash Forward
                angle = settings.slashAngle + kPi * 0.5f;
                break;
            case 2: // Slash Axis
                angle = settings.slashAngle + kPi * 0.5f + (i % 2 == 0 ? 0.0f : kPi);
                break;
            case 3: // Custom
                angle = settings.lightningDirection;
                break;
            default: // Radial
                angle = settings.randomizeDirection
                    ? distLightningAngle(engine)
                    : (static_cast<float>(i) / static_cast<float>(lightningCount)) * kPi * 2.0f;
                break;
            }
            if (settings.lightningMode != 0 && settings.randomizeDirection) {
                angle += distDirection(engine) * settings.lightningDirectionSpread;
            }
            const float distance = settings.randomizePosition ? distLightningRadius(engine) * 0.12f * size * lightningSpread : 0.0f;
            Vector3 boltCursor = {
                pos.x + std::cos(angle) * distance,
                pos.y + (settings.randomizePosition ? distOffset(engine) * size : 0.0f),
                pos.z + (settings.randomizePosition ? distOffset(engine) * size * 0.35f : 0.0f)
            };
            std::vector<Vector3> mainBoltPoints;
            std::vector<float> mainBoltAngles;
            mainBoltPoints.reserve(static_cast<size_t>(lightningSegments) + 1);
            mainBoltAngles.reserve(lightningSegments);
            mainBoltPoints.push_back(boltCursor);
            const float boltTone = settings.randomizeColor ? distSparkTone(engine) : 0.35f;
            const float boltWidth = (0.018f + 0.016f * boltTone) * size * lightningPower * settings.lightningWidth;
            const float boltLife = (settings.randomizeLifetime ? distLightningLife(engine) : 0.115f) * lifeScale;
            const float colorVariation = settings.randomizeColor ? 0.82f + boltTone * 0.36f : 1.0f;
            const Vector4 boltColor = tintColor(settings.lightningColor, 1.45f * colorVariation, 1.35f * colorVariation, 1.20f * colorVariation, 0.68f * lightningPower);
            const Vector4 glowColor = tintColor(settings.lightningGlowColor, 1.0f, 1.0f, 1.0f, settings.lightningGlowOpacity * lightningPower);

            for (int segment = 0; segment < lightningSegments; ++segment) {
                const float segmentScale = settings.randomizeScale ? 0.72f + distSparkTone(engine) * 0.56f : 1.0f;
                const float segmentLength = size * lightningLength * segmentScale / static_cast<float>(lightningSegments);
                const float jitter = settings.randomizeDirection ? distAngle(engine) * (1.8f + lightningSpread) : 0.0f;
                angle += jitter;

                const Vector3 direction = { std::cos(angle), std::sin(angle), 0.0f };
                const Vector3 segmentCenter = {
                    boltCursor.x + direction.x * segmentLength * 0.5f,
                    boltCursor.y + direction.y * segmentLength * 0.5f,
                    boltCursor.z
                };
                if (settings.lightningGlowOpacity > 0.001f) {
                    pushPlane(segmentCenter, { boltWidth * settings.lightningGlowWidth, segmentLength * 1.34f, 1.0f }, angle - kPi * 0.5f, glowColor, boltLife * 1.15f, { 0.0f, 0.0f, 0.0f }, Particle::Type::Lightning);
                }
                pushPlane(segmentCenter, { boltWidth, segmentLength * 1.30f, 1.0f }, angle - kPi * 0.5f, boltColor, boltLife, { 0.0f, 0.0f, 0.0f }, Particle::Type::Lightning);

                boltCursor.x += direction.x * segmentLength;
                boltCursor.y += direction.y * segmentLength;
                mainBoltPoints.push_back(boltCursor);
                mainBoltAngles.push_back(angle);
            }

            const int branchCount = std::clamp(settings.lightningBranchCount, 0, 12);
            const int branchSegments = std::clamp(lightningSegments / 2, 2, 4);
            for (int branch = 0; branch < branchCount; ++branch) {
                const int pointIndex = 1 + (branch * (lightningSegments - 1)) / (branchCount > 0 ? branchCount : 1);
                Vector3 branchCursor = mainBoltPoints[pointIndex];
                const float branchSide = branch % 2 == 0 ? 1.0f : -1.0f;
                float branchAngle = mainBoltAngles[pointIndex - 1] + branchSide * (0.35f + settings.lightningBranchSpread * (0.45f + 0.35f * distSparkTone(engine)));
                const float branchTotalLength = size * lightningLength * settings.lightningBranchLength * (0.72f + 0.38f * distSparkTone(engine));
                const float branchWidth = 0.022f * size * lightningPower * settings.lightningWidth * settings.lightningBranchWidth;
                const float branchLife = (settings.randomizeLifetime ? distLightningLife(engine) : 0.10f) * lifeScale * 0.9f;
                const Vector4 branchColor = tintColor(settings.lightningColor, 1.15f, 1.25f, 1.25f, 0.48f * lightningPower);
                const Vector4 branchGlowColor = tintColor(settings.lightningGlowColor, 1.0f, 1.0f, 1.0f, settings.lightningGlowOpacity * lightningPower * 0.65f);

                for (int segment = 0; segment < branchSegments; ++segment) {
                    if (settings.randomizeDirection) {
                        branchAngle += distAngle(engine) * (1.2f + settings.lightningBranchSpread);
                    }
                    const float segmentLength = branchTotalLength / static_cast<float>(branchSegments);
                    const Vector3 direction = { std::cos(branchAngle), std::sin(branchAngle), 0.0f };
                    const Vector3 segmentCenter = {
                        branchCursor.x + direction.x * segmentLength * 0.5f,
                        branchCursor.y + direction.y * segmentLength * 0.5f,
                        branchCursor.z
                    };
                    if (settings.lightningGlowOpacity > 0.001f) {
                        pushPlane(segmentCenter, { branchWidth * settings.lightningGlowWidth, segmentLength * 1.34f, 1.0f }, branchAngle - kPi * 0.5f, branchGlowColor, branchLife * 1.15f, { 0.0f, 0.0f, 0.0f }, Particle::Type::Lightning);
                    }
                    pushPlane(segmentCenter, { branchWidth, segmentLength * 1.30f, 1.0f }, branchAngle - kPi * 0.5f, branchColor, branchLife, { 0.0f, 0.0f, 0.0f }, Particle::Type::Lightning);
                    branchCursor.x += direction.x * segmentLength;
                    branchCursor.y += direction.y * segmentLength;
                }
            }
        }
    }

    // 交差する鋭い閃光。画面で一目わかる派手さ用。
    if (crossPower > 0.01f) {
        pushPlane({ pos.x, pos.y + 0.02f * size, pos.z }, { 0.055f * size, 3.45f * size * crossPower, 1.0f }, settings.slashAngle - 0.72f, tintColor(settings.crossColor, 1.35f, 1.2f, 1.0f, 0.95f), 0.16f * lifeScale);
        pushPlane({ pos.x, pos.y + 0.02f * size, pos.z }, { 0.050f * size, 2.55f * size * crossPower, 1.0f }, settings.slashAngle + 0.78f, tintColor(settings.crossColor, 0.76f, 0.96f, 1.0f, 0.70f), 0.14f * lifeScale);
        pushPlane({ pos.x, pos.y + 0.03f * size, pos.z }, { 0.30f * size, 1.55f * size * crossPower, 1.0f }, settings.slashAngle + 0.04f, tintColor(settings.crossColor, 1.15f, 1.05f, 1.0f, 0.52f), 0.20f * lifeScale);
    }

    for (int ringIndex = 0; ringIndex < 3; ++ringIndex) {
        Particle ring;
        ring.type = Particle::Type::Ring;
        ring.transform.translate = { pos.x, pos.y + 0.04f * ringIndex, pos.z };
        ring.transform.scale = { 0.18f * size * settings.ringPower, 0.18f * size * settings.ringPower, 1.0f };
        ring.transform.rotate = { 0.0f, 0.0f, -0.35f + ringIndex * 0.38f };
        ring.velocity = { 0.0f, 0.0f, 0.0f };
        ring.color = ringIndex == 0 ? tintColor(settings.ringColor, 1.4f, 1.25f, 1.1f, 0.72f)
            : ringIndex == 1 ? tintColor(settings.ringColor, 0.65f, 0.90f, 1.0f, 0.55f)
            : tintColor(settings.ringColor, 1.0f, 1.0f, 1.0f, 0.42f);
        ring.initialAlpha = ring.color.w;
        ring.lifeTime = 0.0f;
        ring.maxTime = (0.20f + 0.08f * ringIndex) * lifeScale;
        pushParticle(ring);
    }

    Particle pillar;
    pillar.type = Particle::Type::Cylinder;
    pillar.transform.translate = { pos.x, pos.y - 0.16f * size, pos.z };
    pillar.transform.scale = { 0.42f * size, 1.0f * size * pillarPower, 0.42f * size };
    pillar.transform.rotate = { 0.0f, 0.0f, 0.0f };
    pillar.velocity = { 0.0f, 0.0f, 0.0f };
    pillar.color = tintColor(settings.pillarColor, 0.45f, 0.85f, 1.0f, 0.34f * pillarPower);
    pillar.initialAlpha = pillar.color.w;
    pillar.lifeTime = 0.0f;
    pillar.maxTime = 0.22f * lifeScale;
    if (pillarPower > 0.01f) {
        pushParticle(pillar);
    }

    int sparkCount = std::clamp(settings.sparkCount, 0, 160);
    for (int i = 0; i < sparkCount; ++i) {
        float baseAngle = (static_cast<float>(i) / static_cast<float>(sparkCount)) * kPi * 2.0f;
        float angle = baseAngle + (settings.randomizeDirection ? distAngle(engine) : 0.0f);
        float tone = settings.randomizeColor || settings.randomizeScale ? distSparkTone(engine) : 0.5f;
        float speed = settings.randomizeScale ? distSparkSpeed(engine) : 0.07f * settings.sparkSpeed;
        const float offsetX = settings.randomizePosition ? distOffset(engine) * size * scatterRadius : 0.0f;
        const float offsetY = settings.randomizePosition ? distOffset(engine) * size * scatterRadius : 0.0f;
        const float offsetZ = settings.randomizePosition ? distOffset(engine) * size * scatterRadius : 0.0f;
        Vector3 sparkPos = {
            pos.x + offsetX,
            pos.y + offsetY,
            pos.z + offsetZ
        };
        Vector3 sparkVelocity = {
            std::cos(angle) * speed,
            std::sin(angle) * speed * 0.35f,
            std::sin(angle * 1.7f) * speed
        };
        Vector4 sparkColor = (!settings.randomizeColor || tone < settings.blueRatio)
            ? tintColor(settings.sparkColor, 1.05f, 1.0f, 1.0f, 0.88f)
            : tintColor(settings.sparkSecondaryColor, 1.0f, 1.0f, 1.0f, 0.88f);
        const float sparkLife = (settings.randomizeLifetime ? distSparkLife(engine) : 0.205f) * lifeScale;
        pushPlane(sparkPos, { (0.025f + 0.02f * tone) * size, (0.62f + 0.72f * tone) * size * sparkLength, 1.0f }, angle, sparkColor, sparkLife, sparkVelocity);
    }
}

void ParticleManager::Emit(const Vector3& pos, uint32_t count) {
    Emit(kDefaultParticleGroupName, pos, count);
}

void ParticleManager::Emit(const std::string& groupName, const Vector3& pos, uint32_t count) {
    ParticleGroup& group = particleGroups_.contains(groupName)
        ? particleGroups_.at(groupName)
        : GetDefaultParticleGroup();

    std::uniform_real_distribution<float> distRotate(-3.14159265f, 3.14159265f);
    std::uniform_real_distribution<float> distScale(0.4f, 1.5f);
    std::uniform_real_distribution<float> distColor(0.7f, 1.0f);
    std::uniform_real_distribution<float> distTime(0.18f, 0.35f);
    std::uniform_real_distribution<float> distOffset(-0.2f, 0.2f);

    if (group.particles.size() < kMaxParticles) {
        Particle cylinder;
        cylinder.type = Particle::Type::Cylinder;
        cylinder.transform.scale = { 0.7f, 0.8f, 0.7f };
        cylinder.transform.rotate = { 0.0f, distRotate(engine), 0.0f };
        cylinder.transform.translate = { pos.x, pos.y - 0.05f, pos.z };
        cylinder.velocity = { 0.0f, 0.0f, 0.0f };
        cylinder.color = { 0.35f, 0.55f, 1.0f, 0.28f };
        cylinder.initialAlpha = cylinder.color.w;
        cylinder.lifeTime = 0.0f;
        cylinder.maxTime = 0.45f;
        group.particles.push_back(cylinder);
    }

    if (group.particles.size() < kMaxParticles) {
        Particle ring;
        ring.type = Particle::Type::Ring;
        ring.transform.scale = { 0.25f, 0.25f, 1.0f };
        ring.transform.rotate = { 0.0f, 0.0f, distRotate(engine) };
        ring.transform.translate = pos;
        ring.velocity = { 0.0f, 0.0f, 0.0f };
        ring.color = { 1.0f, 0.9f, 0.45f, 0.75f };
        ring.initialAlpha = ring.color.w;
        ring.lifeTime = 0.0f;
        ring.maxTime = 0.35f;
        group.particles.push_back(ring);
    }

    for (uint32_t i = 0; i < count; ++i) {
        if (group.particles.size() >= kMaxParticles) return;

        Particle p;
        p.type = Particle::Type::Splash;
        p.transform.scale = { 0.05f, distScale(engine), 1.0f };
        p.transform.rotate = { 0.0f, 0.0f, distRotate(engine) };
        p.transform.translate = {
            pos.x + distOffset(engine),
            pos.y + distOffset(engine),
            pos.z + distOffset(engine)
        };
        p.velocity = { 0.0f, 0.0f, 0.0f };
        p.color = { distColor(engine), distColor(engine), distColor(engine), 1.0f };
        p.initialAlpha = p.color.w;
        p.lifeTime = 0.0f;
        p.maxTime = distTime(engine);
        group.particles.push_back(p);
    }
}

void ParticleManager::CreateParticleGroup(const std::string& groupName, uint32_t textureHandle) {
    if (groupName.empty()) {
        return;
    }

    ParticleGroup& group = particleGroups_[groupName];
    group.textureHandle = textureHandle != 0 ? textureHandle : textureHandle_;
}

void ParticleManager::SetTexture(const std::string& groupName, uint32_t textureHandle) {
    if (groupName.empty()) {
        return;
    }

    ParticleGroup& group = particleGroups_[groupName];
    group.textureHandle = textureHandle;
}

ParticleManager::ParticleGroup* ParticleManager::FindParticleGroup(const std::string& groupName) {
    auto it = particleGroups_.find(groupName);
    return it == particleGroups_.end() ? nullptr : &it->second;
}

const ParticleManager::ParticleGroup* ParticleManager::FindParticleGroup(const std::string& groupName) const {
    auto it = particleGroups_.find(groupName);
    return it == particleGroups_.end() ? nullptr : &it->second;
}

ParticleManager::ParticleGroup& ParticleManager::GetDefaultParticleGroup() {
    ParticleGroup& group = particleGroups_[kDefaultParticleGroupName];
    if (group.textureHandle == 0) {
        group.textureHandle = textureHandle_;
    }
    return group;
}

const ParticleManager::ParticleGroup& ParticleManager::GetDefaultParticleGroup() const {
    auto it = particleGroups_.find(kDefaultParticleGroupName);
    assert(it != particleGroups_.end());
    return it->second;
}

std::list<Particle>& ParticleManager::Particles() {
    return GetDefaultParticleGroup().particles;
}

const std::list<Particle>& ParticleManager::Particles() const {
    return GetDefaultParticleGroup().particles;
}
