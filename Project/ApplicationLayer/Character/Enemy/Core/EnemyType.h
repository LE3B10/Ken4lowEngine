#pragma once

#include <string_view>

/// 通常ゲームで生成する雑魚敵の種類。StressTest専用のScalableEnemyTypeとは分離して扱う。
enum class EnemyType
{
	Legacy,
	Melee,
	MidRange,
};

inline EnemyType ParseEnemyType(std::string_view archetype)
{
	if (archetype == "Melee" || archetype == "melee")
	{
		return EnemyType::Melee;
	}
	if (archetype == "MidRange" || archetype == "midRange" || archetype == "midrange")
	{
		return EnemyType::MidRange;
	}
	return EnemyType::Legacy;
}
