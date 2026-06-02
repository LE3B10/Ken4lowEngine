#pragma once

#include <string_view>

/// 通常ゲームで生成する雑魚敵の種類。StressTest専用のScalableEnemyTypeとは分離して扱う。
enum class EnemyType
{
	Legacy,
	Melee,
	MidRange,
};

inline EnemyType ParseEnemyType(std::string_view enemyType)
{
	if (enemyType == "Melee" || enemyType == "melee")
	{
		return EnemyType::Melee;
	}
	if (enemyType == "MidRange" || enemyType == "midRange" || enemyType == "midrange")
	{
		return EnemyType::MidRange;
	}
	return EnemyType::Legacy;
}
