#pragma once

class StageMap;

// StageMapが保持するギミック状態の更新処理を集約する。
class StageMapGimmickSystem {
public:
    // 指定IDのPスイッチを起動し、Pブロックの状態更新まで行う。
    static void SetPSwitchActive(StageMap& stageMap, int switchId);
    // ステージ再構築を伴わずにPスイッチ状態だけ初期化する。
    static void ResetPSwitchStateNoRebuild(StageMap& stageMap);
    // Pスイッチ状態を初期化し、必要なマップ状態も再同期する。
    static void ResetPSwitchState(StageMap& stageMap);
    // ON/OFF系ブロックの現在状態を反転する。
    static void ToggleOnState(StageMap& stageMap);
};
