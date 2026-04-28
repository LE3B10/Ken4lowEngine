#pragma once
#include "IEnemyState.h"
#include <Vector3.h>

/// -------------------------------------------------------------
///						敵の戦闘移動状態
/// -------------------------------------------------------------
class EnemyCombatMoveState : public IEnemyState
{
public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	~EnemyCombatMoveState() override = default;

	// 状態に入る
	void Enter(Enemy& enemy) override;

	// 状態更新
	void Update(Enemy& enemy, float deltaTime) override;

	// 状態から出る
	void Exit(Enemy& enemy) override;

	// 状態名を取得
	const char* GetStateName() const override { return "CombatMove"; }

private: /// ---------- メンバ変数 ---------- ///

	float losRepositionTimer_ = 0.0f;
	float retreatRepathTimer_ = 0.0f;
	float coverStayTimer_ = 0.0f;
	Ken4lowEngine::Vector3 retreatTarget_{ 0.0f, 0.0f, 0.0f };
	bool hasRetreatTarget_ = false;
};

