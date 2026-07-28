#pragma once
#include <vector>
#include <string>
#include <filesystem>

#include "Input.h"
#include "Camera.h"
#include "StageMap.h"
#include "StageRenderer.h"
#include "MapCursor.h"
#include "Player.h"
#include "LightCamera.h"

/// <summary>
/// ステージエディタ（作成・編集モード）の管理を行うコントローラークラス。
/// ファイルへの保存・読み込み、ブロック配置モード（1〜7キーなどでの切り替え）、
/// マップカーソル移動、カメラ操作、ドアのペアリング処理などを統合管理します。
/// </summary>
class StageEditorController {
public:
    /// <summary>
    /// エディタ用パラメータや選択状態、ステージファイルリストの初期化を行います。
    /// </summary>
    void Initialize();

    /// <summary>
    /// エディタモードにおける毎フレームの更新処理（キー入力、配置・削除判定など）を行います。
    /// </summary>
    void Update(Input* input, StageMap& stageMap, StageRenderer* stageRenderer, MapCursor* mapCursor, LightCamera* lightCamera, Player* player, Camera* camera);

    /// <summary>
    /// ImGui によるエディタ用パネル（セーブロード、設定、ツールバー）を描画します。
    /// </summary>
    void DrawImGui(StageMap& stageMap, StageRenderer* stageRenderer, MapCursor* mapCursor, Player* player);

    /// <summary>
    /// WASD/QE キーによるマップカーソルの三次元移動処理を行います。
    /// </summary>
    void HandleCursorInput(Input* input, StageMap& stageMap, MapCursor* mapCursor, LightCamera* lightCamera, Camera* camera);

    /// <summary>
    /// IJKL/UO キーによるエディタ専用カメラの移動・回転操作を行います。
    /// </summary>
    void HandleCameraInput(Input* input, Camera* camera);

    /// <summary>
    /// 現在選択されているブロックの種類を取得します。
    /// </summary>
    BlockType GetSelectedBlockType() const { return selectedBlockType_; }

    /// <summary>
    /// 配置するブロックの種類を設定します。
    /// </summary>
    void SetSelectedBlockType(BlockType type) { selectedBlockType_ = type; }

    /// <summary>
    /// ステージマップ全体を走査し、プレイヤースタートブロック（PlayerStart）の位置へプレイヤーをリセットします。
    /// </summary>
    void ResetPlayerToStartCell(StageMap& stageMap, Player* player);

private:
    /// <summary>
    /// Resources/Stages フォルダ内のステージファイル（.txt）一覧を再取得・更新します。
    /// </summary>
    void RefreshStageList();

    /// <summary>
    /// 現在のカーソル位置に対して、選択中のブロック（またはドアのペアリング）を配置・適用します。
    /// </summary>
    void ApplyPlacement(StageMap& stageMap, StageRenderer* stageRenderer, MapCursor* mapCursor, Player* player);

    /// <summary>
    /// ImGui 内にブロック一覧ボタンや回転・配置・削除ボタンなどのツールバーを描画します。
    /// </summary>
    void DrawEditorToolbar(StageMap& stageMap, StageRenderer* stageRenderer, MapCursor* mapCursor, Player* player);

    // ==========================================================
    // メンバ変数
    // ==========================================================
    Vector3 editorBlockScale_{ 1.0f, 1.0f, 1.0f }; // エディタ内でのブロック表示スケール
    float editorUniformBlockScale_ = 1.0f;         // 全体均等スケール調整用スライダー値

    std::vector<std::string> stageFiles_;          // 保存されているステージファイル名リスト
    char newStageName_[64] = "new_stage";          // 新規保存時の入力バッファ
    int selectedStageIndex_ = -1;                  // リストボックスで選択中のインデックス

    BlockType selectedBlockType_ = BlockType::Ground; // 現在配置対象となっているブロック種別
    BlockType bubbleInsideBlockType_ = BlockType::Wall; // シャボン玉（BubblePickup）に仕込むブロック種別
    
    // --- カスタムブロックパーツ作成用 ---
    int selectedCustomPartSlot_ = 1;               // 編集中のカスタムスロット (1〜5)
    int bubbleInsideCustomSlot_ = 0;               // シャボン玉に仕込むカスタムID (0: デフォルト, 1〜5: カスタムパーツ)

    // --- ドアギミック用のペアリング管理 ---
    bool isWaitingForSecondDoor_ = false;          // 1つ目のドアを配置し、2つ目を待機中かどうかのフラグ
    Int3 firstDoorIndex_ = { -1, -1, -1 };         // 1つ目に配置されたドアのマップ座標

    // エディタ上で設定する動く足場の移動量
    Int3 currentMoveOffset_{ 0, 3, 0 }; // 初期値（例として上に3マス）

    int selectedDoorId_ = 1; // 現在選択中のドア番号 (1〜9など)
    int selectedPSwitchId_ = 1;
    int selectedTimedGroupId_ = 1;
    int selectedTimedOrderId_ = 0;
	int holdFrame_ = 0; // キーを押し続けているフレーム数をカウントする変数（長押し判定用）
	int placeHoldFrame_ = 0; // ブロック配置の長押しフレーム数

	// キーのリピート入力を判定するヘルパー関数
    bool RepeatKey(Input* input, BYTE key, int firstDelay = 20, int interval = 5);

	int newStageWidth_ = 100;    // 新規ステージの幅（X方向）
	int newStageHeight_ = 100;    // 新規ステージの高さ（Y方向）
	int newStageDepth_ = 100;    // 新規ステージの深さ（Z方向）

    // --- プレイリスト(Campaign)管理 ---
    std::vector<std::string> campaignFiles_;
    std::vector<std::string> availableFiles_;
    int selectedCampaignIndex_ = -1;
    int selectedAvailableIndex_ = -1;
    void LoadCampaignSequence();
    void SaveCampaignSequence();
};
