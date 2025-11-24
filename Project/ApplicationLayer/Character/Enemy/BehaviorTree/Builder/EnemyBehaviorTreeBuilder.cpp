#include "EnemyBehaviorTreeBuilder.h"
#include "SelectorNode.h"
#include "SequenceNode.h"
#include "ConditionNode.h"
#include "ActionNode.h"
#include "Enemy.h"
#include "EnemyType.h"

/// -------------------------------------------------------------
///				　		ビヘイビアツリー構築
/// -------------------------------------------------------------
std::unique_ptr<BehaviorTree> EnemyBehaviorTreeBuilder::BuildBehaviorTree(EnemyType type)
{
	// ビヘイビアツリーのインスタンスを作成
	std::unique_ptr<BehaviorTree> tree = std::make_unique<BehaviorTree>();

	// root は Selector:
	//  1. 攻撃できるなら攻撃
	//  2. 見えているなら追いかける
	//  3. それ以外はうろうろ

	switch (type)
	{
	case EnemyType::MeleeGrunt: // 近接攻撃を行う雑魚
	default:
		// 近接攻撃をする雑魚のビヘイビアツリー構築
		BuildMeleeGruntTree(*tree);
		break;
	}

	// ビヘイビアツリーを返す
	return tree;
}

/// -------------------------------------------------------------
///			 近接攻撃をする雑魚のビヘイビアツリー構築
/// -------------------------------------------------------------
void EnemyBehaviorTreeBuilder::BuildMeleeGruntTree(BehaviorTree& tree)
{
	// 近接攻撃をする雑魚のビヘイビアツリー構築
		// ルートセレクタノード
	auto rootSelector = std::make_unique<SelectorNode>();

	// シーケンスノード: プレイヤーが攻撃範囲内にいる場合、攻撃する
	auto attackSequence = std::make_unique<SequenceNode>();
	attackSequence->AddChild(std::make_unique<ConditionNode>(
		[](Enemy& enemy) {
			// 攻撃可能か？（距離など）
			return enemy.IsPlayerInAttackRange();
		}
	));
	attackSequence->AddChild(std::make_unique<ActionNode>(
		[](Enemy& enemy, float /*deltaTime*/) {
			// Attack ステートに切り替えるだけ
			enemy.RequestAttackState();
			return BehaviorStatus::Running; // 攻撃中は Running 扱い
		}
	));

	// シーケンスノード: プレイヤーが見える場合、追跡する
	auto chaseSequence = std::make_unique<SequenceNode>();
	chaseSequence->AddChild(std::make_unique<ConditionNode>(
		[](Enemy& enemy) {
			return enemy.CanSeePlayer();
		}
	));
	chaseSequence->AddChild(std::make_unique<ActionNode>(
		[](Enemy& enemy, float /*deltaTime*/) {
			// Chase ステートに任せる
			enemy.RequestChaseState();
			return BehaviorStatus::Running;
		}
	));

	// 徘徊アクションノード
	auto wanderAction = std::make_unique<ActionNode>(
		[](Enemy& enemy, float /*deltaTime*/) {
			// Wander ステートに任せる
			enemy.RequestWanderState();
			return BehaviorStatus::Running;
		}
	);

	//  HP0なら DeadState を要求
	auto deadSequence = std::make_unique<SequenceNode>();
	deadSequence->AddChild(std::make_unique<ActionNode>(
		[](Enemy& enemy, float /*dt*/) {
			if (enemy.IsDead()) {
				enemy.RequestDeadState();
				return BehaviorStatus::Running;
			}
			return BehaviorStatus::Failure;
		}
	));

	// ルートセレクタに子ノードを追加
	rootSelector->AddChild(std::move(attackSequence));
	rootSelector->AddChild(std::move(chaseSequence));
	rootSelector->AddChild(std::move(wanderAction));
	rootSelector->AddChild(std::move(deadSequence));

	// ビヘイビアツリーのルートノードを設定
	tree.SetRootNode(std::move(rootSelector));
}
