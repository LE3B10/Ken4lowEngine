#pragma once

#include <string>
#include <vector>

// 攻撃ステップを分離して、Scratch/OneTwoを共通データで扱えるようにする

enum class MeleeAttackType
{
	Scratch = 0,
	OneTwo,
};

struct MeleeAttackStep
{
	std::string name;
	int damage = 0;
	float startTime = 0.0f;
	float activeTime = 0.0f;
	float range = 0.0f;
	float radius = 0.0f;
	float forwardMoveSpeed = 0.0f;
	float forwardMoveDuration = 0.0f;
};

struct MeleeAttackPattern
{
	MeleeAttackType type = MeleeAttackType::Scratch;
	std::string name;
	float recoveryTime = 0.0f;
	float cooldown = 0.0f;
	float forwardMoveSpeed = 0.0f;
	float forwardMoveDuration = 0.0f;
	std::vector<MeleeAttackStep> steps;
};
