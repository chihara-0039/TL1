#pragma once
#include "StageMap.h"
#include "MyMath.h"
class Goal
{
public:
	// プレイヤー位置と半径をもとに、StageMap内のゴールセルとの接触を判定する。
	static bool Check(const Vector3& pos, const Vector3& radius, const StageMap& map);
};

