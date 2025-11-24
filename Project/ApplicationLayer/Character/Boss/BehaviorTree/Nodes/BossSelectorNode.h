#pragma once
#include "IBossBehaviorNode.h"

#include <vector>
#include <memory>

/// -------------------------------------------------------------
///				　セレクタノードクラス
/// -------------------------------------------------------------
class BossSelectorNode : public IBossBehaviorNode
{
public: /// ---------- メンバ関数 ---------- ///

	// 子ノード追加
	void AddChild(std::unique_ptr<IBossBehaviorNode> child) { children_.push_back(std::move(child)); }

	// ノードの実行
	BehaviorStatus Tick(BossEnemy& boss, float deltaTime) override
	{
		for (auto& c : children_)
		{
			BehaviorStatus status = c->Tick(boss, deltaTime);
			if (status == BehaviorStatus::Success || status == BehaviorStatus::Running)
			{
				return status;
			}
		}
		return BehaviorStatus::Failure;
	}

private: /// ---------- メンバ変数 ---------- ///

	// 子ノードリスト
	std::vector<std::unique_ptr<IBossBehaviorNode>> children_;
};

