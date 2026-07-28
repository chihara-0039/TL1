#pragma once

#include "MyMath.h"

/// <summary>
/// ワールド座標上の軸平行BOXコライダー。
///
/// Blenderから読み込んだ回転付きBOXは、ゲーム側の簡易物理と合わせるため、
/// 回転後の8頂点をすべて内包するAABBへ変換してからこの構造体へ格納する。
/// </summary>
struct WorldCollisionBox {
    Vector3 minimum{ 0.0f, 0.0f, 0.0f }; ///< BOXの各軸における最小座標。
    Vector3 maximum{ 0.0f, 0.0f, 0.0f }; ///< BOXの各軸における最大座標。
};
