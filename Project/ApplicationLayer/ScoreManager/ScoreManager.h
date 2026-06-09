#pragma once

/// -------------------------------------------------------------
///						スコア管理クラス
///
/// GamePlay中のスコア、キル数、現在Wave番号を保持するシングルトン。
/// 所有者は持たず、敵撃破通知やDebug表示から参照される軽量な集計場所として使う。
/// -------------------------------------------------------------
class ScoreManager
{
public: /// ---------- メンバ関数 ---------- ///

	// プロセス内で共有するScoreManagerインスタンスを取得する。
	static ScoreManager* GetInstance();

	// 新規ゲーム開始時にスコアとキル数を初期化する。
	void Initialize();

	// 敵撃破時にキル数と撃破スコアを加算する。
	void AddKill();

	// スコアとキル数を0へ戻す。現在Waveは別途管理する。
	void Reset();

	// Debug用に現在スコアとキル数をImGuiへ表示する。
	void DrawImGui() const;

	// 任意イベントからスコアを加算する。負値を渡すと減点にも使える。
	void AddScore(int value) { score_ += value; }

	// Waveのセット
	//void SetWave(int wave) { currentWave_ = wave; }

	// スコアの取得
	int GetScore() const { return score_; }

	// Kill数の取得
	int GetKills() const { return kills_; }

	// Waveの取得
	int GetCurrentWave() const { return currentWave_; }

private: /// ---------- メンバ変数 ---------- ///

	int score_ = 0;
	int kills_ = 0;
	int currentWave_ = 1;
};

