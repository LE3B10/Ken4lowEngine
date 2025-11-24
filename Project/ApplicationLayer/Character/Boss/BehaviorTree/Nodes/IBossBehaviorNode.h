#pragma once
#include <BehaviorStatus.h>

/// ---------- 前方宣言 ---------- ///
class BossEnemy;

/// -------------------------------------------------------------
///			ボス行動ビヘイビアノードインターフェース
/// -------------------------------------------------------------
class IBossBehaviorNode
{
public: /// ---------- 純粋仮想関数 ---------- ///

	// 仮想デストラクタ
	virtual ~IBossBehaviorNode() = default;

	// ノードの実行
	virtual BehaviorStatus Tick(BossEnemy& enemy, float deltaTime) = 0;
};

