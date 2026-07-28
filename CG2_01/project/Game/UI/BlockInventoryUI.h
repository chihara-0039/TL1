#pragma once
#include <memory>
#include <vector>
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "Input.h"
#include "../Block/BlockInventory.h"
#include "StageMap.h"

// 配置可能ブロックの所持数、選択状態、使用要求を表示・操作するインベントリUI。
class BlockInventoryUI {
public:
    // パネルの開閉アニメーション状態。
    enum class State {
        Closed,
        Opening,
        Opened,
        Closing
    };

    // UI上の1ブロックボタンに必要な描画情報と選択情報。
    struct BlockButton {
        BlockType type;
        std::unique_ptr<Sprite> sprite;
        uint32_t textureHandle;
        Vector2 localPos; // パネル左上からの相対座標。
        Vector2 size;
        bool isAvailable = false;
        int customId = 0; // 0: 通常ブロック, 1～5: カスタムパーツ
        std::vector<std::unique_ptr<Sprite>> silhouetteSprites; // 3x3断面表示用のシルエット。
        std::vector<std::unique_ptr<Sprite>> countSprites; // 所持数ドット表示用スプライト。
    };

public:
    // UIスプライトとテクスチャ、参照するインベントリを初期化する。
    void Initialize(DirectXCommon* dxCommon, SpriteCommon* spriteCommon, TextureManager* textureManager, BlockInventory* inventory);
    // 入力とゲーム状態に応じて、開閉・選択・使用要求を更新する。
    void Update(Input* input, WinApp* winApp, bool isGamePlayMode, const StageMap* stageMap = nullptr);
    void Draw();
    void Finalize();

    // インベントリが開いている、または開閉アニメーション中かを返す。
    bool IsActive() const { return state_ != State::Closed; }
    bool IsOpened() const { return state_ == State::Opened; }

    // 現在選択されている通常ブロック種別。
    BlockType GetSelectedBlockType() const { return selectedBlockType_; }
    void SetSelectedBlockType(BlockType type) { selectedBlockType_ = type; }

    // 現在選択されているカスタムパーツID。0は通常ブロックを表す。
    int GetSelectedCustomId() const { return selectedCustomId_; }
    void SetSelectedCustomId(int id) { selectedCustomId_ = id; }

    // マウスクリック判定などで使う、現在のパネル左端X座標。
    float GetPanelLeftX() const { return currentPos_.x; }

    // 現在の開閉状態。
    State GetState() const { return state_; }

    // インベントリの開閉状態を切り替える。
    void ToggleOpen();

private:
    // マウス座標が指定矩形内に入っているかを判定する。
    bool CheckClick(const Vector2& pos, const Vector2& size, float mouseX, float mouseY);

private:
    SpriteCommon* spriteCommon_ = nullptr;
    TextureManager* textureManager_ = nullptr;
    BlockInventory* inventory_ = nullptr;
    const StageMap* stageMap_ = nullptr; // カスタムパーツ名や形状取得用の参照。

    // パネル背景スプライト。
    std::unique_ptr<Sprite> panelSprite_;
    uint32_t panelTextureHandle_ = 0;

    // 選択中ボタンのフォーカス枠スプライト。
    std::unique_ptr<Sprite> focusSprite_;
    uint32_t focusTextureHandle_ = 0;

    // ブロック選択ボタン一覧。
    std::vector<BlockButton> buttons_;
    BlockType selectedBlockType_ = BlockType::Ground;
    int selectedCustomId_ = 0; // 0: 通常ブロック, 1～5: カスタムパーツ

    // パネル開閉状態とアニメーション位置。
    State state_ = State::Closed;
    Vector2 currentPos_;
    Vector2 closedPos_;
    Vector2 openedPos_;

    float panelWidth_ = 320.0f;
    float panelHeight_ = 720.0f;
    float tabWidth_ = 64.0f; // 開閉タブの幅。
    float arrowWidth_ = 45.0f; // 画面端に残す矢印部分の幅。

    bool prevMouseLeft_ = false;

    // ダブルクリック判定用。
    uint32_t lastClickTick_ = 0;
    BlockType lastClickedType_ = BlockType::None;
    bool useRequested_ = false;

public:
    // ダブルクリックによる即時使用要求を取得し、同時に消費する。
    bool ConsumeUseRequest() {
        bool req = useRequested_;
        useRequested_ = false;
        return req;
    }
};
