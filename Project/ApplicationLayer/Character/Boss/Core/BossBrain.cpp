#define NOMINMAX
#include "BossBrain.h"
#include "BossBase.h"
#include "BossAttackComponent.h"
#include "IBossAttack.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <random>

/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void BossBrain::Initialize(BossBase* owner)
{
	owner_ = owner;
	lastBestAttackName_ = "None";
	lastBestScore_ = -999999.0f;
	previousSelectedAttackName_ = "None";
}

/// -------------------------------------------------------------
///							終了処理
/// -------------------------------------------------------------
void BossBrain::Finalize()
{
	owner_ = nullptr;
	lastBestAttackName_ = "None";
	lastBestScore_ = -999999.0f;
	previousSelectedAttackName_ = "None";
}

/// -------------------------------------------------------------
///					今の状況で最適攻撃を選ぶ
/// -------------------------------------------------------------
std::string BossBrain::SelectBestAttackName() const
{
	lastBestAttackName_ = "None";
	lastBestScore_ = -999999.0f;

	// 所有者がいないときは選択できない
	if (!owner_) return "";

	// 攻撃コンポーネントを取得
	BossAttackComponent* attackComp = owner_->GetAttackComponent();

	// 攻撃コンポーネントがないときは選択できない
	if (!attackComp) return "";

	// すでに攻撃中なら新規選択しない
	if (attackComp->IsAttacking()) return "";

	// 開始可能な攻撃を集める
	const std::vector<IBossAttack*> candidates = attackComp->CollectStartableAttacks();

	// 候補がいないときは選択できない
	if (candidates.empty())	return "";

	// 候補の中から距離帯補正済みスコアを重みとして抽選し、同じ攻撃の連打感を抑える。
	IBossAttack* bestAttack = SelectWeightedAttack(candidates);
	const float bestScore = bestAttack ? EvaluateAttackScore(*bestAttack) : -std::numeric_limits<float>::max();

	// 最適な攻撃が見つからないときは空文字
	if (!bestAttack) return "";

	// 見つかった最適な攻撃の名前を返す
	lastBestAttackName_ = bestAttack->GetName();

	// デバッグ用にスコアも保存しておく
	lastBestScore_ = bestScore;
	previousSelectedAttackName_ = bestAttack->GetName();

	// 最適な攻撃の名前を返す
	return bestAttack->GetName();
}


IBossAttack* BossBrain::SelectWeightedAttack(const std::vector<IBossAttack*>& candidates) const
{
	struct WeightedCandidate
	{
		IBossAttack* attack = nullptr;
		float weight = 0.0f;
	};

	std::vector<WeightedCandidate> weighted;
	weighted.reserve(candidates.size());
	float totalWeight = 0.0f;

	for (IBossAttack* attack : candidates)
	{
		if (!attack) continue;

		float weight = std::max(1.0f, EvaluateAttackScore(*attack));
		if (previousSelectedAttackName_ == attack->GetName())
		{
			weight *= 0.35f;
		}

		weighted.push_back({ attack, weight });
		totalWeight += weight;
	}

	if (weighted.empty() || totalWeight <= 0.0f)
	{
		return nullptr;
	}

	std::uniform_real_distribution<float> dist(0.0f, totalWeight);
	float roll = dist(rng_);
	for (const WeightedCandidate& candidate : weighted)
	{
		roll -= candidate.weight;
		if (roll <= 0.0f)
		{
			return candidate.attack;
		}
	}

	return weighted.back().attack;
}

/// -------------------------------------------------------------
///						 攻撃スコア計算
/// -------------------------------------------------------------
float BossBrain::EvaluateAttackScore(const IBossAttack& attack) const
{
	// 所有者がいないときはスコア計算できない
	if (!owner_) return -100000.0f;

	// 基本スコアは攻撃の優先度
	float score = static_cast<float>(attack.GetPriority());

	// 攻撃距離が有効範囲内ならスコアを上げ、有効外なら候補内でも選ばれにくくする。
	const float distance = owner_->GetDistanceToTargetXZ();
	if (distance >= attack.GetMinRange() && distance <= attack.GetMaxRange())
	{
		score += 20.0f;
	}
	else
	{
		score -= 50.0f;
	}

	// 攻撃名で状況補正をかける
	if (std::strcmp(attack.GetName(), "HeavyPunch") == 0)
	{
		// 超近距離では HeavyPunch をかなり優遇
		if (distance <= 2.5f) score += 30.0f;

		// 近距離ならまだ優遇
		else if (distance <= 3.5f) score += 12.0f;

		// 少し遠いと不利
		else if (distance <= 5.0f) score -= 10.0f;

		// 遠いとかなり不利
		else score -= 40.0f;
	}

	// Shockwave は中距離で選ばれやすくし、近距離ではパンチ系へ譲る。
	else if (std::strcmp(attack.GetName(), "GuardianShockwave") == 0)
	{
		if (distance >= attack.GetMinRange() && distance <= attack.GetMaxRange()) score += 40.0f;
		else if (distance < attack.GetMinRange()) score -= 30.0f;
		else score -= 45.0f;
	}

	// ChargeAttack は遠距離で距離を詰めるため、遠距離帯では強く優遇する。
	else if (std::strcmp(attack.GetName(), "ChargeAttack") == 0)
	{
		if (distance >= attack.GetMinRange() && distance <= attack.GetMaxRange()) score += 45.0f;
		else score -= 35.0f;
	}

	// Punch は Heavy より少し遠めでも届く想定で、近距離以外でもそこそこ優遇
	else if (std::strcmp(attack.GetName(), "Punch") == 0)
	{
		// 中近距離で使いやすくする
		if (distance <= 4.5f) score += 10.0f;

		// Heavy より少し遠めでも届く想定なら少し補正
		if (distance >= 3.0f && distance <= 5.5f) score += 8.0f;
	}

	return score;
}