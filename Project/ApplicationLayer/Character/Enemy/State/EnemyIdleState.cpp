#include "EnemyIdleState.h"
#include "Enemy.h"

using namespace Ken4lowEngine;

void EnemyIdleState::Enter(Enemy& enemy)
{
	// 待機状態では移動しない
	enemy.StopMove();
}

void EnemyIdleState::Update(Enemy& enemy, float deltaTime)
{
	// deltaTime は未使用
	(void)deltaTime;

	// 待機状態では移動しない
	enemy.StopMove();

	// ターゲットがいない場合は何もしない
	if(!enemy.HasTarget()) return;

	// ターゲットの位置を取得
	const Vector3 targetPos = enemy.GetTargetPosition();

	// ターゲットが射程内にいる場合は射撃状態に遷移
	const float distToTarget = enemy.GetDistanceToTarget();

	// ターゲットが視認できる場合は戦闘移動状態に遷移
	if(enemy.CanSeeTargetPublic(targetPos, distToTarget))
	{
		enemy.RememberLastSeenTarget(targetPos);
		enemy.ChangeStateToCombatMove();
		return;
	}
}

void EnemyIdleState::Exit(Enemy& enemy)
{
	(void)enemy;
}
