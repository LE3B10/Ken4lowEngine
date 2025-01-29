#pragma once
#include <cstdint>

/// ---------- 識別IDの定義 ---------- ///
enum class CollisionTypeIdDef : uint32_t
{
	kDefault,		 // デフォルトID
	kPlayer,		 // プレイヤーID
	kFloorBlock,	 // 床のブロックID
	kObstacle,		 // 障害物ID
};