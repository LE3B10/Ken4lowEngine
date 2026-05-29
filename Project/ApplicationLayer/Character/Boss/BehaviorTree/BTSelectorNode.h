#pragma once
#include "IBTNode.h"

#include <memory>
#include <vector>

/// -------------------------------------------------------------
///				ビヘイビアツリーのセレクタノード
/// -------------------------------------------------------------
class BTSelectorNode : public IBTNode
{
public: /// ---------- 基本構造 ---------- ///

	// デストラクタは仮想関数にしておく
	virtual ~BTSelectorNode() = default;

	// 子ノードを追加
	void AddChild(std::unique_ptr<IBTNode> child)
	{
		// 子ノードをリストに追加
		children_.push_back(std::move(child));
	}

	// ビヘイビアツリーの更新
	BehaviorStatus Tick(float deltaTime) override
	{
		// 子ノードを順番に更新していき、最初に成功または実行中になったノードの結果を返す
		for (auto& child : children_)
		{
			// 子ノードの更新
			const BehaviorStatus result = child->Tick(deltaTime);

			// 成功または実行中ならその結果を返す
			if (result == BehaviorStatus::Success || result == BehaviorStatus::Running)
			{
				// 成功または実行中のノードが見つかったら、その結果を返す
				return result;
			}
		}

		// 全ての子ノードが失敗した場合は失敗を返す
		return BehaviorStatus::Failure;
	}

private: /// ---------- メンバ変数 ---------- ///

	// 子ノードのリスト
	std::vector<std::unique_ptr<IBTNode>> children_;
};
