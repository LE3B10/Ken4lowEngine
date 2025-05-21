#pragma once
#include "IEnemyState.h"
class EnemyAttackState : public IEnemyState
{
public:
    void Enter(Enemy* enemy) override;
    void Update(Enemy* enemy) override;
    void Exit(Enemy* enemy) override;
};

