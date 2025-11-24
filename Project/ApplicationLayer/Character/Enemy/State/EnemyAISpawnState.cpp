#include "EnemyAISpawnState.h"
#include "Enemy.h"
#include "EnemyAIWanderState.h"

void EnemyAISpawnState::Enter(Enemy* enemy)
{
	using AIState = Enemy::AIState;

	// 初期状態はSpawnに設定
	enemy->SetState(AIState::Spawn);
	enemy->ResetStateTimer();
	enemy->SetActive(false);
}

void EnemyAISpawnState::Update(Enemy* enemy, float deltaTime)
{
	using AIState = Enemy::AIState;

	// 出現待機時間をカウントアップ
	float stateTimer = enemy->GetStateTimer();

	stateTimer += deltaTime;

	// まだ出現待機時間に達していなければ待機
	if (stateTimer < enemy->GetDelayDuration())
	{
		enemy->SetActive(true);       // ここでメンバ isActive_ を true にする
		stateTimer = 0.0f;            // タイマーリセット
		enemy->SetState(AIState::Wander);
		enemy->ChangeState(std::make_unique<EnemyAIWanderState>()); // unique_ptr は make_unique で
	}
	enemy->SetStateTimer(stateTimer);
}

void EnemyAISpawnState::Exit(Enemy* enemy)
{
	(void)enemy;
}
