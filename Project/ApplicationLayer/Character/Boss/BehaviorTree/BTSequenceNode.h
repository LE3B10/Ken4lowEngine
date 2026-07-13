#pragma once
#include "IBTNode.h"

#include <memory>
#include <vector>

/// -------------------------------------------------------------
///				ビヘイビアツリーのシーケンスノード
/// -------------------------------------------------------------
class BTSequenceNode : public IBTNode
{
public:

	~BTSequenceNode() override = default;

	/// 実行順の末尾へ有効な子ノードを追加する。
	void AddChild(std::unique_ptr<IBTNode> child)
	{
		if (child)
		{
			children_.push_back(std::move(child));
		}
	}

	/// 子を順番に実行し、失敗または実行中ならその時点の結果を返す。
	BehaviorStatus Tick(float deltaTime) override
	{
		for (auto& child : children_)
		{
			const BehaviorStatus result = child->Tick(deltaTime);
			if (result != BehaviorStatus::Success)
			{
				return result;
			}
		}

		return BehaviorStatus::Success;
	}

private:
	std::vector<std::unique_ptr<IBTNode>> children_;
};
