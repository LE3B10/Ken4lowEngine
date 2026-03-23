#pragma once
#include <string>
#include <vector>

/// ---------- 前方宣言 ---------- ///
class BossBase;
class IBossAttack;

/// -------------------------------------------------------------
/// BossBrain
///
/// 役割:
/// - 今の状況から「どの攻撃を選ぶか」を判断する
/// - 実際の攻撃開始は行わない
/// - まずは攻撃選択だけを担当し、状態遷移は Boss 側に任せる
/// -------------------------------------------------------------
class BossBrain
{
public:
	/// <summary>
	/// 初期化
	/// owner は思考対象のボス本体
	/// </summary>
	void Initialize(BossBase* owner);

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// 今の状況で最適な攻撃名を返す
	/// 候補が無いときは空文字
	/// </summary>
	std::string SelectBestAttackName() const;

	/// <summary>
	/// 最後に評価した攻撃名
	/// ImGui デバッグ用
	/// </summary>
	const std::string& GetLastBestAttackName() const { return lastBestAttackName_; }

	/// <summary>
	/// 最後に評価したスコア
	/// ImGui デバッグ用
	/// </summary>
	float GetLastBestScore() const { return lastBestScore_; }

private:
	/// <summary>
	/// 攻撃1つ分のスコアを計算
	/// 基本は priority を土台にし、状況補正を足す
	/// </summary>
	float EvaluateAttackScore(const IBossAttack& attack) const;

private:
	BossBase* owner_ = nullptr;

	// デバッグ表示用
	mutable std::string lastBestAttackName_ = "None";
	mutable float lastBestScore_ = -999999.0f;
};