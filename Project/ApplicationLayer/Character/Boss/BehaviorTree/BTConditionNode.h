#pragma once
#include "IBTNode.h"

#include <functional>
#include <utility>

class BTConditionNode : public IBTNode
{
public:
	using ConditionFunc = std::function<bool()>;

	explicit BTConditionNode(ConditionFunc func) : func_(std::move(func)) {}
	BTNodeResult Tick(float) override
	{
		return func_() ? BTNodeResult::Success : BTNodeResult::Failure;
	}

private:
	ConditionFunc func_;
};

