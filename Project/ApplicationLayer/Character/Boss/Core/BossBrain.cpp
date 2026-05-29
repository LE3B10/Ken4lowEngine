#define NOMINMAX
#include "BossBrain.h"
#include "BossBase.h"
#include "BossAttackComponent.h"
#include "IBossAttack.h"

#include <cstring>
#include <limits>

/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void BossBrain::Initialize(BossBase* owner)
{
	owner_ = owner;
	lastBestAttackName_ = "None";
	lastBestScore_ = -999999.0f;
}

/// -------------------------------------------------------------
///							終了処理
/// -------------------------------------------------------------
void BossBrain::Finalize()
{
	owner_ = nullptr;
	lastBestAttackName_ = "None";
	lastBestScore_ = -999999.0f;
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

	// 候補の中からスコアを計算して最適なものを選ぶ
	IBossAttack* bestAttack = nullptr;

	// スコアの初期値は非常に低くしておく（負の無限大）
	float bestScore = -std::numeric_limits<float>::max();

	// 候補をループしてスコアを計算
	for (IBossAttack* attack : candidates)
	{
		// 攻撃が nullptr ならスコア計算できないのでスキップ
		if (!attack) continue;

		// スコアを計算
		const float score = EvaluateAttackScore(*attack);

		// スコアが最高のものを選ぶ
		if (!bestAttack || score > bestScore)
		{
			bestAttack = attack;
			bestScore = score;
		}
	}

	// 最適な攻撃が見つからないときは空文字
	if (!bestAttack) return "";

	// 見つかった最適な攻撃の名前を返す
	lastBestAttackName_ = bestAttack->GetName();

	// デバッグ用にスコアも保存しておく
	lastBestScore_ = bestScore;

	// 最適な攻撃の名前を返す
	return bestAttack->GetName();
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

	// 攻撃距離が有効範囲内ならスコアを上げる
	const float distance = owner_->GetDistanceToTargetXZ();

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