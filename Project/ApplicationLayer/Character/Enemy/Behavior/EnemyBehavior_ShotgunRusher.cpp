#include "EnemyBehavior_ShotgunRusher.h"
#include "Enemy.h"
#include <cmath>

void EnemyBehavior_ShotgunRusher::AfterFSMUpdate(Enemy& enemy, EnemyAICommand& cmd, float /*dt*/)
{
	if (!cmd.lookAt) return;

	using K4E::Vector3;
	const Vector3 selfPos = enemy.GetCenterPosition();
	Vector3 toTarget = *cmd.lookAt - selfPos;
	toTarget.y = 0.0f;

	const float dist = Vector3::LengthXZ(toTarget);
	const Vector3 dirTo = Vector3::NormalizeXZ(toTarget);
	if (dist <= 1e-6f) return;

	const float rushUntil = enemy.GetAttackRange() * 0.80f;
	if (dist > rushUntil)
	{
		cmd.stopMove = false;
		cmd.moveGoal.reset();
		cmd.moveDir = dirTo;
		cmd.moveSpeed = enemy.GetMoveSpeed() * 1.20f;
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