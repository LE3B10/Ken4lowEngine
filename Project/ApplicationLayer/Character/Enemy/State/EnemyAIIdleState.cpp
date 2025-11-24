#include "EnemyAIIdleState.h"
#include "Enemy.h"

void EnemyAIIdleState::Enter(Enemy* enemy)
{
	using AIState = Enemy::AIState;

	// 初期状態はIdleに設定
	enemy->SetState(AIState::Idle);
}

void EnemyAIIdleState::Update(Enemy* enemy, float deltaTime)
{
	(void)deltaTime;
	(void)enemy;
}

void EnemyAIIdleState::Exit(Enemy* enemy)
{
	(void)enemy;
}
