#define NOMINMAX
#include "WaveManager.h"
#include "CharacterWorld.h"
#include <iostream>

using namespace Ken4lowEngine;


void WaveManager::Reset()
{
	currentWaveIndex_ = -1;
	nextWaveTimer_ = 0.0f;
	started_ = false;
	waveInProgress_ = false;
	allWavesCleared_ = false;
	currentWaveSpawnedCount_ = 0;
	lastSpawnResults_.clear();
	lastSpawnWaveNumber_ = 0;
	nextSpawnRequestId_ = 1;
}

void WaveManager::SetWaves(const std::vector<WaveDefinition>& waves)
{
	waves_ = waves;
	spawnPoints_.clear();
	for (const auto& wave : waves_)
	{
		for (const auto& entry : wave.enemies)
		{
			spawnPoints_.push_back(entry);
		}
	}
	Reset();
}

void WaveManager::Start()
{
	Reset();

	if (waves_.empty())
	{
		allWavesCleared_ = true;
		return;
	}

	started_ = true;
	currentWaveIndex_ = 0;
	nextWaveTimer_ = std::max(0.0f, waves_[0].delayBeforeSpawnSec);
}

void WaveManager::Update(CharacterWorld& characters, float deltaTime)
{
	if (!started_ || allWavesCleared_ || waves_.empty()) return;
	if (!waveInProgress_)
	{
		nextWaveTimer_ -= deltaTime;
		if (nextWaveTimer_ > 0.0f) return;
		currentWaveSpawnedCount_ = SpawnWave(characters, waves_[currentWaveIndex_]);
		waveInProgress_ = (currentWaveSpawnedCount_ > 0);
		nextWaveTimer_ = (currentWaveSpawnedCount_) ? 0.0f : 1.0f;
		return;
	}
	if (characters.GetEnemyCount() > 0) return;
	if (currentWaveIndex_ + 1 >= static_cast<int>(waves_.size()))
	{
		waveInProgress_ = false;
		allWavesCleared_ = true;
		currentWaveSpawnedCount_ = 0;
		return;
	}
	++currentWaveIndex_;
	waveInProgress_ = false;
	nextWaveTimer_ = std::max(0.0f, waves_[currentWaveIndex_].delayBeforeSpawnSec);
}

int WaveManager::SpawnWave(CharacterWorld& characters, const WaveDefinition& wave)
{
	int spawnedCount = 0;
	lastSpawnResults_.clear();
	lastSpawnWaveNumber_ = currentWaveIndex_ + 1;
	for (const auto& entry : wave.enemies)
	{
		auto spawnResult = characters.SpawnEnemyAt(entry.position, nextSpawnRequestId_++);
		WaveSpawnEntry debugEntry = entry;
		debugEntry.requestedPosition = spawnResult.requestedPosition;
		debugEntry.correctedPosition = spawnResult.correctedPosition;
		debugEntry.insideStage = spawnResult.insideStage;
		debugEntry.groundHit = spawnResult.groundHit;
		debugEntry.hitY = spawnResult.hitY;
		debugEntry.spawnAccepted = spawnResult.spawnAccepted;
		debugEntry.spawnRequestId = spawnResult.spawnRequestId;
		lastSpawnResults_.push_back(debugEntry);
		++spawnedCount;
	}
	return spawnedCount;
}
