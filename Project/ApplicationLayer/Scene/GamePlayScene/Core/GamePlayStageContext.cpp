#define NOMINMAX
#include "GamePlayStageContext.h"

#include "StageRepository.h"
#include "LevelLoader.h"
#include "EnemyTuningRepository.h"

#include <algorithm>

using namespace Ken4lowEngine;

namespace
{
	std::vector<StageInfo> BuildDefaultStages()
	{
		std::vector<StageInfo> stages;
		stages.push_back({ 0u, "始まりの森",      "white.png", false, 0u, { 0.18f, 0.49f, 0.20f, 1.0f } });
		stages.push_back({ 1u, "廃鉱山",          "white.png", true,  0u, { 0.43f, 0.30f, 0.25f, 1.0f } });
		stages.push_back({ 2u, "工業地帯",        "white.png", true,  0u, { 0.96f, 0.49f, 0.00f, 1.0f } });
		stages.push_back({ 3u, "朽ちた果てた街",  "white.png", true,  0u, { 0.37f, 0.35f, 0.49f, 1.0f } });
		stages.push_back({ 4u, "港湾ターミナル",  "white.png", true,  0u, { 0.08f, 0.40f, 0.75f, 1.0f } });
		return stages;
	}

	bool IsPlayerSpawnType(const std::string& type) { return type == "PlayerSpawnPoint"; }
	bool IsEnemySpawnType(const std::string& type)  { return type == "EnemySpawnPoint"; }
	bool IsBossSpawnType(const std::string& type)   { return type == "BossSpawnPoint"; }
}

void GamePlayStageContext::InitializeFromRepository()
{
	auto& repo = StageRepository::GetInstance();

	auto stages = repo.GetStages();
	if (stages.empty())
	{
		stages = BuildDefaultStages();
		repo.SetStages(stages);
	}

	int stageIndex = repo.GetStartIndex().value_or(0);

	if (stageIndex < 0)
	{
		stageIndex = 0;
	}
	if (!stages.empty() && stageIndex >= static_cast<int>(stages.size()))
	{
		stageIndex = static_cast<int>(stages.size()) - 1;
	}

	currentStageIndex_ = stageIndex;
	repo.SetStartIndex(currentStageIndex_);
}

GamePlayStageContext::StageAssetPaths GamePlayStageContext::GetCurrentStageAssets() const
{
	return GetStageAssetPaths(currentStageIndex_);
}

GamePlayStageContext::StageAssetPaths GamePlayStageContext::GetStageAssetPaths(int stageIndex) const
{
	switch (stageIndex)
	{
	case 0: return { "stages/fps_stage00.json", "stages/fps_stage00.gltf" };
	case 1: return { "stages/fps_stage01.json", "stages/fps_stage01.gltf" };
	case 2: return { "stages/fps_stage02.json", "stages/fps_stage02.gltf" };
	case 3: return { "stages/fps_stage03.json", "stages/fps_stage03.gltf" };
	case 4: return { "stages/fps_stage04.json", "stages/fps_stage04.gltf" };
	default:
		return { "stages/fps_stage00.json", "stages/fps_stage00.gltf" };
	}
}

void GamePlayStageContext::LoadSpawnPointsFromLevel(const std::string& jsonPath)
{
	enemySpawnInfos_.clear();
	introCameraPoints_.clear();
	introLookAtPoints_.clear();

	hasPlayerSpawnPoint_ = false;
	hasBossSpawnPoint_ = false;

	playerSpawnPoint_ = { 0.0f, 0.0f, 0.0f };
	bossSpawnPoint_ = { 0.0f, 0.0f, 0.0f };

	const std::unique_ptr<K4E::LevelData> levelData = K4E::LevelLoader::LoadLevel(jsonPath);
	if (!levelData)
	{
		return;
	}

	for (const auto& object : levelData->objects)
	{
		if (IsPlayerSpawnType(object.type))
		{
			if (!hasPlayerSpawnPoint_)
			{
				playerSpawnPoint_ = object.position;
				hasPlayerSpawnPoint_ = true;
			}
		}
		else if (IsEnemySpawnType(object.type))
		{
			EnemySpawnInfo info{};
			info.position = object.position;

			if (object.hasSpawnProps)
			{
				info.wave = (object.spawnProps.wave > 0) ? object.spawnProps.wave : 1;
				info.group = object.spawnProps.group;
				info.count = (object.spawnProps.count > 0) ? object.spawnProps.count : 1;

				// --------------------------------------------------------
				// archetype 文字列があれば EnemyArchetype へ変換する
				// 無い、または変換失敗時は RifleGrunt のままにする
				// --------------------------------------------------------
				if (object.spawnProps.hasArchetype)
				{
					EnemyArchetype parsed{};
					if (EnemyTuningRepository::TryParseArchetype(object.spawnProps.archetype, parsed))
					{
						info.archetype = parsed;
					}
				}
			}

			enemySpawnInfos_.push_back(info);
		}
		else if (IsBossSpawnType(object.type))
		{
			if (!hasBossSpawnPoint_)
			{
				bossSpawnPoint_ = object.position;
				hasBossSpawnPoint_ = true;
			}
		}
		else if (object.type == "IntroCameraPoint")
		{
			IntroCameraPointInfo info{};
			info.position = object.position;
			info.rotation = object.rotation;

			if (object.hasIntroCameraProps)
			{
				info.order = object.introCameraProps.order;
				info.duration = object.introCameraProps.duration;
				info.fov = object.introCameraProps.fov;
				info.targetName = object.introCameraProps.targetName;
				info.interpMode = object.introCameraProps.interpMode;
				info.aimMode = object.introCameraProps.aimMode;
			}

			introCameraPoints_.push_back(info);
		}
		else if (object.type == "IntroLookAtPoint")
		{
			IntroLookAtPointInfo info{};
			info.name = object.name;
			info.position = object.position;
			introLookAtPoints_.push_back(info);
		}
	}

	std::sort(
		introCameraPoints_.begin(),
		introCameraPoints_.end(),
		[](const IntroCameraPointInfo& a, const IntroCameraPointInfo& b)
		{
			return a.order < b.order;
		});
}

void GamePlayStageContext::SetupWaves(WaveManager* waveManager) const
{
	if (!waveManager) { return; }

	std::vector<WaveDefinition> waves;

	if (!enemySpawnInfos_.empty())
	{
		int maxWave = 0;
		for (const auto& spawn : enemySpawnInfos_)
		{
			if (spawn.wave > maxWave)
			{
				maxWave = spawn.wave;
			}
		}

		if (maxWave <= 0)
		{
			maxWave = 1;
		}

		waves.resize(static_cast<size_t>(maxWave));

		for (int i = 0; i < maxWave; ++i)
		{
			if (i == 0)      { waves[static_cast<size_t>(i)].delayBeforeSpawnSec = 0.0f; }
			else if (i == 1) { waves[static_cast<size_t>(i)].delayBeforeSpawnSec = 2.0f; }
			else             { waves[static_cast<size_t>(i)].delayBeforeSpawnSec = 2.5f; }
		}

		for (const auto& spawn : enemySpawnInfos_)
		{
			const int waveIndex = std::max(0, spawn.wave - 1);

			WaveSpawnEntry entry{};
			entry.archetype = spawn.archetype;
			entry.position = spawn.position;

			const int spawnCount = std::max(1, spawn.count);
			for (int i = 0; i < spawnCount; ++i)
			{
				waves[static_cast<size_t>(waveIndex)].enemies.push_back(entry);
			}
		}
	}
	else
	{
		WaveDefinition wave;
		wave.delayBeforeSpawnSec = 0.0f;
		wave.enemies =
		{
			{ EnemyArchetype::RifleGrunt, { -10.0f, 1.0f, 20.0f } },
			{ EnemyArchetype::RifleGrunt, {  10.0f, 1.0f, 20.0f } },
		};
		waves.push_back(wave);
	}

	waveManager->SetWaves(waves);
}

void GamePlayStageContext::UnlockNextStage()
{
	auto& repo = StageRepository::GetInstance();

	auto stages = repo.GetStages();
	if (stages.empty())
	{
		stages = BuildDefaultStages();
	}

	const int clearedStageIndex = currentStageIndex_;
	if (clearedStageIndex < 0 || clearedStageIndex >= static_cast<int>(stages.size()))
	{
		repo.SetStages(stages);
		repo.SetStartIndex(0);
		return;
	}

	const int nextStageIndex = clearedStageIndex + 1;

	for (auto& stage : stages)
	{
		stage.justUnlocked = false;
	}

	if (nextStageIndex >= static_cast<int>(stages.size()))
	{
		repo.SetStages(stages);
		repo.SetStartIndex(clearedStageIndex);
		return;
	}

	stages[nextStageIndex].locked = false;
	stages[nextStageIndex].justUnlocked = true;

	repo.SetStages(stages);
	repo.SetStartIndex(nextStageIndex);
}