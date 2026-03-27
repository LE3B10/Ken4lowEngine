#pragma once
#include "EnemyAICommand.h"
#include "EnemyStateMachine.h"
#include "EnemyArchetype.h"

class Enemy;

/// ------------------------------------------------------------
/// EnemyArchetypeBehavior
/// ------------------------------------------------------------
/// Enemy 本体や FSM を分岐させず、アーキタイプ差分だけを担当する基底。
/// ------------------------------------------------------------
class EnemyArchetypeBehavior
{
public:
	virtual ~EnemyArchetypeBehavior() = default;

	virtual void OnApplyTuning(Enemy& enemy, const EnemyTuning& tuning)
	{
		(void)enemy;
		(void)tuning;
	}

	virtual void BeforeFSMUpdate(Enemy& enemy, EnemyAIContext<Enemy>& ctx)
	{
		(void)enemy;
		(void)ctx;
	}

	virtual void AfterFSMUpdate(Enemy& enemy, EnemyAICommand& cmd, float dt)
	{
		(void)enemy;
		(void)cmd;
		(void)dt;
	}

	virtual const char* GetDebugName() const = 0;
};