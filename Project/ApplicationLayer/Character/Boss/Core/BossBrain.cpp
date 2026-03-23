#define NOMINMAX
#include "BossBrain.h"
#include "Core/BossBase.h"
#include "Components/BossAttackComponent.h"
#include "Attacks/IBossAttack.h"

#include <cstring>
#include <limits>

void BossBrain::Initialize(BossBase* owner)
{
	owner_ = owner;
	lastBestAttackName_ = "None";
	lastBestScore_ = -999999.0f;
}

void BossBrain::Finalize()
{
	owner_ = nullptr;
	lastBestAttackName_ = "None";
	lastBestScore_ = -999999.0f;
}

/// -------------------------------------------------------------
/// 今の状況で最適攻撃を選ぶ
/// 手順:
/// 1. 開始可能な候補を集める
/// 2. 各候補の score を計算する
/// 3. 一番 score の高い攻撃名を返す
/// -------------------------------------------------------------
std::string BossBrain::SelectBestAttackName() const
{
	lastBestAttackName_ = "None";
	lastBestScore_ = -999999.0f;

	if (!owner_)
	{
		return "";
	}

	BossAttackComponent* attackComp = owner_->GetAttackComponent();
	if (!attackComp)
	{
		return "";
	}

	// すでに攻撃中なら新規選択しない
	if (attackComp->IsAttacking())
	{
		return "";
	}

	const std::vector<IBossAttack*> candidates = attackComp->CollectStartableAttacks();
	if (candidates.empty())
	{
		return "";
	}

	IBossAttack* bestAttack = nullptr;
	float bestScore = -std::numeric_limits<float>::max();

	for (IBossAttack* attack : candidates)
	{
		if (!attack)
		{
			continue;
		}

		const float score = EvaluateAttackScore(*attack);
		if (!bestAttack || score > bestScore)
		{
			bestAttack = attack;
			bestScore = score;
		}
	}

	if (!bestAttack)
	{
		return "";
	}

	lastBestAttackName_ = bestAttack->GetName();
	lastBestScore_ = bestScore;
	return bestAttack->GetName();
}

/// -------------------------------------------------------------
/// 攻撃スコア計算
///
/// 基本方針:
/// - priority を土台にする
/// - 距離や連打抑制で加点 / 減点する
/// - 「開始不可」判定は Attack 側 CanStart() に任せる
///   ここでは「どれがより適切か」を点数化する
/// -------------------------------------------------------------
float BossBrain::EvaluateAttackScore(const IBossAttack& attack) const
{
	if (!owner_)
	{
		return -100000.0f;
	}

	float score = static_cast<float>(attack.GetPriority());
	const float distance = owner_->GetDistanceToTargetXZ();

	// ---------------------------------------------------------
	// 攻撃名ごとの補正
	// 今は Punch / HeavyPunch のみ
	// ---------------------------------------------------------
	if (std::strcmp(attack.GetName(), "HeavyPunch") == 0)
	{
		// 超近距離では HeavyPunch をかなり優遇
		if (distance <= 2.5f)
		{
			score += 30.0f;
		}
		// 近距離ならまだ優遇
		else if (distance <= 3.5f)
		{
			score += 12.0f;
		}
		// 少し遠いと不利
		else if (distance <= 5.0f)
		{
			score -= 10.0f;
		}
		// 遠いとかなり不利
		else
		{
			score -= 40.0f;
		}
	}
	else if (std::strcmp(attack.GetName(), "Punch") == 0)
	{
		// 中近距離で使いやすくする
		if (distance <= 4.5f)
		{
			score += 10.0f;
		}

		// Heavy より少し遠めでも届く想定なら少し補正
		if (distance >= 3.0f && distance <= 5.5f)
		{
			score += 8.0f;
		}
	}

	return score;
}