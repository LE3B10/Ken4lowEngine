#pragma once
#include "BehaviorTree.h"
#include "EnemyType.h"

#include <memory>

/// ---------- 前方宣言 ---------- ///
class Enemy;

/// -------------------------------------------------------------
///				　敵行動ビヘイビアツリービルダークラス
/// -------------------------------------------------------------
class EnemyBehaviorTreeBuilder
{
public: /// ---------- メンバ関数 ---------- ///

	// ビヘイビアツリー構築
	static std::unique_ptr<BehaviorTree> BuildBehaviorTree(EnemyType type);

private: /// ---------- メンバ関数 ---------- ///

	// 近接攻撃雑魚ビヘイビアツリー構築
	static void BuildMeleeGruntTree(BehaviorTree& tree);
};

