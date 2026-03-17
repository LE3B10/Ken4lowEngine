#pragma once
#include "WaveManager.h"

#include <memory>
#include <string>
#include <vector>

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///				　	ゲームプレイのフロー制御
/// -------------------------------------------------------------
class GamePlayStageContext
{
public: /// ---------- 構造体 ---------- ///

	struct StageAssetPaths
	{
		std::string jsonPath;
		std::string modelPath;
	};

	struct EnemySpawnInfo
	{
		K4E::Vector3 position{ 0.0f, 0.0f, 0.0f };
		int wave = 1;
		int group = 0;
		int count = 1;
	};

	struct IntroCameraPointInfo
	{
		K4E::Vector3 position{ 0.0f, 0.0f, 0.0f };
		K4E::Vector3 rotation{ 0.0f, 0.0f, 0.0f };

		int order = 0;
		float duration = 1.5f;
		float fov = 45.0f;
		std::string targetName;
		std::string interpMode = "Linear";
		std::string aimMode = "Target";
	};

	struct IntroLookAtPointInfo
	{
		std::string name;
		K4E::Vector3 position{ 0.0f, 0.0f, 0.0f };
	};

public:
	void InitializeFromRepository();
	StageAssetPaths GetCurrentStageAssets() const;

	void LoadSpawnPointsFromLevel(const std::string& jsonPath);
	void SetupWaves(WaveManager* waveManager) const;
	void UnlockNextStage();

	int GetCurrentStageIndex() const { return currentStageIndex_; }

	bool HasPlayerSpawnPoint() const { return hasPlayerSpawnPoint_; }
	const K4E::Vector3& GetPlayerSpawnPoint() const { return playerSpawnPoint_; }

	bool HasBossSpawnPoint() const { return hasBossSpawnPoint_; }
	const K4E::Vector3& GetBossSpawnPoint() const { return bossSpawnPoint_; }

	const std::vector<EnemySpawnInfo>& GetEnemySpawnInfos() const { return enemySpawnInfos_; }
	const std::vector<IntroCameraPointInfo>& GetIntroCameraPoints() const { return introCameraPoints_; }
	const std::vector<IntroLookAtPointInfo>& GetIntroLookAtPoints() const { return introLookAtPoints_; }

private:
	StageAssetPaths GetStageAssetPaths(int stageIndex) const;

private:
	int currentStageIndex_ = 0;

	std::vector<EnemySpawnInfo> enemySpawnInfos_;

	K4E::Vector3 playerSpawnPoint_{ 0.0f, 0.0f, 0.0f };
	K4E::Vector3 bossSpawnPoint_{ 0.0f, 0.0f, 0.0f };
	bool hasPlayerSpawnPoint_ = false;
	bool hasBossSpawnPoint_ = false;

	std::vector<IntroCameraPointInfo> introCameraPoints_;
	std::vector<IntroLookAtPointInfo> introLookAtPoints_;
};

