#include "BlockInventory.h"

void BlockInventory::Initialize(int initialCount) {
    // プレイヤーが十分に遊べるよう、各ブロック種別のデフォルト所持数を99個にする（互換性維持のため）
    // ただし、0 が指定された場合は全てのブロックの所持数を 0 個にする（仕様対応）
    if (initialCount == 0) {
        wallCount_ = 0;
        ladderCount_ = 0;
        iceCount_ = 0;
        movingCount_ = 0;
        crumbleCount_ = 0;
        for (int i = 0; i < 5; ++i) {
            customCounts_[i] = 0;
        }
    } else {
        wallCount_ = (initialCount > 0) ? initialCount : 99;
        ladderCount_ = 99;
        iceCount_ = 99;
        movingCount_ = 99;
        crumbleCount_ = 99;
        for (int i = 0; i < 5; ++i) {
            customCounts_[i] = 99;
        }
    }
}

// 追加するブロックの種類と個数を指定して所持数を増やす。customIdが1～5の場合はカスタムパーツ枠を使用する。
void BlockInventory::AddBlock(BlockType type, int count, int customId) {
    if (count <= 0) {
        return;
    }

	// カスタムパーツ枠のIDが1～5の場合は、customCounts_配列に追加する
    if (customId >= 1 && customId <= 5) {
        customCounts_[customId - 1] += count;
    } else {
        if (type == BlockType::Wall) {
            wallCount_ += count;
        } else if (type == BlockType::Ladder) {
            ladderCount_ += count;
        } else if (type == BlockType::IceBlock) {
            iceCount_ += count;
        } else if (type == BlockType::MovingFloor) {
            movingCount_ += count;
        } else if (type == BlockType::CrumblingFloor) {
            crumbleCount_ += count;
        }
    }
}

// 後方互換用。通常Wallを追加する。
void BlockInventory::AddBlock(int count) {
    AddBlock(BlockType::Wall, count, 0);
}

// 指定ブロックの種類と個数を指定して所持数を消費する。足りない場合は状態を変えずfalseを返す。
bool BlockInventory::ConsumeBlock(BlockType type, int count, int customId) {
    if (count <= 0) {
        return false;
    }

	// カスタムパーツ枠のIDが1～5の場合は、customCounts_配列から消費する
    if (customId >= 1 && customId <= 5) {
        if (customCounts_[customId - 1] >= count) {
            customCounts_[customId - 1] -= count;
            return true;
        }
    } else {
        if (type == BlockType::Wall) {
            if (wallCount_ >= count) {
                wallCount_ -= count;
                return true;
            }
        } else if (type == BlockType::Ladder) {
            if (ladderCount_ >= count) {
                ladderCount_ -= count;
                return true;
            }
        } else if (type == BlockType::IceBlock) {
            if (iceCount_ >= count) {
                iceCount_ -= count;
                return true;
            }
        } else if (type == BlockType::MovingFloor) {
            if (movingCount_ >= count) {
                movingCount_ -= count;
                return true;
            }
        } else if (type == BlockType::CrumblingFloor) {
            if (crumbleCount_ >= count) {
                crumbleCount_ -= count;
                return true;
            }
        }
    }

    return false;
}

// 後方互換用。通常Wallを消費する。
bool BlockInventory::ConsumeBlock(int count) {
    return ConsumeBlock(BlockType::Wall, count, 0);
}

// 指定ブロックの現在所持数を返す。customIdが1～5の場合はカスタムパーツ枠の所持数を返す。
int BlockInventory::GetBlockCount(BlockType type, int customId) const {
    if (customId >= 1 && customId <= 5) {
        return customCounts_[customId - 1];
    }

	// 通常ブロックの所持数を返す
    if (type == BlockType::Wall) {
        return wallCount_;
    } else if (type == BlockType::Ladder) {
        return ladderCount_;
    } else if (type == BlockType::IceBlock) {
        return iceCount_;
    } else if (type == BlockType::MovingFloor) {
        return movingCount_;
    } else if (type == BlockType::CrumblingFloor) {
        return crumbleCount_;
    }
    return 0;
}

int BlockInventory::GetBlockCount() const {
    int total = wallCount_ + ladderCount_ + iceCount_ + movingCount_ + crumbleCount_;
    for (int i = 0; i < 5; ++i) {
        total += customCounts_[i];
    }
    return total;
}

bool BlockInventory::HasBlock(BlockType type, int customId) const {
    return GetBlockCount(type, customId) > 0;
    }

bool BlockInventory::HasBlock() const {
    if (wallCount_ > 0 || ladderCount_ > 0 || iceCount_ > 0 || movingCount_ > 0 || crumbleCount_ > 0) return true;
    for (int i = 0; i < 5; ++i) {
        if (customCounts_[i] > 0) return true;
    }
    return false;
}