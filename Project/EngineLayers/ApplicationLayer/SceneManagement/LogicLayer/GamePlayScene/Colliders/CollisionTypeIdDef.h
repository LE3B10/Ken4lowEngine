#pragma once
#include <cstdint>

/// ---------- 識別IDの定義 ---------- ///
enum class CollisionTypeIdDef : uint32_t
{
	kDefault,		 // デフォルトID
	kPlayer,		 // プレイヤー
	kEnemy,			 // 敵のID
	kWeapon,		 // 武器のID
};