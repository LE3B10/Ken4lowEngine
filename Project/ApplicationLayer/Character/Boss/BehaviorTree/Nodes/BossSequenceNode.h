#pragma once
#include "IBossBehaviorNode.h"

#include <vector>
#include <memory>

/// -------------------------------------------------------------
///				　シーケンスノードクラス
/// -------------------------------------------------------------
class BossSequenceNode : public IBossBehaviorNode
{
public: /// ---------- メンバ関数 ---------- ///

	// 子ノードを追加
	void AddChild(std::unique_ptr<IBossBehaviorNode> child) { children_.push_back(std::move(child)); }

	// ノードの実行
	BehaviorStatus Tick(BossEnemy& boss, float dt) override
	{
		for (auto& c : children_)
		{
			BehaviorStatus s = c->Tick(boss, dt);
			if (s == BehaviorStatus::Running)
			{
				return BehaviorStatus::Running;
			}
			if (s == BehaviorStatus::Failure)
			{
				return BehaviorStatus::Failure;
			}
		}
		return BehaviorStatus::Success;
	}

private:
	std::vector<std::unique_ptr<IBossBehaviorNode>> children_;
};

