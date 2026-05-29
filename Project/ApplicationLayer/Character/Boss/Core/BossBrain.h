#pragma once
#include <string>

/// ---------- 前方宣言 ---------- ///
class BossBase;
class IBossAttack;

/// -------------------------------------------------------------
/// 					　ボスの思考クラス
/// -------------------------------------------------------------
class BossBrain
{
public: /// ---------- メンバ関数 ---------- ///

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

private: /// ---------- 内部処理 ---------- ///

	/// <summary>
	/// 攻撃1つ分のスコアを計算
	/// 基本は priority を土台にし、状況補正を足す
	/// </summary>
	float EvaluateAttackScore(const IBossAttack& attack) const;

private: /// ---------- メンバ変数 ---------- ///

	// 思考対象のボス本体
	BossBase* owner_ = nullptr;

	// デバッグ表示用
	mutable std::string lastBestAttackName_ = "None";

	// 最後に評価したスコア
	mutable float lastBestScore_ = -999999.0f;
};