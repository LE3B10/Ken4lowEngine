#pragma once
#include <cstdint>

/// ---------- 敵タイプ列挙型 ---------- ///
enum class EnemyType : uint32_t
{
	MeleeGrunt,	  // 近接攻撃をする雑魚
	ShooterGrunt, // 飛び道具を使う雑魚
	Rusher,		  // 高速で突進してくる敵
	Tank,		  // 高耐久の重装甲敵
};

// 将来敵にJson設定を読み込ませるときに使う予定