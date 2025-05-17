#pragma once
#include <cstdint>

/// ---------- 識別IDの定義 ---------- ///
enum class CollisionTypeIdDef : uint32_t
{
	kDefault,		 // デフォルトID
	kPlayer,		 // プレイヤーID
	kWeapon,		 // 武器ID
	kEnemy,			 // エネミーID
	kBullet,		 // 弾丸ID
};