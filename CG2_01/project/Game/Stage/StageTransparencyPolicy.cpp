#include "StageTransparencyPolicy.h"

// ステージごとの透過エリア判定を行う。
bool StageTransparencyPolicy::IsTransparencyArea(int stageIndex, int x, int y, int z) {
    switch (stageIndex) {
    case 0:
        // ステージ0は壁透過用の特殊エリアを持たない。
        return false;
    case 1:
        // ステージ1の通路部分だけを透過対象にし、他の壁は通常表示のままにする。
        return x >= 6 && x <= 15 &&
               y >= 1 && y <= 3 &&
               z == 6;
    default:
        // 未定義ステージでは誤透過を避けるため、安全側でfalseに倒す。
        return false;
    }
}
