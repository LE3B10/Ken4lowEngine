#pragma once

#include "BossMeleePhaseAttackBase.h"

/// 強パンチは共通基底へ長い予兆、高威力、専用演出の設定だけを渡す。
class BossHeavyPunchAttack : public BossMeleePhaseAttackBase
{
public:
	using Phase = BossMeleePhaseAttackBase::Phase;

	BossHeavyPunchAttack();
	const char* GetName() const override { return "HeavyPunch"; }
	int GetPriority() const override { return 80; }
};
