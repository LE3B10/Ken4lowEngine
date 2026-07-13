#pragma once
#include "IBTNode.h"

#include <functional>
#include <utility>

/// -------------------------------------------------------------
///				ビヘイビアツリーのアクションノード
/// -------------------------------------------------------------
class BTActionNode : public IBTNode
{
public:

	/// ノードが実行する処理の型を表す。
	using ActionFunc = std::function<BehaviorStatus(float)>;

	/// 実行する処理を受け取ってアクションノードを構築する。
	explicit BTActionNode(ActionFunc func) : func_(std::move(func)) {}

	/// 登録された処理を実行し、その状態を親ノードへ返す。
	BehaviorStatus Tick(float dt) override
	{
		return func_(dt);
	}

private:
	ActionFunc func_;
};
