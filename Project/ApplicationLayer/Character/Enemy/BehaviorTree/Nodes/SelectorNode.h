#pragma once
#include "IBehaviorNode.h"

#include <vector>
#include <memory>

/// -------------------------------------------------------------
///				　セレクタノードクラス
/// -------------------------------------------------------------
class SelectorNode : public IBehaviorNode
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

			// 子ノードがSuccessなら、セレクタ全体もSuccessで終了
			if (status == BehaviorStatus::Success)
			{
				return BehaviorStatus::Success;
			}

			// 子ノードがRunningなら、セレクタ全体もRunningで終了
			if (status == BehaviorStatus::Running)
			{
				return BehaviorStatus::Running;
			}
		}

		// すべての子ノードがFailureした場合、セレクタ全体もFailure
		return BehaviorStatus::Failure;
	}

private: /// ---------- メンバ変数 ---------- ///

	// 子ノードリスト
	std::vector<std::unique_ptr<IBehaviorNode>> children_;
};

