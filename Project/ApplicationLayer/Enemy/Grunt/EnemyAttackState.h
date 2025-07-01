#pragma once
#include "IEnemyState.h"

/// -------------------------------------------------------------
///				　		敵の攻撃状態クラス
/// -------------------------------------------------------------
class EnemyAttackState : public IEnemyState
{
public: /// ---------- メンバ関数 ---------- ///

	// 開始処理
	void Enter(Enemy* enemy) override;

	// 更新処理
	void Update(Enemy* enemy) override;

	// 終了処理
	void Exit(Enemy* enemy) override;
};

