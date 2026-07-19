#define NOMINMAX
#include "StageObjectiveManager.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

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
	allWavesCleared_ = false;
	activatedDeviceIds_.clear();
	status_ = Status::Active;
	RefreshSnapshot();
}

void StageObjectiveManager::Update(float deltaTime)
{
	if (status_ != Status::Active)
	{
		RefreshSnapshot();
		return;
	}

	const float safeDeltaTime = std::max(0.0f, deltaTime);
	stageElapsedSec_ += safeDeltaTime;

	if (stageRule_.objectiveType == GamePlayStageContext::StageObjectiveType::DefendTarget && !defenseTargetDestroyed_)
	{
		defendElapsedSec_ += safeDeltaTime;
	}

	RefreshOutcome();
}

bool StageObjectiveManager::IsStageObjectiveCleared(bool allWavesCleared)
{
	allWavesCleared_ = allWavesCleared;
	RefreshOutcome();
	return status_ == Status::Cleared;
}

bool StageObjectiveManager::IsStageObjectiveFailed()
{
	RefreshOutcome();
	return status_ == Status::Failed;
}

bool StageObjectiveManager::NotifyDeviceActivated(const std::string& deviceId)
{
	if (status_ != Status::Active || deviceId.empty())
	{
		return false;
	}

	const auto insertResult = activatedDeviceIds_.insert(deviceId);
	if (!insertResult.second)
	{
		return false;
	}

	activatedDeviceCount_ = std::max(activatedDeviceCount_ + 1, static_cast<int>(activatedDeviceIds_.size()));
	RefreshOutcome(); // 装置イベント直後に達成状態とHUD用進捗を更新する。
	return true;
}

void StageObjectiveManager::AddActivatedDeviceCount(int amount)
{
	if (status_ != Status::Active)
	{
		return;
	}

	activatedDeviceCount_ = std::max(0, activatedDeviceCount_ + amount);
	RefreshOutcome();
}

void StageObjectiveManager::SetReachedGoal(bool reached)
{
	if (status_ != Status::Active && reached)
	{
		return;
	}

	reachedGoal_ = reached;
	RefreshOutcome();
}

void StageObjectiveManager::SetBossDefeated(bool defeated)
{
	if (status_ != Status::Active && defeated)
	{
		return;
	}

	bossDefeated_ = defeated;
	RefreshOutcome();
}

void StageObjectiveManager::SetDefenseTargetDestroyed(bool destroyed)
{
	if (status_ != Status::Active && destroyed)
	{
		return;
	}

	defenseTargetDestroyed_ = destroyed;
	RefreshOutcome();
}

const char* StageObjectiveManager::GetStatusDebugName(Status status)
{
	switch (status)
	{
	case Status::Inactive: return "Inactive";
	case Status::Active: return "Active";
	case Status::Cleared: return "Cleared";
	case Status::Failed: return "Failed";
	default: return "Unknown";
	}
}

int StageObjectiveManager::GetRequiredDeviceCount() const
{
	const int configuredCount = (stageRule_.requiredDeviceCount > 0) ? stageRule_.requiredDeviceCount : devicePointCount_;
	return std::max(1, configuredCount);
}

bool StageObjectiveManager::EvaluateCleared() const
{
	switch (stageRule_.objectiveType)
	{
	case GamePlayStageContext::StageObjectiveType::ClearAllWaves:
		return allWavesCleared_;

	case GamePlayStageContext::StageObjectiveType::ActivateDevices:
		return activatedDeviceCount_ >= GetRequiredDeviceCount();

	case GamePlayStageContext::StageObjectiveType::DefendTarget:
		return !defenseTargetDestroyed_ && defendElapsedSec_ >= std::max(0.0f, stageRule_.defendTimeSec);

	case GamePlayStageContext::StageObjectiveType::ReachGoal:
		return reachedGoal_;

	case GamePlayStageContext::StageObjectiveType::DefeatBoss:
		return bossDefeated_;
	}

	return false;
}

bool StageObjectiveManager::EvaluateFailed() const
{
	switch (stageRule_.objectiveType)
	{
	case GamePlayStageContext::StageObjectiveType::DefendTarget:
		return defenseTargetDestroyed_;

	case GamePlayStageContext::StageObjectiveType::ReachGoal:
		return stageRule_.timeLimitSec > 0.0f && stageElapsedSec_ >= stageRule_.timeLimitSec && !reachedGoal_;

	default:
		return false;
	}
}

void StageObjectiveManager::RefreshOutcome()
{
	if (status_ == Status::Active)
	{
		if (EvaluateCleared())
		{
			TransitionTo(Status::Cleared);
			return;
		}
		if (EvaluateFailed())
		{
			TransitionTo(Status::Failed);
			return;
		}
	}

	RefreshSnapshot();
}

void StageObjectiveManager::TransitionTo(Status nextStatus)
{
	if (status_ == nextStatus)
	{
		RefreshSnapshot();
		return;
	}

	status_ = nextStatus;
	RefreshSnapshot();
	if (statusChangedCallback_)
	{
		statusChangedCallback_(status_, snapshot_);
	}
}

void StageObjectiveManager::RefreshSnapshot()
{
	snapshot_ = {};
	snapshot_.type = stageRule_.objectiveType;
	snapshot_.status = status_;
	snapshot_.title = GetObjectiveTitle(stageRule_.objectiveType);
	snapshot_.detail = BuildActiveDetail();
	snapshot_.elapsedSec = stageElapsedSec_;

	switch (stageRule_.objectiveType)
	{
	case GamePlayStageContext::StageObjectiveType::ClearAllWaves:
		snapshot_.currentValue = allWavesCleared_ ? 1 : 0;
		snapshot_.targetValue = 1;
		snapshot_.normalizedProgress = allWavesCleared_ ? 1.0f : 0.0f;
		break;

	case GamePlayStageContext::StageObjectiveType::ActivateDevices:
		snapshot_.usesCount = true;
		snapshot_.currentValue = activatedDeviceCount_;
		snapshot_.targetValue = GetRequiredDeviceCount();
		snapshot_.normalizedProgress = std::clamp(
			static_cast<float>(snapshot_.currentValue) / static_cast<float>(snapshot_.targetValue),
			0.0f,
			1.0f);
		break;

	case GamePlayStageContext::StageObjectiveType::DefendTarget:
	{
		const float targetSec = std::max(0.0f, stageRule_.defendTimeSec);
		snapshot_.usesTimer = true;
		snapshot_.currentValue = static_cast<int>(std::floor(defendElapsedSec_));
		snapshot_.targetValue = static_cast<int>(std::ceil(targetSec));
		snapshot_.remainingSec = std::max(0.0f, targetSec - defendElapsedSec_);
		snapshot_.normalizedProgress = targetSec > 0.0f ? std::clamp(defendElapsedSec_ / targetSec, 0.0f, 1.0f) : 1.0f;
		break;
	}

	case GamePlayStageContext::StageObjectiveType::ReachGoal:
		snapshot_.usesTimer = stageRule_.timeLimitSec > 0.0f;
		snapshot_.remainingSec = snapshot_.usesTimer ? std::max(0.0f, stageRule_.timeLimitSec - stageElapsedSec_) : 0.0f;
		snapshot_.normalizedProgress = reachedGoal_ ? 1.0f : 0.0f;
		break;

	case GamePlayStageContext::StageObjectiveType::DefeatBoss:
		snapshot_.currentValue = bossDefeated_ ? 1 : 0;
		snapshot_.targetValue = 1;
		snapshot_.normalizedProgress = bossDefeated_ ? 1.0f : 0.0f;
		break;
	}

	if (status_ == Status::Cleared)
	{
		snapshot_.detail = "目標達成";
		snapshot_.normalizedProgress = 1.0f;
	}
	else if (status_ == Status::Failed)
	{
		snapshot_.detail = "任務失敗";
	}
}

std::string StageObjectiveManager::BuildActiveDetail() const
{
	char text[96]{};
	switch (stageRule_.objectiveType)
	{
	case GamePlayStageContext::StageObjectiveType::ClearAllWaves:
		return "すべての敵ウェーブを撃破";

	case GamePlayStageContext::StageObjectiveType::ActivateDevices:
		std::snprintf(text, sizeof(text), "起動済み %d / %d", activatedDeviceCount_, GetRequiredDeviceCount());
		return text;

	case GamePlayStageContext::StageObjectiveType::DefendTarget:
		std::snprintf(text, sizeof(text), "残り %.0f 秒", std::max(0.0f, stageRule_.defendTimeSec - defendElapsedSec_));
		return text;

	case GamePlayStageContext::StageObjectiveType::ReachGoal:
		if (stageRule_.timeLimitSec > 0.0f)
		{
			std::snprintf(text, sizeof(text), "制限時間 残り %.0f 秒", std::max(0.0f, stageRule_.timeLimitSec - stageElapsedSec_));
			return text;
		}
		return "目標地点を探索中";

	case GamePlayStageContext::StageObjectiveType::DefeatBoss:
		return "ボス戦進行中";
	}

	return {};
}

const char* StageObjectiveManager::GetObjectiveTitle(GamePlayStageContext::StageObjectiveType type)
{
	switch (type)
	{
	case GamePlayStageContext::StageObjectiveType::ClearAllWaves: return "敵部隊を殲滅せよ";
	case GamePlayStageContext::StageObjectiveType::ActivateDevices: return "装置を起動せよ";
	case GamePlayStageContext::StageObjectiveType::DefendTarget: return "防衛対象を守り抜け";
	case GamePlayStageContext::StageObjectiveType::ReachGoal: return "脱出地点へ到達せよ";
	case GamePlayStageContext::StageObjectiveType::DefeatBoss: return "ボスを撃破せよ";
	default: return "ステージ目標を達成せよ";
	}
}
