#pragma once
#include "IBTNode.h"

#include <memory>
#include <vector>

/// -------------------------------------------------------------
///				ビヘイビアツリーのセレクタノード
/// -------------------------------------------------------------
class BTSelectorNode : public IBTNode
{
public:

	~BTSelectorNode() override = default;

	/// 評価順の末尾へ有効な子ノードを追加する。
	void AddChild(std::unique_ptr<IBTNode> child)
	{
		if (child)
		{
			children_.push_back(std::move(child));
		}
	}

	/// 子を順番に評価し、最初の成功または実行中の結果を返す。
	BehaviorStatus Tick(float deltaTime) override
	{
		for (auto& child : children_)
		{
			const BehaviorStatus result = child->Tick(deltaTime);
			if (result == BehaviorStatus::Success || result == BehaviorStatus::Running)
			{
				return result;
			}
		}

		return BehaviorStatus::Failure;
	}

private:
	std::vector<std::unique_ptr<IBTNode>> children_;
};
