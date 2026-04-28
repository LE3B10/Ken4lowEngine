#pragma once
#include "IEnemyState.h"

/// -------------------------------------------------------------
///						　敵の待機状態
/// -------------------------------------------------------------
class EnemyIdleState : public IEnemyState
{
public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	~EnemyIdleState() override = default;

	// 状態に入る
	void Enter(Enemy& enemy) override;

	// 状態更新
	void Update(Enemy& enemy, float deltaTime) override;

	// 状態から出る
	void Exit(Enemy& enemy) override;

	// 状態名を取得
	const char* GetStateName() const override { return "Idle"; }
};

