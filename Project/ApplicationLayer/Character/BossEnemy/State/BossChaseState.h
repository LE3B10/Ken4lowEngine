#pragma once
#include "IBossAttackState.h"

/// -------------------------------------------------------------
///				　	ボス追尾状態クラス
/// -------------------------------------------------------------
class BossChaseState : public IBossAttackState
{
public: /// ---------- メンバ関数 ---------- ///

	// 状態開始時処理
	void OnEnter(BossEnemy* boss) override;

	// 状態の更新。BehaviorStatus で「まだ続く」「終わった」を返す
	BehaviorStatus Update(BossEnemy* boss, float deltaTime) override;

	// 状態終了時処理
	void OnExit(BossEnemy* boss) override;
};

