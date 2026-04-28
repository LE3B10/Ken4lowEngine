#pragma once

/// ---------- 前方宣言 ---------- ///
class Enemy;

/// -------------------------------------------------------------
///					敵の状態インターフェース
/// -------------------------------------------------------------
class IEnemyState
{
public: /// ---------- 純粋仮想関数 ---------- ///

	// デストラクタ
	virtual ~IEnemyState() = default;

	// 状態に入る
	virtual void Enter(Enemy& enemy) = 0;

	// 状態更新
	virtual void Update(Enemy& enemy, float deltaTime) = 0;

	// 状態から出る
	virtual void Exit(Enemy& enemy) = 0;

	// 状態名を取得
	virtual const char* GetStateName() const = 0;
};

