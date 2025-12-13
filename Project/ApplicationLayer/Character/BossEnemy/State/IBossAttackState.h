#pragma once
#include <BehaviorStatus.h>

/// ---------- 前方宣言 ---------- ///
class BossEnemy;

/// -------------------------------------------------------------
///		　		ボス攻撃状態インターフェース
/// -------------------------------------------------------------
class IBossAttackState
{
public: /// ---------- 純粋仮想関数 ---------- ///

	// 仮想デストラクタ
	virtual ~IBossAttackState() = default;

	// 状態開始時処理
	virtual void OnEnter(BossEnemy* boss) = 0;

	// 状態の更新。BehaviorStatus で「まだ続く」「終わった」を返す
	virtual BehaviorStatus Update(BossEnemy* boss, float deltaTime) = 0;

	// 状態終了時処理
	virtual void OnExit(BossEnemy* boss) = 0;
};

