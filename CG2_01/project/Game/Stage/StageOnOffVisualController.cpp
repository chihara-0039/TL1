#include "StageOnOffVisualController.h"

#include "Object3d.h"

// StageMapのON/OFF状態に応じて、描画オブジェクトの色を更新する。
std::vector<Object3d*> StageOnOffVisualController::Apply(
    const StageMap& stageMap,
    const std::vector<std::unique_ptr<Object3d>>& objects) {

	// 変更があったオブジェクトのポインタを返す。
    std::vector<Object3d*> dirtyObjects;
    const bool isOn = stageMap.IsOnState();
    size_t objIndex = 0;

    // StageMapのセル順と描画オブジェクト配列の順番を同期させて色だけ更新する。
    for (int y = 0; y < stageMap.GetHeight(); ++y) {
        for (int z = 0; z < stageMap.GetDepth(); ++z) {
            for (int x = 0; x < stageMap.GetWidth(); ++x) {
                const MapCell* cell = stageMap.GetCell(x, y, z);
                if (!cell || cell->type == BlockType::None) {
                    continue;
                }

				// オブジェクト配列のインデックスが範囲外になったら終了する。
                if (objIndex >= objects.size()) {
                    return dirtyObjects;
                }

                Object3d* obj = objects[objIndex].get();
                if (!obj) {
                    // 空きスロットがあっても、セルとオブジェクトの対応順は崩さない。
                    ++objIndex;
                    continue;
                }

                if (cell->type == BlockType::OnOffSwitch) {
                    // スイッチは現在状態そのものを赤/青で表示する。
                    obj->SetColor(isOn
                        ? Vector4{ 1.0f, 0.2f, 0.2f, 1.0f }
                        : Vector4{ 0.2f, 0.2f, 1.0f, 1.0f });
                    dirtyObjects.push_back(obj);
                } else if (cell->type == BlockType::OnBlock) {
                    // ONブロックは非アクティブ時も位置確認できるよう半透明で残す。
                    obj->SetColor(isOn
                        ? Vector4{ 1.0f, 0.2f, 0.2f, 1.0f }
                        : Vector4{ 1.0f, 0.2f, 0.2f, 0.3f });
                    dirtyObjects.push_back(obj);
                } else if (cell->type == BlockType::OffBlock) {
                    // OFFブロックはONブロックと逆の条件で不透明/半透明を切り替える。
                    obj->SetColor(!isOn
                        ? Vector4{ 0.2f, 0.2f, 1.0f, 1.0f }
                        : Vector4{ 0.2f, 0.2f, 1.0f, 0.3f });
                    dirtyObjects.push_back(obj);
                }

                ++objIndex;
            }
        }
    }

    return dirtyObjects;
}
