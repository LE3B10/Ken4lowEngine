#pragma once
#include <cstdint>
#include <ParticleTransform.h>

/// -------------------------------------------------------------
///				　		エミッタ構造体
/// -------------------------------------------------------------
struct Emitter final
{
	ParticleTransform transform; // エミッタのTransform
	uint32_t count;		 // 発生数
	float frequency;	 // 発生頻度
	float frequencyTime; // 頻度用時刻
};