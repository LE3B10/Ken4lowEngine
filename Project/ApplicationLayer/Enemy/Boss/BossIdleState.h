#pragma once
#include "IEnemyState.h"
class BossIdleState : public IEnemyState
{
public: /// ---------- メンバ関数 ---------- ///

	// ステートの開始処理
	void Enter(Enemy* enemy) override;

	// ステートの更新処理
	void Update(Enemy* enemy) override;

	// ステートの終了処理
	void Exit(Enemy* enemy) override;
};

