#pragma once
#include "IEnemyState.h"

/// -------------------------------------------------------------
///				　		敵の追跡状態クラス
/// -------------------------------------------------------------
class EnemyChaseState : public IEnemyState
{
public: /// ---------- メンバ関数 ---------- ///

	// 開始処理
	void Enter(Enemy* enemy) override;

	// 更新処理
	void Update(Enemy* enemy) override;

	// 終了処理
	void Exit(Enemy* enemy) override;
};

