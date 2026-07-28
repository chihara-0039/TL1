#include "StageCrumblingFloorEffectUpdater.h"

#include "Object3d.h"
#include "StageMap.h"

// 崩れる床の進行状態を描画オブジェクトに反映する。
std::vector<Object3d*> StageCrumblingFloorEffectUpdater::Apply(
    const StageMap& stageMap,
    bool isEditorMode,
    const std::vector<std::unique_ptr<Object3d>>& objects) {

	// 変更があったオブジェクトのポインタを返す。
    std::vector<Object3d*> dirtyObjects;
    size_t objIndex = 0;

    // objectsはStageRenderer::BuildFromStageMapと同じセル走査順で作られている前提。
    for (int y = 0; y < stageMap.GetHeight(); ++y) {
        for (int z = 0; z < stageMap.GetDepth(); ++z) {
            for (int x = 0; x < stageMap.GetWidth(); ++x) {
                const MapCell* cell = stageMap.GetCell(x, y, z);
                if (!cell || cell->type == BlockType::None) {
                    continue;
                }

                // プレイ中はPlayerStartの描画オブジェクトが作られないため、インデックスも進めない。
                if (cell->type == BlockType::PlayerStart && !isEditorMode) {
                    continue;
                }

                // マップと描画オブジェクト数がずれても範囲外アクセスしない。
                if (objIndex >= objects.size()) {
                    return dirtyObjects;
                }

                Object3d* obj = objects[objIndex].get();
                if (cell->type == BlockType::CrumblingFloor) {
                    // 崩れる床の進行状態はMapCell側の色成分と透明度で表現する。
                    obj->SetColor({
                        1.0f,
                        cell->colorG,
                        cell->colorB,
                        cell->opacity
                    });
					// 変更があったオブジェクトとしてリストに追加する。
                    dirtyObjects.push_back(obj);
                }

                objIndex++;
            }
        }
    }

    return dirtyObjects;
}
