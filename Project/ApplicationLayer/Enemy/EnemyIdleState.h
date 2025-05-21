#pragma once
#include "IEnemyState.h"

class EnemyIdleState : public IEnemyState
{
public:
	void Enter(Enemy* enemy) override;
	void Update(Enemy* enemy) override;
	void Exit(Enemy* enemy) override;

private:
	float idleTimer_ = 0.0f; // 最低実行時間（秒）
};

