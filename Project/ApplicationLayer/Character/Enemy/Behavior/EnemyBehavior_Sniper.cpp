#include "EnemyBehavior_Sniper.h"
#include "Enemy.h"
#include <cmath>

void EnemyBehavior_Sniper::AfterFSMUpdate(Enemy& enemy, EnemyAICommand& cmd, float /*dt*/)
{
	if (!cmd.lookAt) return;

	using K4E::Vector3;
	const Vector3 selfPos = enemy.GetCenterPosition();
	Vector3 toTarget = *cmd.lookAt - selfPos;
	toTarget.y = 0.0f;

	const float dist = Vector3::LengthXZ(toTarget);
	const Vector3 dirTo = Vector3::NormalizeXZ(toTarget);
	if (dist <= 1e-6f) return;

	const float tooClose = enemy.GetAttackRange() * 0.55f;
	if (dist < tooClose)
	{
		cmd.moveGoal.reset();
		cmd.moveDir = dirTo * -1.0f;
		cmd.moveSpeed = enemy.GetMoveSpeed() * 1.05f;
		return;
	}

	if (cmd.fireAt.has_value())
	{
		cmd.stopMove = true;
		cmd.moveDir.reset();
		cmd.moveGoal.reset();
		cmd.moveSpeed.reset();
	}
}