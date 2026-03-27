#pragma once
#include "EnemyArchetypeBehavior.h"

class EnemyBehavior_Default final : public EnemyArchetypeBehavior
{
public:
	const char* GetDebugName() const override { return "EnemyBehavior_Default"; }
};