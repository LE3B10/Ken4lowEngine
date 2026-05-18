#define NOMINMAX
#include "StageObjectiveManager.h"

#include <algorithm>

void StageObjectiveManager::Initialize(const GamePlayStageContext& stageContext)
{
	stageRule_ = stageContext.GetCurrentStageRule();

	// ステージ配置点の個数を保持し、ルール未指定時の目的達成数に使う。
	devicePointCount_ = static_cast<int>(stageContext.GetDevicePoints().size());
	defenseTargetPointCount_ = static_cast<int>(stageContext.GetDefenseTargetPoints().size());
	goalPointCount_ = static_cast<int>(stageContext.GetGoalPoints().size());
	hasBossSpawnPoint_ = stageContext.HasBossSpawnPoint();

	activatedDeviceCount_ = 0;
	defendElapsedSec_ = 0.0f;
	stageElapsedSec_ = 0.0f;
	reachedGoal_ = false;
	bossDefeated_ = false;
	defenseTargetDestroyed_ = false;
}

void StageObjectiveManager::Update(float deltaTime)
{
	stageElapsedSec_ += deltaTime;

	switch (stageRule_.objectiveType)
	{
	case GamePlayStageContext::StageObjectiveType::DefendTarget:
		// 防衛中は対象が破壊されていない時間だけをクリア判定に積む。
		if (!defenseTargetDestroyed_)
		{
			defendElapsedSec_ += deltaTime;
		}
		break;

	case GamePlayStageContext::StageObjectiveType::ReachGoal:
	case GamePlayStageContext::StageObjectiveType::ActivateDevices:
	case GamePlayStageContext::StageObjectiveType::DefeatBoss:
	case GamePlayStageContext::StageObjectiveType::ClearAllWaves:
	default:
		break;
	}
}

bool StageObjectiveManager::IsStageObjectiveCleared(bool allWavesCleared) const
{
	switch (stageRule_.objectiveType)
	{
	case GamePlayStageContext::StageObjectiveType::ClearAllWaves:
		// WAVEは既存WaveManagerの全ウェーブクリア状態をそのまま採用する。
		return allWavesCleared;

	case GamePlayStageContext::StageObjectiveType::ActivateDevices:
		// SEARCHは必要数が未指定ならDevicePoint数をクリア条件にする。
		return activatedDeviceCount_ >= GetRequiredDeviceCount();

	case GamePlayStageContext::StageObjectiveType::DefendTarget:
		// DEFENSEはDefenseTargetPointが破壊されず防衛時間を満たしたらクリアにする。
		return !defenseTargetDestroyed_ && defendElapsedSec_ >= stageRule_.defendTimeSec;

	case GamePlayStageContext::StageObjectiveType::ReachGoal:
		// ESCAPEはGoalPoint到達通知をクリア条件にする。
		return reachedGoal_;

	case GamePlayStageContext::StageObjectiveType::DefeatBoss:
		// BOSSはBossSpawnPointから出したボスの撃破通知をクリア条件にする。
		return bossDefeated_;
	}

	return false;
}

bool StageObjectiveManager::IsStageObjectiveFailed() const
{
	switch (stageRule_.objectiveType)
	{
	case GamePlayStageContext::StageObjectiveType::DefendTarget:
		// DEFENSEは防衛対象破壊通知を失敗条件にする。
		return defenseTargetDestroyed_;

	case GamePlayStageContext::StageObjectiveType::ReachGoal:
		// ESCAPEは制限時間ありのステージだけ時間切れを失敗にする。
		return (stageRule_.timeLimitSec > 0.0f) && (stageElapsedSec_ >= stageRule_.timeLimitSec) && !reachedGoal_;

	default:
		return false;
	}
}

void StageObjectiveManager::AddActivatedDeviceCount(int amount)
{
	activatedDeviceCount_ = std::max(0, activatedDeviceCount_ + amount);
}

void StageObjectiveManager::SetReachedGoal(bool reached)
{
	reachedGoal_ = reached;
}

void StageObjectiveManager::SetBossDefeated(bool defeated)
{
	bossDefeated_ = defeated;
}

void StageObjectiveManager::SetDefenseTargetDestroyed(bool destroyed)
{
	defenseTargetDestroyed_ = destroyed;
}

int StageObjectiveManager::GetRequiredDeviceCount() const
{
	return (stageRule_.requiredDeviceCount > 0) ? stageRule_.requiredDeviceCount : devicePointCount_;
}
