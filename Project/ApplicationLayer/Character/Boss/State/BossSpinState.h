#pragma once
#include "IBossAttackState.h"

/// -------------------------------------------------------------
///				　	ボス回転攻撃状態クラス
/// -------------------------------------------------------------
class BossSpinState : public IBossAttackState
{
public: /// ---------- メンバ関数 ---------- ///

	// 状態開始時処理
	void OnEnter(BossEnemy* boss) override;

	// 状態の更新。BehaviorStatus で「まだ続く」「終わった」を返す
	BehaviorStatus Update(BossEnemy* boss, float deltaTime) override;

	// 状態終了時処理
	void OnExit(BossEnemy* boss) override;

private: /// ---------- メンバ変数 ---------- ///

	float elapsed_ = 0.0f;
	float startYaw_ = 0.0f;
};

