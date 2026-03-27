#pragma once
#include "EnemyArchetypeBehavior.h"

class EnemyBehavior_BurstTrooper final : public EnemyArchetypeBehavior
{
public:
	void AfterFSMUpdate(Enemy& enemy, EnemyAICommand& cmd, float dt) override;
	const char* GetDebugName() const override { return "EnemyBehavior_BurstTrooper"; }
};