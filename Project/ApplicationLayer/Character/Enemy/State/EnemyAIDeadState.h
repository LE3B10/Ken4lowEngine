#pragma once
#include "IEnemyAIState.h"

/// -------------------------------------------------------------
///				　		敵AI死亡状態クラス
/// -------------------------------------------------------------
class EnemyAIDeadState : public IEnemyAIState
{
public: /// ---------- メンバ関数 ---------- ///

	// 状態に入ったときの処理
	void Enter(Enemy* enemy) override;

	// 状態の更新
	void Update(Enemy* enemy, float deltaTime) override;

	// 状態を出るときの処理
	void Exit(Enemy* enemy) override;

	// 状態名取得
	const char* GetName() const override { return "Dead"; }
};

