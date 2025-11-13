#pragma once
#include "Vector3.h"

/// ---------- 線分の構造体 ---------- ///
struct Segment final
{
	Vector3 origin; // 始点
	Vector3 diff;   // 終点からの差分
};

// Line Ray Segment