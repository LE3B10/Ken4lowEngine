#pragma once
#include "GamePlayStageContext.h"

/// -------------------------------------------------------------
///                 ステージ目的の進行状態管理クラス
/// -------------------------------------------------------------
class StageObjectiveManager
{
public: /// ---------- メンバ関数 ---------- ///

	void Initialize(const GamePlayStageContext& stageContext);
	void Update(float deltaTime);

	bool UsesWaveSystem() const { return stageRule_.useWaveSystem; }
	float GetStageElapsedSec() const { return stageElapsedSec_; }
	int GetActivatedDeviceCount() const { return activatedDeviceCount_; }
	int GetDevicePointCount() const { return devicePointCount_; }
	int GetDefenseTargetPointCount() const { return defenseTargetPointCount_; }
	int GetGoalPointCount() const { return goalPointCount_; }
	bool HasBossSpawnPoint() const { return hasBossSpawnPoint_; }
	bool HasReachedGoal() const { return reachedGoal_; }
	bool IsBossDefeated() const { return bossDefeated_; }
	bool IsDefenseTargetDestroyed() const { return defenseTargetDestroyed_; }
	bool IsStageObjectiveCleared(bool allWavesCleared) const;
	bool IsStageObjectiveFailed() const;

	// TODO: 実オブジェクト接触判定が入るまでの仮API。
	void AddActivatedDeviceCount(int amount = 1);
	// TODO: 実オブジェクト接触判定が入るまでの仮API。
	void SetReachedGoal(bool reached);
	// TODO: 実オブジェクト接触判定が入るまでの仮API。
	void SetBossDefeated(bool defeated);
	// TODO: 実オブジェクト破壊判定が入るまでの仮API。
	void SetDefenseTargetDestroyed(bool destroyed);

private: /// ---------- メンバ関数 ---------- ///

	int GetRequiredDeviceCount() const;

private: /// ---------- メンバ変数 ---------- ///

	GamePlayStageContext::StageRule stageRule_{};

	int devicePointCount_ = 0;
	int defenseTargetPointCount_ = 0;
	int goalPointCount_ = 0;
	bool hasBossSpawnPoint_ = false;

	int activatedDeviceCount_ = 0;
	float defendElapsedSec_ = 0.0f;
	float stageElapsedSec_ = 0.0f;

	bool reachedGoal_ = false;
	bool bossDefeated_ = false;
	bool defenseTargetDestroyed_ = false;
};
