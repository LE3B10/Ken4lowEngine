#pragma once
#include "IBehaviorNode.h"

#include <memory>

/// ---------- 前方宣言 ---------- ///
class Enemy;

/// -------------------------------------------------------------
///				　ビヘイビアツリークラス
/// -------------------------------------------------------------
class BehaviorTree
{
public: /// ---------- メンバ関数 ---------- /// 

	// ルートノード設定
	void SetRootNode(std::unique_ptr<IBehaviorNode> rootNode) { rootNode_ = std::move(rootNode); }

	// ビヘイビアツリーの更新
	void Tick(Enemy& enemy, float deltaTime) { if (rootNode_) rootNode_->Tick(enemy, deltaTime); }

private: /// ---------- メンバ変数 ---------- ///

	// ルートノード
	std::unique_ptr<IBehaviorNode> rootNode_;
};

