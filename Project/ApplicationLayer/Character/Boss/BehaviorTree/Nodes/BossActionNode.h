#pragma once
#include "IBossBehaviorNode.h"

#include <functional>

/// -------------------------------------------------------------
///				　    アクションノードクラス
/// -------------------------------------------------------------
class BossActionNode : public IBossBehaviorNode
{
public: /// ---------- メンバ関数 ---------- ///

	// エイリアス
	using Function = std::function<BehaviorStatus(BossEnemy&, float)>;

	// コンストラクタ
	explicit BossActionNode(Function function) : function_(std::move(function)) {}

	// ノードの実行
	BehaviorStatus Tick(BossEnemy& boss, float deltaTime) override { return function_(boss, deltaTime); }

private: /// ---------- メンバ変数 ---------- ///

	Function function_;
};

