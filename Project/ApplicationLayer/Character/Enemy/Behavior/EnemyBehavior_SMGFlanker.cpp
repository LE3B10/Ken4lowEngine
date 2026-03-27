#include "EnemyBehavior_SMGFlanker.h"
#include "Enemy.h"
#include <cmath>
#include "Vector3.h"

using namespace Ken4lowEngine;

void EnemyBehavior_SMGFlanker::AfterFSMUpdate(Enemy& enemy, EnemyAICommand& cmd, float /*dt*/)
{
	if (enemy.GetStateId() != EnemyStateId::Attack) return;
	if (!cmd.lookAt) return;

	const Vector3 selfPos = enemy.GetCenterPosition();
	Vector3 toTarget = *cmd.lookAt - selfPos;
	toTarget.y = 0.0f;

	const Vector3 dirTo = NormalizeXZ(toTarget);
	if (Vector3::LengthXZ(dirTo) <= 1e-6f) return;

	const Vector3 side = Vector3::PerpRightXZ(dirTo);

	Vector3 outDir = side;
	if (cmd.moveDir)
	{
		Vector3 current = *cmd.moveDir;
		current.y = 0.0f;
		outDir = NormalizeXZ(current + side * 0.85f + dirTo * 0.20f);
	}
	else
	{
		outDir = NormalizeXZ(side + dirTo * 0.25f);
	}

	cmd.moveGoal.reset();
	cmd.moveDir = outDir;
	cmd.moveSpeed = enemy.GetMoveSpeed() * 1.10f;
}