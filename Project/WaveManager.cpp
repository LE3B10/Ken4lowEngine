#define NOMINMAX
#include "WaveManager.h"
#include "CharacterWorld.h"

using namespace Ken4lowEngine;


void WaveManager::Reset()
{
	currentWaveIndex_ = -1;
	nextWaveTimer_ = 0.0f;
	started_ = false;
	waveInProgress_ = false;
	allWavesCleared_ = false;
}

void WaveManager::SetWaves(const std::vector<WaveDefinition>& waves)
{
	waves_ = waves;
	Reset();
}

void WaveManager::Start()
{
	Reset();

	if (waves_.empty())
	{
		allWavesCleared_ = true; // ウェーブ定義が空なら即クリア扱い
		return;
	}

	started_ = true;
	currentWaveIndex_ = 0; // 最初のウェーブは Update 内でインクリメントしてからスポーンする
	nextWaveTimer_ = std::max(0.0f, waves_[0].delayBeforeSpawnSec); // 最初のウェーブの待機時間をセット
}

void WaveManager::Update(CharacterWorld& characters, float deltaTime)
{
	if (!started_ || allWavesCleared_ || waves_.empty()) return;

	// まだ現在ウェーブをスポーンしていない状態
	if (!waveInProgress_)
	{
		nextWaveTimer_ -= deltaTime;
		
		// タイマーがまだ残っているなら待機
		if (nextWaveTimer_ > 0.0f) return;

		// ウェーブをスポーン
		SpawnWave(characters, waves_[currentWaveIndex_]);
		waveInProgress_ = true;
		nextWaveTimer_ = 0.0f;
		return;
	}

	// ウェーブ進行中は、敵が残っている限り何もしない
	if (characters.GetEnemyCount() > 0) return;


	// 現在ウェーブ殲滅完了
	if (currentWaveIndex_ + 1 >= static_cast<int>(waves_.size()))
	{
		// 最終ウェーブまで終わった
		waveInProgress_ = false;
		allWavesCleared_ = true;
		return;
	}

	// 次ウェーブへ
	++currentWaveIndex_;
	waveInProgress_ = false;
	nextWaveTimer_ = std::max(0.0f, waves_[currentWaveIndex_].delayBeforeSpawnSec);
}

void WaveManager::SpawnWave(CharacterWorld& characters, const WaveDefinition& wave)
{
	for (const auto& entry : wave.enemies)
	{
		characters.SpawnEnemy(entry.archetype, entry.position);
	}
}
