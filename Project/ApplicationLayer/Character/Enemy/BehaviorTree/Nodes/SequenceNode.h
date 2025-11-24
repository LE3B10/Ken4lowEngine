#pragma once
#include "IBehaviorNode.h"

#include <vector>
#include <memory>

/// -------------------------------------------------------------
///				　シーケンスノードクラス
/// -------------------------------------------------------------
class SequenceNode : public IBehaviorNode
{
public: /// ---------- メンバ関数 ---------- ///

	// 子ノード追加
	void AddChild(std::unique_ptr<IBehaviorNode> child) { children_.push_back(std::move(child)); }

	// ノードの実行
	BehaviorStatus Tick(Enemy& enemy, float deltaTime) override
	{
		// 前回の続きから実行したい場合は currentIndex_ をメンバにしてもOK
		for (size_t i = 0; i < children_.size(); ++i)
		{
			// 子ノードを実行
			BehaviorStatus status = children_[i]->Tick(enemy, deltaTime);

			// 子ノードがRunningなら、シーケンス全体もRunningで終了
			if (status == BehaviorStatus::Running)
			{
				return BehaviorStatus::Running;
			}

			// 子ノードがFailureなら、シーケンス全体もFailureで終了
			if (status == BehaviorStatus::Failure)
			{
				return BehaviorStatus::Failure;
			}
		}

		// すべての子ノードがSuccessした場合、シーケンス全体もSuccess
		return BehaviorStatus::Success;
	}

private: /// ---------- メンバ変数 ---------- ///

	// 子ノードリスト
	std::vector<std::unique_ptr<IBehaviorNode>> children_;
};

