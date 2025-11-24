#pragma once
#include <memory>
#include "BossBehaviorTree.h"
#include "BossType.h"

class BossBehaviorTreeBuilder
{
public: /// ---------- メンバ関数 ---------- ///

    // ビヘイビアツリー構築
    static std::unique_ptr<BossBehaviorTree> BuildBehaviorTree(BossType type);

private: /// ---------- メンバ関数 ---------- ///

    // 最初のボス
    static void BuildBossA(BossBehaviorTree& tree);
};

