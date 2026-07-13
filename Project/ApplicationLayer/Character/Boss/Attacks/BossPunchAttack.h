#pragma once

#include "BossMeleePhaseAttackBase.h"

/// 通常パンチは共通基底へタイミング、威力、命中形状、演出設定だけを渡す。
class BossPunchAttack : public BossMeleePhaseAttackBase
{
public:
	using Phase = BossMeleePhaseAttackBase::Phase;

	BossPunchAttack();
	const char* GetName() const override { return "Punch"; }
	int GetPriority() const override { return 50; }
};
