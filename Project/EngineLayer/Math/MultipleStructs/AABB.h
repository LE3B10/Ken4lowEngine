#pragma once
#include "Vector3.h"

struct AABB final
{
	Vector3 min; // 最小値
	Vector3 max; // 最大値
};