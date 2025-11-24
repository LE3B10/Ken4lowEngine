#pragma once
#include <BehaviorStatus.h>

/// ---------- 前方宣言 ---------- ///
class Enemy;

/// ---------------------------------------------
///		 ビヘイビアノードインターフェース
/// ---------------------------------------------
class IBehaviorNode
{
public: /// ---------- メンバ関数 ---------- ///

	// 仮想デストラクタ
	virtual ~IBehaviorNode() = default;

	// ノードの実行
	virtual BehaviorStatus Tick(Enemy& enemy, float deltaTime) = 0;
};

