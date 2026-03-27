#include "EnemyBehavior_BurstTrooper.h"
#include "Enemy.h"
#include <cmath>

using namespace Ken4lowEngine;

void EnemyBehavior_BurstTrooper::AfterFSMUpdate(Enemy& enemy, EnemyAICommand& cmd, float /*dt*/)
{
	if (!cmd.lookAt) return;

	using K4E::Vector3;
	const Vector3 selfPos = enemy.GetCenterPosition();
	Vector3 toTarget = *cmd.lookAt - selfPos;
	toTarget.y = 0.0f;

	const float dist = Vector3::LengthXZ(toTarget);
	const Vector3 dirTo = Vector3::NormalizeXZ(toTarget);
	if (dist <= 1e-6f) return;

	const float preferMidMin = enemy.GetAttackRange() * 0.55f;
	const float preferMidMax = enemy.GetAttackRange() * 0.95f;

	if (dist < preferMidMin)
	{
		cmd.moveGoal.reset();
		cmd.moveDir = dirTo * -1.0f;
		cmd.moveSpeed = enemy.GetMoveSpeed() * 0.95f;
	}
	else if (dist > preferMidMax)
	{
		cmd.moveGoal.reset();
		cmd.moveDir = dirTo;
		cmd.moveSpeed = enemy.GetMoveSpeed() * 0.90f;
	}
}