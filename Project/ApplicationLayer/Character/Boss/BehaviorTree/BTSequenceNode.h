#pragma once
#include "IBTNode.h"

#include <memory>
#include <vector>

/// -------------------------------------------------------------
///				ビヘイビアツリーのシーケンスノード
/// -------------------------------------------------------------
class BTSequenceNode : public IBTNode
{
public: /// ---------- 基本構造 ---------- ///

	// デストラクタは仮想関数にしておく
	~BTSequenceNode() override = default;

	// 子ノードを追加
	void AddChild(std::unique_ptr<IBTNode> child)
	{
		children_.push_back(std::move(child));
	}

	// ビヘイビアツリーの更新
	BehaviorStatus Tick(float deltaTime) override
	{
		// 子ノードを順番に更新
		for (auto& child : children_)
		{
			// 子ノードの更新結果を取得
			const BehaviorStatus result = child->Tick(deltaTime);

			// 子ノードが失敗したらシーケンス全体も失敗
			if (result != BehaviorStatus::Success)
			{
				return result;
			}
		}

		// 全ての子ノードが成功したらシーケンス全体も成功
		return BehaviorStatus::Success;
	}

private: /// ---------- メンバ変数 ---------- ///

	// 子ノードのリスト
	std::vector<std::unique_ptr<IBTNode>> children_;
};
