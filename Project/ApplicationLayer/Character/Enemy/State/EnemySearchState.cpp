#define NOMINMAX
#include "EnemySearchState.h"
#include "Enemy.h"
#include <cmath>
#include <algorithm>

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
	localSweepTimer_ = 0.0f;
	localSwitchTimer_ = 0.0f;
	enemy.PlaySearchAnimation(enemy.GetSearchMoveSpeed());
}

void EnemySearchState::Update(Enemy& enemy, float deltaTime)
{
	searchTimer_ += deltaTime;
	localSwitchTimer_ -= deltaTime;

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
	const bool dangerMode = enemy.IsRetreating();
	const float searchSpeed = dangerMode
		? enemy.GetSearchMoveSpeed() * 0.85f
		: enemy.GetSearchMoveSpeed();

	if (dangerMode && distToTarget < enemy.GetLowHpRetreatDistance())
	{
		Vector3 retreatPos = enemy.GetCenterPosition();
		if (enemy.TryFindCoverPosition(targetPos, true, retreatPos))
		{
			enemy.MoveTowardsPath(retreatPos, enemy.GetRetreatSpeed() * enemy.GetLowHpRetreatSpeedScale(), deltaTime);
		}
		else
		{
			enemy.MoveAwayFrom(targetPos, enemy.GetRetreatSpeed() * enemy.GetLowHpRetreatSpeedScale());
		}
		enemy.PlaySearchAnimation(searchSpeed);
		return;
	}

	if (distToLastSeen > 1.35f)
	{
		localSweepTimer_ = 0.0f;
		enemy.MoveTowardsPath(lastSeen, searchSpeed, deltaTime);
		enemy.PlaySearchAnimation(searchSpeed);
	}
	else
	{
		localSweepTimer_ += deltaTime;
		if (localSwitchTimer_ <= 0.0f)
		{
			const float better = enemy.ChooseBetterStrafeSign(lastSeen, enemy.GetLosProbeDistance() * 0.75f);
			enemy.ForceStrafeSign(better);
			localSwitchTimer_ = 0.45f;
		}

		// 到達後の短い探索移動: オービット + 小さな接近離脱
		const float sweepBias = std::sin(localSweepTimer_ * 2.7f) * 0.35f;
		const float bias = dangerMode ? std::min(sweepBias, -0.25f) : sweepBias;
		enemy.MoveTacticalAround(lastSeen, enemy.GetCurrentStrafeSign(), bias, searchSpeed * 0.9f);
		enemy.PlaySearchAnimation(searchSpeed * 0.9f);
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
