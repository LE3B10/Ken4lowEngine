#pragma once
#include "Vector3.h"

//平面
struct Plane final
{
	Vector3 normal;		//!< 法線
	float distance;		//!< 距離
};
