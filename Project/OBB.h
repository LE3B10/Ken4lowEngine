#pragma once
#include "Vector3.h"

// OBBの構造体
struct OBB
{
	Vector3 center;
	Vector3 orientations[3];
	Vector3 size;
};