#include "StagePSwitchVisualController.h"

#include "Object3d.h"

namespace {
// Pスイッチ発動中は対象を非表示扱いにするため、スケール0で描画から消す。
void ApplyScale(
    bool active,
    std::vector<StagePSwitchVisualObject>& objects,
    std::vector<Object3d*>& dirtyObjects) {

    for (auto& item : objects) {
        if (!item.object) {
            continue;
        }

        // 復帰時に元の大きさへ戻せるよう、通常スケールは登録時の値を使う。
        item.object->SetScale(active ? Vector3{ 0.0f, 0.0f, 0.0f } : item.normalScale);
        dirtyObjects.push_back(item.object);
    }
}
}

// PスイッチのON/OFF状態に応じて、描画オブジェクトのスケールを更新する。
std::vector<Object3d*> StagePSwitchVisualController::Apply(
    bool active,
    std::vector<StagePSwitchVisualObject>& switchObjects,
    std::vector<StagePSwitchVisualObject>& blockObjects) {

    std::vector<Object3d*> dirtyObjects;
    // スイッチ本体とPブロックは同じ表示ルールなので、同じ処理でまとめて更新する。
    ApplyScale(active, switchObjects, dirtyObjects);
    ApplyScale(active, blockObjects, dirtyObjects);
    return dirtyObjects;
}
