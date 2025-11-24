#pragma once
#include "IEnemyAIState.h"

/// -------------------------------------------------------------
///				　		敵AI徘徊状態クラス
/// -------------------------------------------------------------
class EnemyAIWanderState : public IEnemyAIState
{
public: /// ---------- メンバ関数 ---------- ///

	// 状態に入ったときの処理
	void Enter(Enemy* enemy) override;

	// 状態の更新
	void Update(Enemy* enemy, float deltaTime) override;

	// 状態を出るときの処理
	void Exit(Enemy* enemy) override;

	// 状態名取得
	const char* GetName() const override { return "Wander"; }

private: /// ---------- メンバ関数 ---------- ///

	// 新しい徘徊方向をランダムに決定する
	void PickNewWanderDirection(Enemy* enemy);
};

