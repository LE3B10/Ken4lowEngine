#include "EnemyDeadState.h"
#include "Enemy.h"

void EnemyDeadState::Enter(Enemy& enemy)
{
	enemy.StopMove();
	enemy.PlayDeadAnimation();
}

void EnemyDeadState::Update(Enemy& enemy, float deltaTime)
{
	(void)deltaTime;
	enemy.StopMove();
	enemy.PlayDeadAnimation();
}

void EnemyDeadState::Exit(Enemy& enemy)
{
	(void)enemy;
}
