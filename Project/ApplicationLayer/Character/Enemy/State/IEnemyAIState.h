#pragma once

/// ---------- 前方宣言 ---------- ///
class Enemy;

/// -------------------------------------------------------------
///				　	敵AI状態インターフェース
/// -------------------------------------------------------------
class IEnemyAIState
{
public: /// ---------- メンバ関数 ---------- ///

	// 仮想デストラクタ
	virtual ~IEnemyAIState() = default;

	// 状態に入ったときの処理
	virtual void Enter(Enemy* enemy) = 0;

	// 状態の更新
	virtual void Update(Enemy* enemy, float deltaTime) = 0;

	// 状態を出るときの処理
	virtual void Exit(Enemy* enemy) = 0;

	// 状態名取得
	virtual const char* GetName() const = 0;
};

