#pragma once
#include <cstdint>

/// ---------- 武器のカテゴリー ---------- ///
enum class WeaponClass : uint8_t
{
	Primary, // プライマリ
	Backup,	 // バックアップ
	Melee,	 // 接近
	Special, // 特殊
	Sniper,	 // スナイパー
	Heavy,	 // 重機関銃
};