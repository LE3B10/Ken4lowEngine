#pragma once

/// ---------- 前方宣言 ---------- ///
class Enemy;

/// -------------------------------------------------------------
///				　		敵の状態インターフェース
/// -------------------------------------------------------------
class IEnemyState
{
public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	virtual ~IEnemyState() = default;

	// ステートの開始、更新、終了処理
	virtual void Enter(Enemy* enemy) = 0;
	virtual void Update(Enemy* enemy) = 0;
	virtual void Exit(Enemy* enemy) = 0;
};

