#include "MapCursor.h"
#include <cassert>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

MapCursor::~MapCursor() = default;

void MapCursor::Initialize(Object3dCommon* object3dCommon) {
    assert(object3dCommon);
    object3dCommon_ = object3dCommon;

    // ステージエディタで現在選択しているセルを示すカーソルモデルを読み込む。
    cursorModel_ = Model::CreateFromOBJ(
        object3dCommon_->GetDxCommon(),
        "Resources/Models/choice",
        "choice.obj",
        object3dCommon_->GetTextureManager());

    cursorObject_ = std::make_unique<Object3d>();
    cursorObject_->Initialize(object3dCommon_);
    cursorObject_->SetModel(cursorModel_.get());

    // ブロックより少し小さくし、選択中セルが見やすいようにする。
    cursorObject_->SetScale({ 0.5f, 0.5f, 0.5f });
    cursorObject_->SetRotation({ 0.0f, 0.0f, 0.0f });
    cursorObject_->SetPosition(IndexToWorldPosition());
}

void MapCursor::Update(const Matrix4x4& lightVP) {
    if (!cursorObject_) {
        return;
    }

    // グリッドインデックスをワールド座標へ変換し、モデルの見た目に合わせて少し下げる。
    Vector3 cursorPosition = IndexToWorldPosition();
    cursorPosition.y += -0.16f;

    cursorObject_->SetPosition(cursorPosition);
    cursorObject_->SetScale(scale_);
    cursorObject_->Update(lightVP);
}

void MapCursor::Draw() {
    if (!cursorObject_) {
        return;
    }

    cursorObject_->Draw();
}

void MapCursor::SetCamera(const Matrix4x4& view, const Matrix4x4& projection) {
    if (!cursorObject_) {
        return;
    }

    cursorObject_->SetCamera(view, projection);
}

void MapCursor::Move(int dx, int dy, int dz, const StageMap& stageMap) {
    index_.x += dx;
    index_.y += dy;
    index_.z += dz;
    ClampToStage(stageMap);
}

void MapCursor::SetIndex(const Int3& index, const StageMap& stageMap) {
    index_ = index;
    ClampToStage(stageMap);
}

void MapCursor::ClampToStage(const StageMap& stageMap) {
    // カーソルがステージ範囲外へ出ないよう、各軸を有効範囲に丸める。
    // X/Zは原点中心のステージ編集に使えるよう、マップ幅と同じ距離だけ負方向を許可する。
    const int minimumX = stageMap.GetWidth() > 0 ? -(stageMap.GetWidth() - 1) : 0;
    const int minimumZ = stageMap.GetDepth() > 0 ? -(stageMap.GetDepth() - 1) : 0;
    if (index_.x < minimumX) { index_.x = minimumX; }
    if (index_.y < 0) { index_.y = 0; }
    if (index_.z < minimumZ) { index_.z = minimumZ; }

    if (stageMap.GetWidth() > 0 && index_.x >= stageMap.GetWidth()) {
        index_.x = stageMap.GetWidth() - 1;
    }
    if (stageMap.GetHeight() > 0 && index_.y >= stageMap.GetHeight()) {
        index_.y = stageMap.GetHeight() - 1;
    }
    if (stageMap.GetDepth() > 0 && index_.z >= stageMap.GetDepth()) {
        index_.z = stageMap.GetDepth() - 1;
    }
}

Vector3 MapCursor::IndexToWorldPosition() const {
    return {
        static_cast<float>(index_.x),
        static_cast<float>(index_.y) + 0.2f,
        static_cast<float>(index_.z)
    };
}

void MapCursor::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::Text("Cursor Index: (%d, %d, %d)", index_.x, index_.y, index_.z);
#endif
}
