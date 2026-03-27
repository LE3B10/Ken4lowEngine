#include "EnemyBehavior_HeavyRifleman.h"
#include "Enemy.h"

void EnemyBehavior_HeavyRifleman::AfterFSMUpdate(Enemy& enemy, EnemyAICommand& cmd, float /*dt*/)
{
	if (enemy.GetStateId() != EnemyStateId::Attack) return;

	if (cmd.moveDir)
	{
		cmd.moveSpeed = enemy.GetMoveSpeed() * 0.55f;
	}

	if (cmd.fireAt.has_value())
	{
		cmd.stopMove = true;
		cmd.moveDir.reset();
		cmd.moveGoal.reset();
		cmd.moveSpeed.reset();
	}
}