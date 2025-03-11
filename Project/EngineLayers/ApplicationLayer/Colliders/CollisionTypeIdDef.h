#pragma once
#include <cstdint>

/// ---------- 識別IDの定義 ---------- ///
enum class CollisionTypeIdDef : uint32_t
{
	kDefault,		 // デフォルトID - 0
	kPlayer,		 // プレイヤーID - 1
	kWeapon,		 // ウェポンID - 2
	kEnemy,			 // エネミーID - 3
};