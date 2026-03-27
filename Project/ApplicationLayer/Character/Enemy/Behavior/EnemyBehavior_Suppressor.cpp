#include "EnemyBehavior_Suppressor.h"
#include "Enemy.h"
#include <cmath>

void EnemyBehavior_Suppressor::AfterFSMUpdate(Enemy& enemy, EnemyAICommand& cmd, float /*dt*/)
{
	if (!cmd.lookAt) return;

	using K4E::Vector3;
	const Vector3 selfPos = enemy.GetCenterPosition();
	Vector3 toTarget = *cmd.lookAt - selfPos;
	toTarget.y = 0.0f;

	const Vector3 dirTo = Vector3::NormalizeXZ(toTarget);
	if (Vector3::LengthXZ(dirTo) <= 1e-6f) return;

	const Vector3 side = PerpRightXZ(dirTo);

	if (!cmd.fireAt)
	{
		cmd.moveGoal.reset();
		cmd.moveDir = NormalizeXZ(side + dirTo * 0.15f);
		cmd.moveSpeed = enemy.GetMoveSpeed() * 0.90f;
	}
}