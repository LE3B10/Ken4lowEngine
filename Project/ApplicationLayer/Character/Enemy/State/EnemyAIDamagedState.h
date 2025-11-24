#pragma once
#include "IEnemyAIState.h"
#include <Vector4.h>

/// -------------------------------------------------------------
///				　		敵AIダメージ状態クラス
/// -------------------------------------------------------------
class EnemyAIDamagedState : public IEnemyAIState
{
public: /// ---------- メンバ関数 ---------- ///

	// 状態に入ったときの処理
	void Enter(Enemy* enemy) override;

	// 状態の更新
	void Update(Enemy* enemy, float deltaTime) override;

	// 状態を出るときの処理
	void Exit(Enemy* enemy) override;

	// 状態名取得
	const char* GetName() const override { return "Damaged"; }

private: /// ---------- メンバ関数 ---------- ///

	// 死亡演出を開始
	void StartDeathSequence(Enemy* enemy);

	// 全部位に色を適用する
	void ApplyColorToAll(Enemy* enemy, const Vector4& color); // body_ と parts_ に一括適用
};

