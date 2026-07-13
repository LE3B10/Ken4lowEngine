#define NOMINMAX
#include "BossBrain.h"

#include "BossAttackComponent.h"
#include "BossBase.h"
#include "IBossAttack.h"
#include "BTActionNode.h"
#include "BTConditionNode.h"
#include "BTSelectorNode.h"
#include "BTSequenceNode.h"

#include <algorithm>
#include <utility>

BossBrain::~BossBrain() = default;

void BossBrain::Initialize(BossBase* owner)
{
	owner_ = owner;
	lastBestAttackName_ = "None";
	lastBestScore_ = -999999.0f;
	previousSelectedAttackName_ = "None";
	lastDecision_ = BossDecision::Idle;
	BuildDecisionTree();
}

void BossBrain::Finalize()
{
	decisionTree_.reset();
	attackRules_.clear();
	owner_ = nullptr;
	lastBestAttackName_ = "None";
	lastBestScore_ = -999999.0f;
	previousSelectedAttackName_ = "None";
	lastDecision_ = BossDecision::Idle;
}

BossDecision BossBrain::TickDecision(const BossDecisionContext& context, float deltaTime)
{
	decisionContext_ = context;
	lastDecision_ = BossDecision::Idle;

	if (!decisionTree_)
	{
		return lastDecision_;
	}

	// 毎フレーム同じツリーを実行し、条件分岐の優先順位をノード構造で管理する。
	decisionTree_->Tick(deltaTime);
	return lastDecision_;
}

void BossBrain::RegisterAttackRule(BossAttackRule rule)
{
	if (rule.attackName.empty())
	{
		return;
	}

	auto found = std::find_if(attackRules_.begin(), attackRules_.end(),
		[&rule](const BossAttackRule& current) { return current.attackName == rule.attackName; });
	if (found != attackRules_.end())
	{
		*found = std::move(rule);
		return;
	}

	attackRules_.push_back(std::move(rule));
}

void BossBrain::ClearAttackRules()
{
	attackRules_.clear();
}

void BossBrain::BuildDecisionTree()
{
	auto root = std::make_unique<BTSelectorNode>();

	auto attackSequence = std::make_unique<BTSequenceNode>();
	attackSequence->AddChild(std::make_unique<BTConditionNode>([this]()
		{
			return decisionContext_.canAttack;
		}));
	attackSequence->AddChild(std::make_unique<BTActionNode>([this](float)
		{
			const std::string selectedAttack = SelectBestAttackName();
			if (selectedAttack.empty())
			{
				return BehaviorStatus::Failure;
			}

			lastDecision_ = BossDecision::Attack;
			return BehaviorStatus::Success;
		}));
	root->AddChild(std::move(attackSequence));

	auto moveSequence = std::make_unique<BTSequenceNode>();
	moveSequence->AddChild(std::make_unique<BTConditionNode>([this]()
		{
			if (!owner_)
			{
				return false;
			}

			// 移動中だけ停止距離を使い、待機と移動の境界で状態が往復することを防ぐ。
			const float threshold = decisionContext_.isMoving
				? decisionContext_.moveStopDistance
				: decisionContext_.moveStartDistance;
			return owner_->GetDistanceToTargetXZ() > threshold;
		}));
	moveSequence->AddChild(std::make_unique<BTActionNode>([this](float)
		{
			lastDecision_ = BossDecision::Move;
			return BehaviorStatus::Success;
		}));
	root->AddChild(std::move(moveSequence));

	root->AddChild(std::make_unique<BTActionNode>([this](float)
		{
			lastDecision_ = BossDecision::Idle;
			return BehaviorStatus::Success;
		}));

	decisionTree_ = std::move(root);
}

const BossAttackRule* BossBrain::FindAttackRule(const char* attackName) const
{
	if (!attackName)
	{
		return nullptr;
	}

	const auto found = std::find_if(attackRules_.begin(), attackRules_.end(),
		[attackName](const BossAttackRule& rule) { return rule.attackName == attackName; });
	return found != attackRules_.end() ? &(*found) : nullptr;
}

std::string BossBrain::SelectBestAttackName() const
{
	lastBestAttackName_ = "None";
	lastBestScore_ = -999999.0f;

	if (!owner_)
	{
		return "";
	}

	BossAttackComponent* attackComponent = owner_->GetAttackComponent();
	if (!attackComponent || attackComponent->IsAttacking())
	{
		return "";
	}

	const std::vector<IBossAttack*> candidates = attackComponent->CollectStartableAttacks();
	IBossAttack* selectedAttack = SelectWeightedAttack(candidates);
	if (!selectedAttack)
	{
		return "";
	}

	lastBestAttackName_ = selectedAttack->GetName();
	lastBestScore_ = EvaluateAttackScore(*selectedAttack);
	previousSelectedAttackName_ = selectedAttack->GetName();
	return selectedAttack->GetName();
}

IBossAttack* BossBrain::SelectWeightedAttack(const std::vector<IBossAttack*>& candidates) const
{
	struct WeightedCandidate
	{
		IBossAttack* attack = nullptr;
		float weight = 0.0f;
	};

	std::vector<WeightedCandidate> weightedCandidates;
	weightedCandidates.reserve(candidates.size());
	float totalWeight = 0.0f;

	for (IBossAttack* attack : candidates)
	{
		if (!attack)
		{
			continue;
		}

		const BossAttackRule* rule = FindAttackRule(attack->GetName());
		// ルールを1件でも登録したボスでは、未登録攻撃を自動選択へ混ぜない。
		if ((!attackRules_.empty() && !rule) || (rule && !rule->enabledForAI))
		{
			continue;
		}

		float weight = std::max(1.0f, EvaluateAttackScore(*attack));
		if (previousSelectedAttackName_ == attack->GetName())
		{
			weight *= rule ? std::clamp(rule->repeatWeightScale, 0.0f, 1.0f) : 0.35f;
		}

		weightedCandidates.push_back({ attack, weight });
		totalWeight += weight;
	}

	if (weightedCandidates.empty() || totalWeight <= 0.0f)
	{
		return nullptr;
	}

	std::uniform_real_distribution<float> distribution(0.0f, totalWeight);
	float roll = distribution(rng_);
	for (const WeightedCandidate& candidate : weightedCandidates)
	{
		roll -= candidate.weight;
		if (roll <= 0.0f)
		{
			return candidate.attack;
		}
	}

	return weightedCandidates.back().attack;
}

float BossBrain::EvaluateAttackScore(const IBossAttack& attack) const
{
	if (!owner_)
	{
		return -100000.0f;
	}

	float score = static_cast<float>(attack.GetPriority());
	const float distance = owner_->GetDistanceToTargetXZ();
	if (distance >= attack.GetMinRange() && distance <= attack.GetMaxRange())
	{
		score += 20.0f;
	}
	else
	{
		score -= 50.0f;
	}

	const BossAttackRule* rule = FindAttackRule(attack.GetName());
	if (rule && rule->evaluateDistance)
	{
		score += rule->evaluateDistance(distance);
	}

	return score;
}
