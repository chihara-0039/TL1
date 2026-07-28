#pragma once
#include "../Stage/StageMap.h" // BlockTypeを使用するため

// プレイヤーが所持している配置可能ブロックの種類別・カスタム別個数を管理する。
class BlockInventory {
public:
    // 全所持数を初期化する。initialCountは互換用に通常Wallへ反映する。
    void Initialize(int initialCount = 0);

    // 指定ブロックの所持数を増やす。customIdが0以外の場合はカスタムパーツ枠を使う。
    void AddBlock(BlockType type, int count = 1, int customId = 0);
    void AddBlock(int count = 1); // 後方互換用。通常Wallを追加する。

    // 指定ブロックの所持数を消費する。足りない場合は状態を変えずfalseを返す。
    bool ConsumeBlock(BlockType type, int count = 1, int customId = 0);
    bool ConsumeBlock(int count = 1); // 後方互換用。通常Wallを消費する。

    // 指定ブロックの現在所持数を返す。
    int GetBlockCount(BlockType type, int customId = 0) const;
    int GetBlockCount() const; // 後方互換用。全配置可能ブロックの合計を返す。

    // 指定ブロックを1個以上所持しているかを返す。
    bool HasBlock(BlockType type, int customId = 0) const;
    bool HasBlock() const; // 後方互換用。何か1つでも所持しているかを返す。

private:
    int wallCount_ = 0;
    int ladderCount_ = 0;
    int iceCount_ = 0;
    int movingCount_ = 0;
    int crumbleCount_ = 0;
    int customCounts_[5] = { 0, 0, 0, 0, 0 }; // カスタムパーツID 1～5 の所持数
};
