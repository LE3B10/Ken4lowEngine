#pragma once
#include "IBossBehaviorNode.h"

#include <functional>

/// -------------------------------------------------------------
///				　     条件ノードクラス
/// -------------------------------------------------------------
class BossConditionNode : public IBossBehaviorNode
{
public: /// ---------- メンバ関数 ---------- ///

	// エイリアス
	using Function = std::function<bool(BossEnemy&)>;

	// コンストラクタ
	explicit BossConditionNode(Function function) : function_(std::move(function)) {}

	// ノードの実行
	BehaviorStatus Tick(BossEnemy& boss, float /*deltaTime*/) override
	{
		// 条件関数を評価して、結果に応じてSuccessかFailureを返す
		return function_(boss) ? BehaviorStatus::Success : BehaviorStatus::Failure;
	}

private: /// ---------- メンバ変数 ---------- ///

	Function function_;
};

