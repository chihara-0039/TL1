#pragma once

#include "StageMap.h"
#include "StageRenderer.h"
#include "BlockInventory.h"
#include "MyMath.h"

#include <functional>

// プレイヤー位置をもとに、近くのシャボン玉ブロックを回収して所持数へ反映する。
class BubblePickupController {
public:
    // 回収対象のマップ、描画、所持数管理を外部から注入する。
    void Initialize(
        StageMap* stageMap,
        StageRenderer* stageRenderer,
        BlockInventory* inventory
    );

    // プレイヤー周辺セルを確認し、回収可能なシャボン玉があれば取得する。
    void Update(const Vector3& playerPosition);

    // 回収演出やSEを呼び出すための通知先を登録する。
    void SetCollectCallback(std::function<void(const Vector3&)> callback);

private:
    // ワールド座標を最寄りのステージセル座標へ変換する。
    Int3 ToGridIndex(const Vector3& position) const;

    // 指定セルにシャボン玉があれば回収し、マップと描画を更新する。
    bool TryCollectAt(const Int3& index);

private:
    StageMap* stageMap_ = nullptr;
    StageRenderer* stageRenderer_ = nullptr;
    BlockInventory* inventory_ = nullptr;
    std::function<void(const Vector3&)> collectCallback_;
};
