#pragma once
#include "Vector3.h"

//直線
struct Line final
{
	Vector3 origin;		//始点
	Vector3 diff;		//終点からの差分
};