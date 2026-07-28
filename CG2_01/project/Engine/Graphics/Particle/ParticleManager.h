#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <list>
#include <string>
#include <unordered_map>
#include "MyMath.h"
#include "DirectXCommon.h"
#include "TextureManager.h"

class StageMap;

// パーティクル1つのデータ構造
struct Particle {
    enum class Type {
        Fall,
        Splash,
        Ring,
        Cylinder,
        Lightning,
        StormCloud,
        StormRain,
        StormWind
    };
    Type type = Type::Fall;
    Transform transform; // 位置、回転、スケール
    Vector3 velocity;    // 速度
    Vector4 color;       // 色
    float initialAlpha = 1.0f;
    float lifeTime;      // 時間(現在)
    float maxTime;       // (最大)
};

// マネージャークラス
class ParticleManager {
public: // サブクラスなど
    // 定数：最大パーティクル数
    static const uint32_t kMaxParticles = 4096; // 天候のために増やす

    // インスタンシング用データ構造（シェーダーに送る）
    struct InstanceData {
        Matrix4x4 WVP;
        Vector4 color;
        float shape = 0.0f;
    };

    // 頂点データ構造（板ポリゴン用）
    struct VertexData {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    struct GPUParticle {
        Vector3 translate;
        Vector3 velocity;
        Vector3 scale;
        float lifeTime;
        float currentTime;
        Vector4 color;
    };

    struct PerView {
        Matrix4x4 viewProjection;
        Matrix4x4 billboardMatrix;
    };

    struct GPUParticleEmitterSphere {
        Vector3 translate = { 0.0f, 0.0f, 0.0f };
        float radius = 1.0f;
        uint32_t count = 10;       // 1回の射出で生成するParticle数
        float frequency = 0.5f;    // 射出間隔（秒）
        float frequencyTime = 0.0f;
        uint32_t emit = 0;         // CPU側で判定した「このフレーム射出するか」
    };

    struct GPUParticlePerFrame {
        float time = 0.0f;
        float deltaTime = 0.0f;
    };

    enum class WeatherImpactEffect {
        None,
        Rain,
        Snow,
    };

    struct WeatherEmitter {
        bool active = false;
        Vector3 center = {0,0,0};
        Vector3 size = {40, 20, 40};       // 発生範囲
        float emitRate = 100.0f;           // 1秒あたりの発生数
        float emitTimer = 0.0f;
        Vector3 velocity = {0, -5.0f, 0};  // 基本落下速度
        Vector3 velocityRandom = {1, 0.5f, 1}; // 速度のばらつき
        Vector3 particleSize = {0.2f, 0.2f, 0.2f};
        float particleLife = 4.0f;
        Vector4 color = {1,1,1,1};
        WeatherImpactEffect impactEffect = WeatherImpactEffect::None;
    };

    struct AmbientCloudEmitter {
        bool active = true;
        Vector3 center = { 0.0f, 0.0f, 0.0f };
        float minimumHeight = 10.0f;
        float areaX = 35.0f;
        float areaZ = 35.0f;
        float emitRate = 0.45f;
        float life = 24.0f;
        float size = 1.4f;
        float speed = 0.012f;
        Vector4 color = { 0.72f, 0.78f, 0.90f, 0.22f };
        float emitTimer = 0.0f;
    };

    struct ParticleGroup {
        uint32_t textureHandle = 0;
        std::list<Particle> particles;
        uint32_t planeInstanceStart = 0;
        uint32_t planeInstanceCount = 0;
        uint32_t cloudInstanceStart = 0;
        uint32_t cloudInstanceCount = 0;
        uint32_t ringInstanceStart = 0;
        uint32_t ringInstanceCount = 0;
        uint32_t cylinderInstanceStart = 0;
        uint32_t cylinderInstanceCount = 0;
    };

    struct HitEffectSettings {
        float size = 1.0f;
        float brightness = 1.0f;
        float lifeScale = 1.0f;
        float slashAngle = -0.2f;
        float slashSpread = 1.48f;
        int slashCount = 11;
        int sparkCount = 48;
        float sparkSpeed = 1.0f;
        float sparkLength = 1.0f;
        float scatterRadius = 1.0f;
        float blueRatio = 0.62f;
        float ringPower = 1.0f;
        float corePower = 1.0f;
        float crossPower = 1.0f;
        float pillarPower = 1.0f;
        int lightningCount = 0;
        int lightningSegments = 4;
        float lightningLength = 1.0f;
        float lightningSpread = 1.0f;
        float lightningPower = 1.0f;
        float lightningWidth = 1.0f;
        float lightningGlowWidth = 3.0f;
        float lightningGlowOpacity = 0.22f;
        int lightningBranchCount = 3;
        float lightningBranchLength = 0.4f;
        float lightningBranchSpread = 0.8f;
        float lightningBranchWidth = 0.42f;
        int lightningMode = 0;
        float lightningDirection = 0.0f;
        float lightningDirectionSpread = 0.35f;
        bool randomizePosition = true;
        bool randomizeDirection = true;
        bool randomizeAngle = false;
        float angleRandomRange = 0.5f;
        bool randomizeScale = true;
        bool randomizeLifetime = true;
        bool randomizeColor = true;
        Vector4 coreColor = { 0.78f, 0.92f, 1.0f, 1.0f };
        Vector4 slashColor = { 0.55f, 0.85f, 1.0f, 1.0f };
        Vector4 sparkColor = { 0.55f, 0.85f, 1.0f, 1.0f };
        Vector4 sparkSecondaryColor = { 1.0f, 0.55f, 0.18f, 1.0f };
        Vector4 ringColor = { 0.42f, 0.78f, 1.0f, 1.0f };
        Vector4 crossColor = { 0.72f, 0.90f, 1.0f, 1.0f };
        Vector4 pillarColor = { 0.35f, 0.68f, 1.0f, 1.0f };
        Vector4 lightningColor = { 0.48f, 0.88f, 1.0f, 1.0f };
        Vector4 lightningGlowColor = { 0.20f, 0.34f, 1.0f, 1.0f };
        // Legacy palette fields retained for loading older preset files.
        Vector4 coolColor = { 0.55f, 0.85f, 1.0f, 1.0f };
        Vector4 warmColor = { 1.0f, 0.55f, 0.18f, 1.0f };
    };

    struct StormEffectSettings {
        float cloudAreaX = 6.5f;
        float cloudAreaZ = 4.5f;
        float cloudHeight = 4.2f;
        float cloudEmitRate = 18.0f;
        float cloudLife = 8.0f;
        float cloudSize = 2.6f;
        Vector4 cloudColor = { 0.018f, 0.024f, 0.060f, 0.38f };
        bool randomizeCloudPosition = true;
        bool randomizeCloudSize = true;

        float rainAreaX = 6.5f;
        float rainAreaZ = 4.5f;
        float rainEmitRate = 72.0f;
        float rainSpeed = 1.0f;
        float rainLength = 1.0f;
        Vector4 rainColor = { 0.32f, 0.52f, 0.82f, 0.48f };
        bool randomizeRainPosition = true;
        bool randomizeRainSpeed = true;

        float windEmitRate = 12.5f;
        float windSpeed = 1.0f;
        float windLength = 1.0f;
        Vector4 windColor = { 0.46f, 0.68f, 0.90f, 0.25f };

        float lightningIntervalMin = 0.65f;
        float lightningIntervalMax = 2.10f;
        float lightningFrequency = 1.0f;
        float lightningAreaX = 3.3f;
        float lightningAreaZ = 1.8f;
        float lightningStrikeSize = 1.0f;
        float lightningSizeRandomMin = 0.38f;
        float lightningSizeRandomMax = 1.45f;
        float lightningLengthRandomMin = 0.55f;
        float lightningLengthRandomMax = 1.70f;
        float lightningWidthRandomMin = 0.35f;
        float lightningWidthRandomMax = 1.85f;
        float lightningCoreRandomMin = 0.30f;
        float lightningCoreRandomMax = 1.80f;
        int lightningSimultaneousCount = 1;
        float lightningSimultaneousSpread = 2.0f;
        int lightningBurstCount = 2;
        float lightningBurstInterval = 0.12f;
        int lightningCount = 3;
        int lightningSegments = 8;
        float lightningLength = 5.2f;
        float lightningSpread = 0.7f;
        float lightningPower = 1.6f;
        float lightningWidth = 1.35f;
        float lightningGlowWidth = 5.0f;
        float lightningGlowOpacity = 0.22f;
        int lightningBranchCount = 5;
        float lightningBranchLength = 0.42f;
        float lightningBranchSpread = 0.85f;
        float lightningBranchWidth = 0.45f;
        Vector4 lightningColor = { 0.62f, 0.82f, 1.0f, 1.0f };
        Vector4 lightningGlowColor = { 0.18f, 0.30f, 1.0f, 1.0f };
        float pointLightPower = 11.0f;
        bool randomizeLightningPosition = true;
        bool randomizeLightningInterval = true;
        bool randomizeLightningDirection = true;
        bool randomizeLightningSize = true;
        bool randomizeLightningBurstCount = true;
        bool randomizeLightningBranchCount = true;
    };

public: // メンバ関数
    // 初期化
    void Initialize(DirectXCommon* dxCommon, TextureManager* textureManager);

    // 更新
    void Update(float deltaTime, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix, const Vector3& playerPos, StageMap* stageMap = nullptr);

    // 描画
    void Draw();

    // パーティクル発生（エミッター）
    // pos: 発生位置, count: 発生数
    void Emit(const Vector3& pos, uint32_t count);
    void Emit(const std::string& groupName, const Vector3& pos, uint32_t count);

    // 飛沫を生成する（ブロック衝突時など）
    void EmitSplash(const Vector3& pos, const Vector4& color);

    // 評価課題用: ヒット時のフラッシュ・スパーク・衝撃波をまとめて生成する
    void EmitHitEffect(const Vector3& pos);
    void EmitHitEffect(const Vector3& pos, const HitEffectSettings& settings);

    void SetStormActive(bool active, const Vector3& center = { 0.0f, 0.0f, 0.0f });
    void SetStormCenter(const Vector3& center) { stormCenter_ = center; }
    void SetStormMinimumCloudHeight(float height) { stormMinimumCloudHeight_ = height; }
    bool IsStormActive() const { return stormActive_; }
    bool ConsumeStormLightningFlash();
    const Vector3& GetStormLightningPosition() const { return stormLightningPosition_; }
    float GetStormLightningPowerScale() const { return stormLightningPowerScale_; }
    StormEffectSettings& GetStormSettings() { return stormSettings_; }
    const StormEffectSettings& GetStormSettings() const { return stormSettings_; }

    // テクスチャ設定
    void SetTexture(uint32_t textureHandle) {
        textureHandle_ = textureHandle;
        GetDefaultParticleGroup().textureHandle = textureHandle;
    }
    void SetTexture(const std::string& groupName, uint32_t textureHandle);
    void CreateParticleGroup(const std::string& groupName, uint32_t textureHandle);
    ParticleGroup* FindParticleGroup(const std::string& groupName);
    const ParticleGroup* FindParticleGroup(const std::string& groupName) const;

    void SetDrawGPUParticleSphere(bool draw) { drawGPUParticleSphere_ = draw; }
    bool GetDrawGPUParticleSphere() const { return drawGPUParticleSphere_; }

    // 天候エミッターの取得・設定
    WeatherEmitter& GetWeatherEmitter() { return weatherEmitter_; }
    AmbientCloudEmitter& GetAmbientCloudEmitter() { return ambientCloudEmitter_; }

    void ClearParticles() {
        for (auto& [name, group] : particleGroups_) {
            group.particles.clear();
        }
        planeInstanceCount_ = 0;
        cloudInstanceCount_ = 0;
        ringInstanceCount_ = 0;
        cylinderInstanceCount_ = 0;
    }

private: // 内部処理
    void CreateRootSignature();
    void CreatePipelineState();
    void CreateMesh(); // 板ポリゴンの作成
    void CreateRingMesh();
    void CreateCylinderMesh();
    void EmitStormRainSplash(const Vector3& pos, const Vector4& color);
    void EmitSnowImpact(const Vector3& pos, const Vector4& color);
    void CreateGPUParticleResources();
    void CreateGPUParticlePipeline();
    void InitializeGPUParticles();
    void EmitGPUParticles();
    void UpdateGPUParticles();
    void DrawGPUParticles();
    void TransitionGPUParticleResource(D3D12_RESOURCE_STATES stateAfter);
    ParticleGroup& GetDefaultParticleGroup();
    const ParticleGroup& GetDefaultParticleGroup() const;
    std::list<Particle>& Particles();
    const std::list<Particle>& Particles() const;

private: // メンバ変数
    DirectXCommon* dxCommon_ = nullptr;
    TextureManager* textureManager_ = nullptr;

    // DirectXリソース
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> primitivePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> cloudPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> gpuParticlePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> initializeParticleRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> initializeParticlePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> emitParticlePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> updateParticlePipelineState_;

    // モデルデータ（板ポリ）
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    uint32_t planeVertexCount_ = 0;

    // モデルデータ（Ring）
    Microsoft::WRL::ComPtr<ID3D12Resource> ringVertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW ringVertexBufferView_{};
    uint32_t ringVertexCount_ = 0;

    // モデルデータ（Cylinder）
    Microsoft::WRL::ComPtr<ID3D12Resource> cylinderVertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW cylinderVertexBufferView_{};
    uint32_t cylinderVertexCount_ = 0;

    // GPU Particle
    static const uint32_t kMaxGPUParticles = 1024;
    Microsoft::WRL::ComPtr<ID3D12Resource> gpuParticleResource_;
    // FreeListは「空いているParticleのIndex」をGPU側で使い回すための仕組み。
    // Index本体とは別に、末尾位置を指す1要素のバッファを持つ。
    Microsoft::WRL::ComPtr<ID3D12Resource> gpuParticleFreeListIndexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> gpuParticleFreeListResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> gpuParticleEmitterResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> gpuParticlePerFrameResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> perViewResource_;
    PerView* perViewData_ = nullptr;
    GPUParticleEmitterSphere* gpuParticleEmitterData_ = nullptr;
    GPUParticlePerFrame* gpuParticlePerFrameData_ = nullptr;
    GPUParticleEmitterSphere gpuParticleEmitter_{};
    GPUParticlePerFrame gpuParticlePerFrame_{};
    D3D12_RESOURCE_STATES gpuParticleResourceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    bool gpuParticlesInitialized_ = false;
    bool drawGPUParticleSphere_ = true;

    // インスタンシング用データ
    Microsoft::WRL::ComPtr<ID3D12Resource> instancingBuffer_;
    D3D12_VERTEX_BUFFER_VIEW instancingBufferView_{};
    InstanceData* instancingDataMapped_ = nullptr;
    uint32_t planeInstanceCount_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> cloudInstancingBuffer_;
    D3D12_VERTEX_BUFFER_VIEW cloudInstancingBufferView_{};
    InstanceData* cloudInstancingDataMapped_ = nullptr;
    uint32_t cloudInstanceCount_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> ringInstancingBuffer_;
    D3D12_VERTEX_BUFFER_VIEW ringInstancingBufferView_{};
    InstanceData* ringInstancingDataMapped_ = nullptr;
    uint32_t ringInstanceCount_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> cylinderInstancingBuffer_;
    D3D12_VERTEX_BUFFER_VIEW cylinderInstancingBufferView_{};
    InstanceData* cylinderInstancingDataMapped_ = nullptr;
    uint32_t cylinderInstanceCount_ = 0;

    static constexpr const char* kDefaultParticleGroupName = "default";

    // テクスチャハンドル
    uint32_t textureHandle_ = 0;

    // パーティクルグループ。課題要件に合わせ、名前ごとにテクスチャとパーティクル列を管理する。
    std::unordered_map<std::string, ParticleGroup> particleGroups_;

    // 天候用エミッター
    WeatherEmitter weatherEmitter_;
    AmbientCloudEmitter ambientCloudEmitter_;

    bool stormActive_ = false;
    bool stormLightningFlash_ = false;
    Vector3 stormCenter_ = { 0.0f, 0.0f, 0.0f };
    float stormMinimumCloudHeight_ = 8.0f;
    Vector3 stormLightningPosition_ = { 0.0f, 0.5f, 0.0f };
    float stormLightningPowerScale_ = 1.0f;
    float stormCloudEmitTimer_ = 0.0f;
    float stormRainEmitTimer_ = 0.0f;
    float stormWindEmitTimer_ = 0.0f;
    float stormLightningTimer_ = 1.0f;
    float stormLightningBurstTimer_ = 0.0f;
    int stormLightningBurstsRemaining_ = 0;
    StormEffectSettings stormSettings_{};
};

