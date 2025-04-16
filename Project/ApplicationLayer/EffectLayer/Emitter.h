#pragma once
#include <cstdint>
#include "WorldTransform.h"

struct Emitter final
{
	WorldTransform worldTransform; // エミッタのTransform
	uint32_t count;		 // 発生数
	float frequency;	 // 発生頻度
	float frequencyTime; // 頻度用時刻
};