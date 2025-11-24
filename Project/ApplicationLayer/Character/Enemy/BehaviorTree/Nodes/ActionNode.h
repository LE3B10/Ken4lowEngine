#pragma once
#include "IBehaviorNode.h"

#include <functional>

/// -------------------------------------------------------------
///				　     アクションノードクラス
/// -------------------------------------------------------------
class ActionNode : public IBehaviorNode
{
public: /// ---------- メンバ関数 ---------- ///

	// Function型のアクション関数を受け取るコンストラクタ
	using Function = std::function<BehaviorStatus(Enemy&, float)>;

	// コンストラクタ
	explicit ActionNode(Function function) : function_(std::move(function)) {}

	// ノードの実行
	BehaviorStatus Tick(Enemy& enemy, float deltaTime) override
	{
		// アクション関数を実行して、その結果を返す
		return function_(enemy, deltaTime);
	}

private: /// ---------- メンバ変数 ---------- ///

	// アクション関数
	Function function_;
};

