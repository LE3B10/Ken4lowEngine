#pragma once
#include "IBTNode.h"

#include <functional>
#include <utility>

class BTActionNode : public IBTNode
{
public:
	using ActionFunc = std::function<BTNodeResult(float)>;

	explicit BTActionNode(ActionFunc func) : func_(std::move(func)) {}
	BTNodeResult Tick(float dt) override
	{
		return func_(dt);
	}

private:
	ActionFunc func_;
};

