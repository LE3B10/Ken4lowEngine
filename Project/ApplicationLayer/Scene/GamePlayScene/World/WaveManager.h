#pragma once
#include <vector>
#include <algorithm>
#include <string>
#include "Vector3.h"

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 ---------- ///
class CharacterWorld;

/// ---------- 1体分のスポーン情報 ---------- ///
struct WaveSpawnEntry
{
	K4E::Vector3 position = { 0.0f, 0.0f, 0.0f }; // レベルデータのスポーン位置
	K4E::Vector3 correctedPosition = { 0.0f, 0.0f, 0.0f }; // 安全補正後位置
	int wave = 1;
	int group = 0;
	int count = 1;
	int sourceIndex = -1;
	int spawnRequestId = -1;
	std::string name;
	std::string archetype;
	bool insideStage = false;
};

/// ---------- 1ウェーブ分の定義 ---------- ///
struct WaveDefinition
{
	std::vector<WaveSpawnEntry> enemies; // このウェーブに含まれるスポーン情報のリスト
	float delayBeforeSpawnSec = 0.0f; // ウェーブ開始前の待機時間（秒）
};

/// -------------------------------------------------------------
///				　	　ウェーブ管理クラス
/// -------------------------------------------------------------
class WaveManager
{
public: /// ---------- メンバ関数 ---------- ///

	// ウェーブ管理をリセット
	void Reset();

	/// <summary>
	/// ウェーブを設定します。
	/// </summary>
	/// <param name="waves">設定するウェーブ定義のベクター。</param>
	void SetWaves(const std::vector<WaveDefinition>& waves);

	/// <summary>
	/// 処理を開始します。
	/// </summary>
	void Start();

	/// <summary>
	/// キャラクターワールドを更新します。
	/// </summary>
	/// <param name="characters">更新するキャラクターワールドへの参照。</param>
	/// <param name="deltaTime">前回の更新からの経過時間(秒)。</param>
	void Update(CharacterWorld& characters, float deltaTime);

public: /// ---------- アクセッサ ---------- ///

	bool HasStarted() const { return started_; } // ウェーブスポーンが開始されたかどうか
	bool IsWaveInProgress() const { return waveInProgress_; } // 現在ウェーブがスポーン中かどうか
	bool IsWaitingNextWave() const { return started_ && !waveInProgress_ && !allWavesCleared_; } // 次のウェーブスポーン待ちかどうか
	bool IsAllWavesCleared() const { return allWavesCleared_; } // すべてのウェーブがクリアされたかどうか

	int GetCurrentWaveIndex() const { return currentWaveIndex_; } // 現在のウェーブのインデックス
	int GetCurrentWaveNumber() const { return currentWaveIndex_ + 1; } // 現在のウェーブの番号（1始まり）
	int GetTotalWaveCount() const { return static_cast<int>(waves_.size()); } // 総ウェーブ数

	float GetNextWaveTimer() const { return nextWaveTimer_; } // 次のウェーブスポーンまでのタイマー（秒)
	const std::vector<WaveSpawnEntry>& GetSpawnPoints() const { return spawnPoints_; }
	const std::vector<WaveSpawnEntry>& GetLastSpawnResults() const { return lastSpawnResults_; }
	int GetLastSpawnWaveNumber() const { return lastSpawnWaveNumber_; }

private: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// ウェーブをスポーンします。
	/// </summary>
	/// <param name="characters">キャラクターワールド。</param>
	/// <param name="wave">スポーンするウェーブの定義。</param>
	int SpawnWave(CharacterWorld& characters, const WaveDefinition& wave);

private: /// ---------- メンバ変数 ---------- ///

	std::vector<WaveDefinition> waves_; // ウェーブ定義のリスト

	int currentWaveIndex_ = 0; // 現在のウェーブのインデックス
	float nextWaveTimer_ = 0.0f; // 次のウェーブスポーンまでのタイマー（秒）

	bool started_ = false; // ウェーブスポーンが開始されたかどうか
	bool waveInProgress_ = false; // 現在ウェーブがスポーン中かどうか
	bool allWavesCleared_ = false; // すべてのウェーブがクリアされたかどうか
	int currentWaveSpawnedCount_ = 0; // 現在のウェーブでスポーンした敵の数

	std::vector<WaveSpawnEntry> spawnPoints_;
	std::vector<WaveSpawnEntry> lastSpawnResults_;
	int lastSpawnWaveNumber_ = 0;
	int nextSpawnRequestId_ = 1;
};

