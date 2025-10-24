#pragma once
#include "WeaponConfig.h"

// 射撃状態構造体
struct FireState
{
	WeaponConfig weaponConfig = {}; // 武器設定
	float cooldown = 0.0f;          // 次の射撃までの時間
	float interval = 0.1f;          // 連射間隔（秒）→ 例: 600rpm ≒ 0.1s
	bool shotScheduled = false;     // クールダウン終了時に撃つ予約
};
