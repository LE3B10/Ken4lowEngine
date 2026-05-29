#pragma once
#include "IBTNode.h"

#include <functional>
#include <utility>

/// -------------------------------------------------------------
///				ビヘイビアツリーの条件ノード
/// -------------------------------------------------------------
class BTConditionNode : public IBTNode
{
public: /// ---------- 基本構造 ---------- ///

	// 条件関数の型
	using ConditionFunc = std::function<bool()>;

	// 条件関数を受け取るコンストラクタ
	explicit BTConditionNode(ConditionFunc func) : func_(std::move(func)) {}

	// ビヘイビアツリーの更新
	BehaviorStatus Tick(float) override
	{
		// 条件関数を呼び出して、成功なら Success、失敗なら Failure を返す
		return func_() ? BehaviorStatus::Success : BehaviorStatus::Failure;
	}

private: /// ---------- メンバ変数 ---------- ///

	// 条件関数
	ConditionFunc func_;
};

