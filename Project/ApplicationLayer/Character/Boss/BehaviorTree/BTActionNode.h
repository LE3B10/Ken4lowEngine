#pragma once
#include "IBTNode.h"

#include <functional>
#include <utility>

/// -------------------------------------------------------------
///				ビヘイビアツリーのアクションノード
/// -------------------------------------------------------------
class BTActionNode : public IBTNode
{
public: /// ---------- 基本構造 ---------- ///

	// アクション関数の型
	using ActionFunc = std::function<BehaviorStatus(float)>;

	// アクション関数を受け取るコンストラクタ
	explicit BTActionNode(ActionFunc func) : func_(std::move(func)) {}

	// ビヘイビアツリーの更新
	BehaviorStatus Tick(float dt) override
	{
		// アクション関数を呼び出して、その結果を返す
		return func_(dt);
	}

private: /// ---------- メンバ変数 ---------- ///

	// アクション関数
	ActionFunc func_;
};

