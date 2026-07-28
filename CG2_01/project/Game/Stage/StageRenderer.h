#pragma once
#include <vector>
#include <memory>
#include <unordered_map>
#include "StageMap.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include "Model.h"
#include "StagePSwitchVisualController.h"
#include <GameplayCameraController.h>


class StageRenderer {
public:
    
    ~StageRenderer();

    void Initialize(Object3dCommon* object3dCommon);
    void BuildFromStageMap(const StageMap& stageMap);

	void SetBlockScale(const Vector3& scale) { blockScale_ = scale; }
    const Vector3& GetBlockScale() const { return blockScale_; }
    void SetCamera(const Matrix4x4& view, const Matrix4x4& projection);

    void SetIsEditorMode(bool isEditor) { isEditorMode_ = isEditor; }
    bool GetIsEditorMode() const { return isEditorMode_; }

    void Update(const StageMap& stageMap, const Matrix4x4& lightVP);
    void DrawShadow(const Matrix4x4& lightVP);
    void DrawTransparent();
    void Draw();

    void UpdateEffect(const StageMap& stageMap);

    void ApplyPSwitchVisualState(const StageMap& stageMap);

    // 配置プレビュー表示機能
    void SetPlacementPreview(
        const StageMap& stageMap,
        const Int3& cursorIndex,
        BlockType type,
        int customId,
        float rotationY
    );
    void ClearPlacementPreview();

    void Clear();

	// ON/OFFブロックとスイッチの視覚状態をステージマップに基づいて更新する
    void ApplyOnOffVisualState(const StageMap& stageMap);

private:
    Object3dCommon* object3dCommon_ = nullptr;

    std::unique_ptr<Model> groundModel_;
    std::unique_ptr<Model> wallModel_;
    std::unique_ptr<Model> bubbleModel_;
    std::unique_ptr<Model> goalModel_;
    std::unique_ptr<Model> ladderModel_;
    std::unique_ptr<Model> doorModel_;
    std::unique_ptr<Model> pSwichModel_;
    std::unique_ptr<Model> pBlockOnModel_;
    //Model* PBlockOff_ = nullptr;
    std::unique_ptr<Model> crumbleModel_;
    std::unique_ptr<Model> iceBlockModel_;
    std::unique_ptr<Model> movingFloorModel_;
    // ▼ 追加 ▼
    std::unique_ptr<Model> keyModel_;
    std::unique_ptr<Model> keyBlockModel_;
    // 中間地点
    std::unique_ptr<Model> checkpointModel_;
    std::unique_ptr<Model> spikeModel_;
    // ONOFFブロックとスイッチ
    std::unique_ptr<Model> onBlockModel_;
    std::unique_ptr<Model> offBlockModel_;
    std::unique_ptr<Model> onOffSwichModel_;

    struct CloudInstance {
        std::vector<std::unique_ptr<Object3d>> objects; // 雲を構成する球体オブジェクトのリスト
        Vector3 basePosition;  // 基準位置
        Vector3 speed;         // 流れる速度
        float floatTimer;      // フワフワ動くための個別タイマー
        float floatSpeed;      // フワフワ速度
        std::vector<Vector3> localOffsets; // 基準位置からの相対座標
        std::vector<Vector3> localScales;  // 各球体のスケール
    };
    std::vector<CloudInstance> clouds_;
    std::vector<std::unique_ptr<Object3d>> objects_;
    size_t activeObjectCount_ = 0;
    std::vector<std::unique_ptr<Object3d>> previewObjects_; // 半透明プレビュー用オブジェクト
    Vector3 blockScale_{ 1.0f, 1.0f, 1.0f };

private:
    Object3d* CreateStageObject(Model* model, const Vector3& position, const Vector3& scale, const Vector3& rotation, BlockType type = BlockType::None);

    // 動く足場とマップ上のセル位置を紐付ける構造体
        struct MovingFloorInstance 
        {
            Object3d* object = nullptr; // 3Dオブジェクトへのポインタ
            Int3 cellIndex;            // StageMap上での [x, y, z] の位置
        };

    // ステージ内のすべての動く足場を管理するリスト
    std::vector<MovingFloorInstance> movingFloorInstances_;

    // 動く足場の管理リストの近くに追加
    struct CrumblingFloorInstance {
        Object3d* object = nullptr; // 3Dオブジェクトへのポインタ
        Int3 cellIndex;             // StageMap上での [x, y, z] 位置
    };
    std::vector<CrumblingFloorInstance> crumblingFloorInstances_; // 崩れる足場の管理リスト
    // ステージ内のすべての敵キャラクターを管理するリスト
    struct EnemyInstance 
    {
        Object3d* object = nullptr; // 3Dオブジェクトへのポインタ
        Int3 cellIndex;            // StageMap上での [x, y, z] の位置
    };
    std::vector<EnemyInstance> enemyInstances_;

    std::vector<StagePSwitchVisualObject> pSwitchObjects_;
    std::vector<StagePSwitchVisualObject> pBlockObjects_;

	// 壁のオブジェクトを管理するリスト
    std::vector<Object3d*> wallObjects_;
    struct TimedBlockInstance {
        Object3d* object = nullptr;
        Int3 cellIndex;
    };
    std::vector<TimedBlockInstance> timedBlockInstances_;

    bool isEditorMode_ = false;

private:
    // インスタンシング描画用のデータ構造
    struct InstanceData {
        Matrix4x4 world;
        Vector4 color;
        float shininess;
        float metallic;
        float emissive;
        float padding[3];
    };

	// インスタンシング描画用の定数バッファ構造体 (View/Projection)
    struct ViewProjectionMatrix {
        Matrix4x4 viewProjection;
        Matrix4x4 lightViewProjection;
    };

    // インスタンシング用の定数バッファ (View/Projection)
    Microsoft::WRL::ComPtr<ID3D12Resource> viewProjectionResource_;
    ViewProjectionMatrix* viewProjectionData_ = nullptr;

    // モデルごとの StructuredBuffer 管理用
    struct InstancedBufferInfo {
        Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
        UINT maxInstances = 0;
    };
    std::unordered_map<Model*, InstancedBufferInfo> instancedBuffers_;

    // 最後に使用されたライトのViewProjection行列 (メンバ変数に保存)
    Matrix4x4 lastLightVP_{};

    // StructuredBuffer を動的に生成・取得するヘルパー
    ID3D12Resource* GetOrCreateInstancedBuffer(Model* model, UINT numInstances);

    // --- 高速インスタンシング描画用グループ管理構造とメソッド ---
public:
	// インスタンス描画用のオブジェクト情報を保持する構造体
    struct RenderInstance {
        Object3d* object = nullptr;
        size_t index = 0; // objects_ または previewObjects_ 内のインデックス
    };

	// モデルごとにインスタンシング描画用のグループを管理する構造体
    struct RenderGroup {
        Model* model = nullptr;
        std::vector<RenderInstance> instances;
        std::vector<InstanceData> instanceData;
        Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
        UINT maxInstances = 0;
        bool isDirty = true;
    };

private:
	// インスタンシング描画用のグループリスト
    std::vector<RenderGroup> renderGroups_;
    std::vector<RenderGroup> previewRenderGroups_;
    // オブジェクトの生ポインタから、renderGroups_ 内の [グループインデックス, インスタンスインデックス] を高速に引くためのマップ
    std::unordered_map<Object3d*, std::pair<size_t, size_t>> objectToInstanceMap_;

    void BuildRenderGroups();
    void BuildPreviewRenderGroups();
    void MarkDirty(Object3d* obj);

    std::vector<RenderGroup> transparentRenderGroups_;

    void RebuildTransparencyGroups();

public:

	// 壁の透過処理
    void UpdateWallTransparency(
        const Vector3& cameraPos,
        const Vector3& playerPos,
        bool enableTransparency,
        float transparencyAlpha,
        int currentStageIndex
    );
    //雲の透過
    void UpdateCloudTransparency(const Vector3& cameraPos, const Vector3& playerPos);

};
