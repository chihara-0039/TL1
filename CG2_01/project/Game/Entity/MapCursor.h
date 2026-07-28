#pragma once
#include "StageMap.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include "Model.h"
#include <memory>

// ステージ編集時に現在選択中のセルを示す3Dカーソル。
class MapCursor {
public:
    MapCursor() = default;
    ~MapCursor();

    // カーソル表示に必要なモデルとObject3dを生成する。
    void Initialize(Object3dCommon* object3dCommon);
    // 現在インデックスに合わせて表示位置を更新する。
    void Update(const Matrix4x4& lightVP);
    void Draw();

    void SetCamera(const Matrix4x4& view, const Matrix4x4& projection);
    void SetScale(const Vector3& scale) { scale_ = scale; }

    // ステージ範囲内に収まるようにセル単位でカーソルを移動する。
    void Move(int dx, int dy, int dz, const StageMap& stageMap);
    void SetIndex(const Int3& index, const StageMap& stageMap);

    const Int3& GetIndex() const { return index_; }

    // ImGuiデバッグ表示用。
    void DrawImGui();

private:
    Object3dCommon* object3dCommon_ = nullptr;
    std::unique_ptr<Model> cursorModel_;
    std::unique_ptr<Object3d> cursorObject_;

    Int3 index_{ 0, 0, 0 };
    Vector3 scale_ = { 1.2f, 1.2f, 1.2f }; // デフォルトのカーソル表示スケール。

private:
    // 現在インデックスをStageMapの有効範囲へ丸める。
    void ClampToStage(const StageMap& stageMap);
    // セル座標を描画用ワールド座標へ変換する。
    Vector3 IndexToWorldPosition() const;
};
