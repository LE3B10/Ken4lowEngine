#pragma once
#include "IEnemyState.h"

/// -------------------------------------------------------------
///				　		敵のアイドル状態クラス
/// -------------------------------------------------------------
class EnemyIdleState : public IEnemyState
{
public: /// ---------- メンバ関数 ---------- ///

	// 開始処理
	void Enter(Enemy* enemy) override;

	// 更新処理
	void Update(Enemy* enemy) override;

	// 終了処理
	void Exit(Enemy* enemy) override;

private: /// ---------- メンバ変数 ---------- ///

	float idleTimer_ = 0.0f; // 最低実行時間（秒）
};

