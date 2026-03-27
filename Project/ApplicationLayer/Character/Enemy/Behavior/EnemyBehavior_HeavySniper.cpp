#include "EnemyBehavior_HeavySniper.h"
#include "Enemy.h"
#include <cmath>

void EnemyBehavior_HeavySniper::AfterFSMUpdate(Enemy& enemy, EnemyAICommand& cmd, float /*dt*/)
{
	if (!cmd.lookAt) return;

	using K4E::Vector3;
	const Vector3 selfPos = enemy.GetCenterPosition();
	Vector3 toTarget = *cmd.lookAt - selfPos;
	toTarget.y = 0.0f;

	const float dist = Vector3::LengthXZ(toTarget);
	const Vector3 dirTo = Vector3::NormalizeXZ(toTarget);
	if (dist <= 1e-6f) return;

	const float tooClose = enemy.GetAttackRange() * 0.60f;
	if (dist < tooClose)
	{
		cmd.moveGoal.reset();
		cmd.moveDir = dirTo * -1.0f;
		cmd.moveSpeed = enemy.GetMoveSpeed() * 0.80f;
		return;
	}

	if (cmd.fireAt)
	{
		cmd.stopMove = true;
		cmd.moveDir.reset();
		cmd.moveGoal.reset();
		cmd.moveSpeed.reset();
	}
}