#include "EnemyBehavior_Scout.h"
#include "Enemy.h"
#include <cmath>

using namespace Ken4lowEngine;

void EnemyBehavior_Scout::AfterFSMUpdate(Enemy& enemy, EnemyAICommand& cmd, float /*dt*/)
{
	if (!cmd.lookAt) return;

	using K4E::Vector3;
	const Vector3 selfPos = enemy.GetCenterPosition();
	Vector3 toTarget = *cmd.lookAt - selfPos;
	toTarget.y = 0.0f;

	const Vector3 dirTo = Vector3::NormalizeXZ(toTarget);
	if (Vector3::LengthXZ(dirTo) <= 1e-6f) return;

	const Vector3 side = PerpRightXZ(dirTo);

	cmd.moveGoal.reset();
	cmd.moveDir = NormalizeXZ(side + dirTo * 0.10f);
	cmd.moveSpeed = enemy.GetMoveSpeed() * 1.25f;
}