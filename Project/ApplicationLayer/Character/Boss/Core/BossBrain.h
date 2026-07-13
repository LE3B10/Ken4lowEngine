#pragma once

#include <functional>
#include <memory>
#include <random>
#include <string>
#include <vector>

class BossBase;
class IBossAttack;
class IBTNode;

/// ボスが次に行う大分類を表し、派生ボス側の状態遷移へ渡す。
enum class BossDecision
{
	Idle,
	Move,
	Attack,
};

/// ビヘイビアツリーが行動を判断するために必要な実行時情報をまとめる。
struct BossDecisionContext
{
	bool canAttack = false;
	bool isMoving = false;
	float moveStartDistance = 0.0f;
	float moveStopDistance = 0.0f;
};

/// 攻撃ごとのAI使用可否と距離補正を、選択処理から分離して登録する。
struct BossAttackRule
{
	using DistanceScoreFunction = std::function<float(float)>;

	std::string attackName;
	bool enabledForAI = true;
	float repeatWeightScale = 0.35f;
	DistanceScoreFunction evaluateDistance;
};

/// ボスの行動決定と、登録ルールに基づく攻撃選択を担当する。
class BossBrain
{
public:
	BossBrain() = default;
	~BossBrain();

	/// 所有者を設定し、毎フレーム実行するビヘイビアツリーを構築する。
	void Initialize(BossBase* owner);

	/// ツリーと攻撃ルールを破棄し、所有者との参照を切る。
	void Finalize();

	/// 状況をツリーへ渡し、攻撃・移動・待機のいずれかを返す。
	BossDecision TickDecision(const BossDecisionContext& context, float deltaTime);

	/// 攻撃ルールを名前単位で登録し、同名ルールがあれば置き換える。
	void RegisterAttackRule(BossAttackRule rule);

	/// 派生ボスを再設定できるよう、登録済みの攻撃ルールをすべて消す。
	void ClearAttackRules();

	/// 現在開始可能な候補から、登録ルールに基づいて攻撃名を1件選ぶ。
	std::string SelectBestAttackName() const;

	/// 直近の行動決定で選ばれた攻撃名を返す。
	const std::string& GetLastBestAttackName() const { return lastBestAttackName_; }

	/// 直近の行動決定で選ばれた攻撃スコアを返す。
	float GetLastBestScore() const { return lastBestScore_; }

private:
	/// セレクタ・シーケンス・条件・行動を組み合わせた判断木を作る。
	void BuildDecisionTree();

	/// 登録名に一致する攻撃ルールを返し、未登録ならnullptrを返す。
	const BossAttackRule* FindAttackRule(const char* attackName) const;

	/// 攻撃の優先度、射程、登録済み距離補正から抽選用スコアを計算する。
	float EvaluateAttackScore(const IBossAttack& attack) const;

	/// 候補を登録ルールで絞り込み、スコアを重みとした抽選を行う。
	IBossAttack* SelectWeightedAttack(const std::vector<IBossAttack*>& candidates) const;

private:
	BossBase* owner_ = nullptr;
	std::unique_ptr<IBTNode> decisionTree_;
	std::vector<BossAttackRule> attackRules_;
	BossDecisionContext decisionContext_{};
	BossDecision lastDecision_ = BossDecision::Idle;
	mutable std::string lastBestAttackName_ = "None";
	mutable float lastBestScore_ = -999999.0f;
	mutable std::string previousSelectedAttackName_ = "None";
	mutable std::mt19937 rng_{ std::random_device{}() };
};
