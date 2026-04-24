#include "EnemySearchState.h"
#include "Enemy.h"
#include <cmath>

using namespace Ken4lowEngine;

namespace
{
	float DistXZ(const Vector3& a, const Vector3& b)
	{
		const float dx = a.x - b.x;
		const float dz = a.z - b.z;
		return std::sqrt(dx * dx + dz * dz);
	}
}

void EnemySearchState::Enter(Enemy& enemy)
{
	searchTimer_ = 0.0f;
	enemy.PlaySearchAnimation(enemy.GetSearchMoveSpeed());
}

void EnemySearchState::Update(Enemy& enemy, float deltaTime)
{
	searchTimer_ += deltaTime;

	if (!enemy.HasTarget())
	{
		enemy.ChangeStateToIdle();
		return;
	}

	const Vector3 targetPos = enemy.GetTargetPosition();
	const float distToTarget = enemy.GetDistanceToTarget();
	if (enemy.CanSeeTargetPublic(targetPos, distToTarget))
	{
		enemy.RememberLastSeenTarget(targetPos);
		enemy.ChangeStateToCombatMove();
		return;
	}

	const Vector3 lastSeen = enemy.GetLastSeenTargetPosition();
	const float distToLastSeen = DistXZ(enemy.GetCenterPosition(), lastSeen);

	if (distToLastSeen > 1.2f)
	{
		enemy.MoveToLastSeen(enemy.GetSearchMoveSpeed());
		enemy.PlaySearchAnimation(enemy.GetSearchMoveSpeed());
	}
	else
	{
		enemy.UpdateStrafeDecision(deltaTime);
		enemy.MoveStrafeAround(lastSeen, enemy.GetCurrentStrafeSign(), enemy.GetSearchMoveSpeed() * 0.7f);
		enemy.PlaySearchAnimation(enemy.GetSearchMoveSpeed() * 0.7f);
	}

	if (searchTimer_ >= enemy.GetSearchDuration())
	{
		enemy.ChangeStateToIdle();
		return;
	}
}

void EnemySearchState::Exit(Enemy& enemy)
{
	(void)enemy;
}
