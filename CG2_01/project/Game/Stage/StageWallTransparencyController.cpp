#include "StageWallTransparencyController.h"

#include <cmath>
#include "Object3d.h"
#include "StageTransparencyPolicy.h"

// 壁オブジェクトの透過処理を行う。プレイヤーの位置とステージごとの透過エリアに応じて、壁のアルファ値を更新する。
void StageWallTransparencyController::Apply(
    std::vector<Object3d*>& wallObjects,
    const Vector3& cameraPos,
    const Vector3& playerPos,
    bool enableTransparency,
    float transparencyAlpha,
    int currentStageIndex) {

    // 現状はセル範囲ベースの透過判定のみ使う。引数は将来の視線判定拡張用に残している。
    cameraPos;

    if (!enableTransparency) {
        // 透過無効時は、前フレームで半透明にした壁を必ず不透明へ戻す。
        for (auto& obj : wallObjects) {
            if (obj) {
                obj->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
            }
        }
        return;
    }

    int playerCellX = static_cast<int>(std::floor(playerPos.x + 0.5f));
    int playerCellY = static_cast<int>(std::floor(playerPos.y + 0.5f));
    int playerCellZ = static_cast<int>(std::floor(playerPos.z + 0.5f));

    // 各壁を一度不透明に戻してから、そのフレームで必要な壁だけ半透明にする。
    for (auto& obj : wallObjects) {
        if (!obj) {
            continue;
        }

        Vector3 wallPos = obj->GetPosition();

        int wallCellX = static_cast<int>(std::floor(wallPos.x + 0.5f));
        int wallCellY = static_cast<int>(std::floor(wallPos.y + 0.5f));
        int wallCellZ = static_cast<int>(std::floor(wallPos.z + 0.5f));

        obj->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

        // プレイヤーより下の壁は視界を遮らないので対象外。
        if (wallCellY <= playerCellY) {
            continue;
        }

        // ステージごとの設計範囲外では、意図しない壁透過を起こさない。
        if (!StageTransparencyPolicy::IsTransparencyArea(
                currentStageIndex,
                wallCellX,
                wallCellY,
                wallCellZ)) {
            continue;
        }

        // プレイヤー近傍かつ上方向の壁だけを薄くして、奥行き感を残す。
        bool insideTransparencyArea =
            std::abs(wallCellX - playerCellX) <= 2 &&
            std::abs(wallCellZ - playerCellZ) <= 1 &&
            wallCellY <= playerCellY + 2;

		// 透過対象の壁は半透明にする。
        if (insideTransparencyArea) {
            obj->SetColor({ 1.0f, 1.0f, 1.0f, transparencyAlpha });
        }
    }
}
