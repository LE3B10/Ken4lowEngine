#pragma once
#include "Vector3.h"
#include "StageMissionFactory.h"
#include "StageType.h"
#include <string>

class GamePlayWorld;
class GamePlayStageContext;

namespace K4E = ::Ken4lowEngine;

struct StageMissionConfig
{
	StageType stageType = StageType::Wave;
	K4E::Vector3 clearPoint{ 0.0f, 0.0f, 0.0f };
	float clearRadius = 3.0f;
	float defenseTime = 60.0f;
	float defenseTargetHp = 100.0f;
	K4E::Vector3 escapePoint{ 0.0f, 0.0f, 0.0f };
	float escapeRadius = 3.0f;
	float timeLimit = 0.0f;
	std::string bossSpawnName;
	int bossEnemyId = -1;
};

class IStageMission
{
public:
	virtual ~IStageMission() = default;
	virtual void Initialize(GamePlayWorld* world, const GamePlayStageContext& stageContext, const StageMissionConfig& config) = 0;
	virtual void Update(float dt) = 0;
	virtual void DrawDebugImGui() = 0;
	virtual bool IsCleared() const = 0;
	virtual bool IsFailed() const = 0;
	virtual const char* GetDebugName() const = 0;
	virtual StageType GetStageType() const = 0;
};

class StageMissionBase : public IStageMission
{
public:
	void Initialize(GamePlayWorld* world, const GamePlayStageContext& stageContext, const StageMissionConfig& config) override;
	void DrawDebugImGui() override;
	bool IsCleared() const override { return isCleared_; }
	bool IsFailed() const override { return isFailed_; }
	StageType GetStageType() const override { return config_.stageType; }

protected:
	GamePlayWorld* world_ = nullptr;
	const GamePlayStageContext* stageContext_ = nullptr;
	StageMissionConfig config_{};
	bool isCleared_ = false;
	bool isFailed_ = false;
};

