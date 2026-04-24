#pragma once
#include "IEnemyState.h"

/// -------------------------------------------------------------
///							敵の索敵状態
/// -------------------------------------------------------------
class EnemySearchState : public IEnemyState
{
public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	~EnemySearchState() override = default;

	// 状態に入る
	void Enter(Enemy& enemy) override;

	// 状態更新
	void Update(Enemy& enemy, float deltaTime) override;

	// 状態から出る
	void Exit(Enemy& enemy) override;

	// 状態名を取得
	const char* GetStateName() const override { return "Search"; }

private: /// ---------- メンバ変数 ---------- ///

	float searchTimer_ = 0.0f; // 索敵状態の滞在時間を計測するタイマー
};

