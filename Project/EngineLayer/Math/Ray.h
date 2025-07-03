#pragma once
#include "Vector3.h"

//半直線
struct Ray final
{
	Vector3 origin;		//始点
	Vector3 diff;		//終点からの差分
};
