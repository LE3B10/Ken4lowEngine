#pragma once
#include "IBossAttackState.h"
#include <Vector3.h>

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///				　	ボス突進攻撃状態クラス
/// -------------------------------------------------------------
class BossRushState : public IBossAttackState
{
public: /// ---------- メンバ関数 ---------- ///

	// 状態開始時処理
	void OnEnter(BossEnemy* boss) override;

	// 状態の更新。BehaviorStatus で「まだ続く」「終わった」を返す
	BehaviorStatus Update(BossEnemy* boss, float deltaTime) override;

	// 状態終了時処理
	void OnExit(BossEnemy* boss) override;

private: /// ---------- メンバ変数 ---------- ///

	float   elapsed_ = 0.0f;
	K4E::Vector3 dir_{ 0.0f, 0.0f, 0.0f };

	float   moveDistance_ = 0.0f; // この攻撃で進んでいい最大距離
	float   moved_ = 0.0f; // すでに進んだ距離
};

