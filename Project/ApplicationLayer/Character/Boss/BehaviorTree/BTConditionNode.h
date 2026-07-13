#pragma once
#include "IBTNode.h"

#include <functional>
#include <utility>

/// -------------------------------------------------------------
///				ビヘイビアツリーの条件ノード
/// -------------------------------------------------------------
class BTConditionNode : public IBTNode
{
public:

	/// ノードが評価する条件の型を表す。
	using ConditionFunc = std::function<bool()>;

	/// 評価する条件を受け取って条件ノードを構築する。
	explicit BTConditionNode(ConditionFunc func) : func_(std::move(func)) {}

	/// 条件の真偽をビヘイビアツリーの実行結果へ変換する。
	BehaviorStatus Tick(float) override
	{
		return func_() ? BehaviorStatus::Success : BehaviorStatus::Failure;
	}

private:
	ConditionFunc func_;
};
