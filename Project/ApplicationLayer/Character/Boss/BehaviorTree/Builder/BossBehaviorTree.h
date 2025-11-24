#pragma once
#include <BehaviorStatus.h>
#include "IBossBehaviorNode.h"

#include <memory>

/// ---------- 前方宣言 ---------- ///
class BossEnemy;

/// -------------------------------------------------------------
///				　ボス行動ビヘイビアツリークラス
/// -------------------------------------------------------------
class BossBehaviorTree
{
public: /// ---------- メンバ関数 ---------- ///

	// ルートノード設定
	void SetRootNode(std::unique_ptr<IBossBehaviorNode> rootNode) { rootNode_ = std::move(rootNode); }

	// ビヘイビアツリーの更新
	void Tick(BossEnemy& enemy, float deltaTime) { if (rootNode_) rootNode_->Tick(enemy, deltaTime); }

private: /// ---------- メンバ変数 ---------- ///

	// ルートノード
	std::unique_ptr<IBossBehaviorNode> rootNode_;
};

