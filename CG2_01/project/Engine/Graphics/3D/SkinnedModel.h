#pragma once
#include "Model.h"
#include "MyMath.h"
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

// スキニング用頂点データ (GPUへ転送する構造体)
struct SkinnedVertexData {
    Vector4 position; // ローカル座標 (xyz + w=1.0)
    Vector2 texcoord; // UV 座標 (0.0?1.0)
    Vector3 normal;   // 法線ベクトル (正規化済み)
    int32_t jointIndices[4]; // 影響を受けるボーンのインデックス
    float   weights[4];      // ウェイト
};

struct VertexInfluence {
    float weights[4];
    int32_t jointIndices[4];
};

// ボーン (ジョイント) 構造体
struct Joint {
    std::string name;
    Vector3 translation; // 親からの相対位置 (初期位置)
    Vector3 rotation;    // 回転 (オイラー角 ラジアン)
    Vector3 scale;       // スケール
    Quaternion rotationQuat = { 0.0f, 0.0f, 0.0f, 1.0f }; // クォータニオン回転
    bool isQuaternion = false; // クォータニオンでの更新を行うか

    Vector3 restTranslation = { 0.0f, 0.0f, 0.0f };
    Vector3 restRotation = { 0.0f, 0.0f, 0.0f };
    Vector3 restScale = { 1.0f, 1.0f, 1.0f };
    Quaternion restRotationQuat = { 0.0f, 0.0f, 0.0f, 1.0f };
    bool restIsQuaternion = false;

    Matrix4x4 localMatrix;
    Matrix4x4 globalMatrix;
    Matrix4x4 offsetMatrix; // Bind Poseのグローバル行列の逆行列
    Matrix4x4 externalParentMatrix; // skin外の親Node変換

    int parentIndex = -1;
    std::vector<int> childIndices;
};

// キーフレームアニメーション用構造体
struct JointKeyframe {
    float time = 0.0f; // タイムスタンプ (秒)
    Vector3 translation = { 0.0f, 0.0f, 0.0f };
    Vector3 rotation = { 0.0f, 0.0f, 0.0f }; // オイラー角 (ラジアン)
    Vector3 scale = { 1.0f, 1.0f, 1.0f };
    Quaternion rotationQuat = { 0.0f, 0.0f, 0.0f, 1.0f }; // クォータニオン回転
    bool isQuaternion = false; // クォータニオンでの補間を行うか
};

struct JointAnimation {
    std::string name;
    std::vector<JointKeyframe> keyframes;
};

struct MotionData {
    std::string name;          // アニメーション名 (glTFから読み込む)
    float duration = 2.0f; // アニメーションの総時間 (秒)
    std::vector<JointAnimation> jointAnimations;
};

struct WellForGPU {
    Matrix4x4 skeletonSpaceMatrix;
    Matrix4x4 skeletonSpaceInverseTransposeMatrix;
};

struct SkinningInformationForGPU {
    uint32_t numVertices = 0;
};

// スキニング可能な人型モデルクラス
class SkinnedModel {
public:
    SkinnedModel() = default;
    ~SkinnedModel();

    // 初期化 (人型モデルの生成とバッファ構築)
    void Initialize(DirectXCommon* dxCommon, TextureManager* textureManager);

    // glTFファイルから初期化
    void InitializeFromGltf(DirectXCommon* dxCommon, const std::string& filePath, TextureManager* textureManager);
    void CreateBuffers(DirectXCommon* dxCommon);

    // ゲッター群
    const std::vector<Joint>& GetJoints() const { return joints_; }
    int GetRootJointIndex() const { return rootJointIndex_; }
    const std::unordered_map<std::string, int>& GetJointIndexMap() const { return jointIndexMap_; }
    uint32_t GetTextureHandle() const { return textureHandle_; }

    // D3D12 描画用バッファビュー
    const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return vertexBufferView_; }
    const D3D12_VERTEX_BUFFER_VIEW& GetInfluenceBufferView() const { return influenceBufferView_; }
    const D3D12_VERTEX_BUFFER_VIEW& GetSkinnedVertexBufferView() const { return skinnedVertexBufferView_; }
    ID3D12Resource* GetJointBuffer() const { return jointBuffer_.Get(); }
    size_t GetVertexCount() const { return skinnedVertices_.size(); }

    // アニメーション/ポーズの更新とスキニング計算
    void Update(DirectXCommon* dxCommon);
    void DispatchSkinning(DirectXCommon* dxCommon);

    // 描画
    void Draw(ID3D12GraphicsCommandList* commandList);

    // ボーンの取得・設定
    std::vector<Joint>& GetJoints() { return joints_; }
    
    // 描画用のModelポインタを取得
    Model* GetModel() const { return model_.get(); }

    // ポーズをデフォルトに戻す
    void ResetPose();
    bool SaveMotion(const std::string& filePath);
    bool LoadMotion(const std::string& filePath);
    void ApplyMotion(float time);
    void ApplyMotionBlend(int fromMotionIndex, int toMotionIndex, float time, float blendRate);
    const std::string& GetName() const { return name_; }
    void ClearKeyframes();
    void GenerateWalkPreset();
    void GenerateRunPreset();
    void GenerateJumpPreset();
    void EnsureDefaultPlayerMotions();
    int GetActiveMotionIndex() const { return activeMotionIndex_; }
    void SetActiveMotionIndex(int index);
    const std::vector<MotionData>& GetMotions() const { return motions_; }
    const std::string& GetActiveMotionName() const { return GetMotionData().name; }
    void SetActiveMotionName(const std::string& name);
    float GetMotionDuration() const;
    void SetMotionDuration(float duration);

    // 簡易的なアニメーション (テスト用)
    void ApplyTestAnimation(float time, float speed = 1.0f);

    // 現在のボーン状態をキーフレームとして追加 (アニメーション作成用)
    void AddKeyframe(float time);
    
    // 特定のモーションを再生開始
    void PlayAnimation(const std::string& animationName);
    
    // アニメーションを評価し、時間 time におけるポーズを joints_ に適用する
    void EvaluateAnimation(float time);

    MotionData& GetMotionData();
    const MotionData& GetMotionData() const;

private:
    void CreateHumanoidSkeleton();
    void BuildJointMetadata();
    void CaptureRestPose();
    void GenerateHumanoidMesh();
    void AddCubeMesh(const Vector3& center, const Vector3& size, int jointIndex);
    void SmoothWeights();
    void CreateComputeSkinningPipeline(DirectXCommon* dxCommon);
    void TransitionSkinnedVertexBuffer(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES stateAfter);

private:
    std::vector<Joint> joints_;
    int rootJointIndex_ = -1;
    std::unordered_map<std::string, int> jointIndexMap_;
    std::vector<SkinnedVertexData> skinnedVertices_; // GPUに転送するスキニング用頂点データ

    // GPU バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> influenceBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> jointBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> skinnedVertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> skinningInformationBuffer_;
    WellForGPU* mappedPalette_ = nullptr;
    SkinningInformationForGPU* mappedSkinningInformation_ = nullptr;
    D3D12_VERTEX_BUFFER_VIEW               vertexBufferView_{};
    D3D12_VERTEX_BUFFER_VIEW               influenceBufferView_{};
    D3D12_VERTEX_BUFFER_VIEW               skinnedVertexBufferView_{};
    D3D12_RESOURCE_STATES                  skinnedVertexBufferState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> computePipelineState_;

    std::string name_ = "SkinnedModel";
    std::unique_ptr<Model> model_;                  // デバッグ描画や互換性のために保持
    uint32_t textureHandle_ = 0;                    // テクスチャハンドル

    std::vector<MotionData> motions_;               // 読み込まれたアニメーションデータ
    int activeMotionIndex_ = -1;                    // 現在再生中のアニメーションインデックス
    bool restPoseCaptured_ = false;
};






