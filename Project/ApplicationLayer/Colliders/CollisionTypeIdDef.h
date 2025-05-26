#pragma once
#include <cstdint>

/// ---------- 識別IDの定義 ---------- ///
enum class CollisionTypeIdDef : uint32_t
{
	kDefault,		 // デフォルトID 0
	kPlayer,		 // プレイヤーID 1
	kWeapon,		 // 武器ID 2
	kEnemy,			 // エネミーID 3
	kBullet,		 // 弾丸ID 4
	kEnemyBullet,	 // 敵弾ID 5
	kItem,			 // アイテムID 6
};