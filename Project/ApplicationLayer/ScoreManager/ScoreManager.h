#pragma once

/// -------------------------------------------------------------
///						スコア管理クラス
/// -------------------------------------------------------------
class ScoreManager
{
public: /// ---------- メンバ関数 ---------- ///

	// シングルトンインスタンス取得
	static ScoreManager* GetInstance();

	// 初期化処理
	void Initialize();

	// Kill数の追加
	void AddKill();

	// スコアのリセット
	void Reset();

	// ImGui描画処理
	void DrawImGui() const;

	// スコアの追加
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

