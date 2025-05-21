#pragma once

class Enemy;

class IEnemyState
{
public:
	virtual ~IEnemyState() = default;

	virtual void Enter(Enemy* enemy) = 0;
	virtual void Update(Enemy* enemy) = 0;
	virtual void Exit(Enemy* enemy) = 0;
};

