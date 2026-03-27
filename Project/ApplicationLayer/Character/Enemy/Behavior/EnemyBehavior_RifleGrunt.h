#pragma once
#include "EnemyArchetypeBehavior.h"

class EnemyBehavior_RifleGrunt final : public EnemyArchetypeBehavior
{
public:
	const char* GetDebugName() const override { return "EnemyBehavior_RifleGrunt"; }
};