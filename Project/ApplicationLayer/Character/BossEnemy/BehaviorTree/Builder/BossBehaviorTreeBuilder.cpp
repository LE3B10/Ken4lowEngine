#include "BossBehaviorTreeBuilder.h"
#include "BossSelectorNode.h"
#include "BossSequenceNode.h"
#include "BossConditionNode.h"
#include "BossActionNode.h"
#include "BossEnemy.h"

std::unique_ptr<BossBehaviorTree> BossBehaviorTreeBuilder::BuildBehaviorTree(BossType type)
{
    auto tree = std::make_unique<BossBehaviorTree>();

    switch (type)
    {
    case BossType::BossA:
    default:
        BuildBossA(*tree);
        break;
    }

    return tree;
}

void BossBehaviorTreeBuilder::BuildBossA(BossBehaviorTree& tree)
{
    auto root = std::make_unique<BossSelectorNode>();

    // 1. 死亡チェック → 死亡演出へ
    auto deadSeq = std::make_unique<BossSequenceNode>();
    deadSeq->AddChild(std::make_unique<BossActionNode>(
        [](BossEnemy& boss, float /*dt*/) {
            if (!boss.IsDead()) { return BehaviorStatus::Failure; }
            boss.RequestDeadState();
            return BehaviorStatus::Running;
        }
    ));

    // 2. 登場演出（未完了なら優先）
    auto appearSeq = std::make_unique<BossSequenceNode>();
    appearSeq->AddChild(std::make_unique<BossConditionNode>(
        [](BossEnemy& boss) {
            return !boss.IsAppearFinished();
        }
    ));
    appearSeq->AddChild(std::make_unique<BossActionNode>(
        [](BossEnemy& boss, float dt) {
            boss.UpdateAppear(dt); // 登場アニメ&カットイン
            return boss.IsAppearFinished() ? BehaviorStatus::Success : BehaviorStatus::Running;
        }
    ));

    // 3. フェーズ別攻撃（例: HP 50% 以上/未満）
    auto phaseSelector = std::make_unique<BossSelectorNode>();

    // Phase2: 激怒モード
    auto phase2Seq = std::make_unique<BossSequenceNode>();
    phase2Seq->AddChild(std::make_unique<BossConditionNode>(
        [](BossEnemy& boss) {
            return boss.GetHPRate() <= 0.5f;
        }
    ));
    phase2Seq->AddChild(std::make_unique<BossActionNode>(
        [](BossEnemy& boss, float dt) {
            return boss.UpdateRagePhase(dt); // レーザー・召喚など
        }
    ));

    // Phase1: 通常モード
    auto phase1Action = std::make_unique<BossActionNode>(
        [](BossEnemy& boss, float dt) {
            return boss.UpdateNormalPhase(dt); // 突進・近距離攻撃中心
        }
    );

    phaseSelector->AddChild(std::move(phase2Seq));
    phaseSelector->AddChild(std::move(phase1Action));

    // セレクタに登録順：死亡 > 登場 > 戦闘フェーズ
    root->AddChild(std::move(deadSeq));
    root->AddChild(std::move(appearSeq));
    root->AddChild(std::move(phaseSelector));

    tree.SetRootNode(std::move(root));
}
