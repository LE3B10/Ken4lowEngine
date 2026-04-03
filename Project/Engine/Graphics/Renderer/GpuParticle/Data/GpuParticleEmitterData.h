#pragma once
#include <cstdint>
#include <Vector3.h>

#include "GpuParticleType.h"
#include "BillboardMode.h"

namespace Ken4lowEngine
{

// エミッターの球体情報
struct GpuEmitterCBData
{
	Vector3 translate;		// 位置
	float radius;			// 半径
	uint32_t count;			// 発生数
	float frequency;		// 発生頻度
	float frequencyTime;	// 発生頻度タイマー
	uint32_t emit; 			// 発生フラグ
	uint32_t type;			// エミッターの種類
	uint32_t billboardMode; // ビルボードモード
};
} // namespace Ken4lowEngine
