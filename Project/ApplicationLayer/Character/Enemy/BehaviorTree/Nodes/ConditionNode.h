#pragma once
#include "IBehaviorNode.h"

#include <functional>

/// -------------------------------------------------------------
///				　         条件ノードクラス
/// -------------------------------------------------------------
class ConditionNode : public IBehaviorNode
{
public: /// ---------- メンバ関数 ---------- ///

	// Function型の条件関数を受け取るコンストラクタ
	using Function = std::function<bool(Enemy&)>;

	// コンストラクタ
	explicit ConditionNode(Function function) : function_(std::move(function)) {}

	// ノードの実行
	BehaviorStatus Tick(Enemy& enemy, float /*deltaTime */) override
	{
		// 条件関数を評価して、結果に応じてSuccessかFailureを返す
		return function_(enemy) ? BehaviorStatus::Success : BehaviorStatus::Failure;
	}

private: /// ---------- メンバ変数 ---------- ///

	// 条件関数
	Function function_;
};

