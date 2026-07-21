#define NOMINMAX
#include "CrystalManager.h"

#include "ApplicationLayer/Character/Player/IPlayerRuntime.h"
#include "CharacterWorld.h"
#include "CameraManager.h"
#include "CollisionManager.h"
#include "EnemyBase.h"
#include "SkyBox.h"
#include "StageRepository.h"

#include <algorithm>
#include <cmath>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
	float Clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }

	float SaturateInverseLerp(float minValue, float maxValue, float value)
	{
		if (std::fabs(maxValue - minValue) <= 0.0001f) return value >= maxValue ? 1.0f : 0.0f;
		return Clamp01((value - minValue) / (maxValue - minValue));
	}
}

CrystalManager::~CrystalManager() { Finalize(); }

void CrystalManager::Initialize(
	const std::vector<CrystalSpawnPoint>& spawnPoints,
	CollisionManager* collisionManager,
	const std::vector<Ken4lowEngine::AABB>* floorAABBs,
	const std::vector<Ken4lowEngine::AABB>* obstacleAABBs)
{
	Finalize();

	baseLightingSettings_ = Ken4lowEngine::LightManager::GetInstance()->GetLightingSettings();
	crystalParameterController_.RegisterReactionParameters(BuildReactionParameterBinding());
	crystalParameterController_.ApplyReactionParameters(BuildReactionParameterBinding());
	crystalParameterController_.RegisterHpBarParameters(BuildHpBarParameterBinding());
	crystalParameterController_.ApplyHpBarParameters(BuildHpBarParameterBinding());
	crystalParameterController_.RegisterSkyColorParameters(BuildSkyColorParameterBinding());
	crystalParameterController_.ApplySkyColorParameters(BuildSkyColorParameterBinding());
	ApplySkyColor(normalSkyColor_);

	collisionManager_ = collisionManager;
	floorAABBs_ = floorAABBs;
	obstacleAABBs_ = obstacleAABBs;
	spawnPoints_ = spawnPoints;
	for (CrystalSpawnPoint& spawnPoint : spawnPoints_)
	{
		crystalParameterController_.RegisterCrystalParameters(spawnPoint, [this]() { SyncCrystalsFromParameterManager(); });
		crystalParameterController_.ApplyParameterToSpawnPoint(spawnPoint);
	}

	crystals_.reserve(spawnPoints_.size());
	for (const CrystalSpawnPoint& spawnPoint : spawnPoints_)
	{
		EnemySpawnCrystal crystal;
		crystal.Initialize(spawnPoint, floorAABBs_, obstacleAABBs_);
		crystals_.push_back(std::move(crystal));
	}
	crystalHpBarController_.Initialize(crystals_.size());

	if (collisionManager_)
	{
		for (EnemySpawnCrystal& crystal : crystals_) collisionManager_->AddCollider(&crystal);
	}

	selectedCrystalIndex_ = 0;
	nextSpawnCrystalIndex_ = 0;
	enableCrystalEnemySpawn_ = true;
	maxSpawnPerInterval_ = 1;
	difficultyRuntime_ = {};
	difficultyRuntime_.maxAliveEnemiesPerCrystal = difficultySettings_.normalMaxAlivePerCrystal;
	difficultyRuntime_.maxAliveEnemiesTotal = difficultySettings_.normalMaxAliveEnemiesTotal;
	hasCrystalBroken_ = false;
	isFinalPhaseReady_ = false;
	requestBossAppear_ = false;
	worldColorChanging_ = false;
	worldColorChangeComplete_ = false;
	worldColorChangeTimer_ = 0.0f;
	debugAliveNormalEnemyCount_ = 0;
	debugBossSpawnConditionMet_ = false;
	debugBossSpawned_ = false;
	debugBossSpawnPosition_ = {};
}

void CrystalManager::Finalize()
{
	if (collisionManager_)
	{
		for (EnemySpawnCrystal& crystal : crystals_) collisionManager_->RemoveCollider(&crystal);
	}
	crystalParameterController_.UnregisterCrystalParameters();
	crystalParameterController_.UnregisterReactionParameters();
	crystalParameterController_.UnregisterHpBarParameters();
	crystalParameterController_.UnregisterSkyColorParameters();
	RestoreWorldColor();
	crystals_.clear();
	crystalHpBarController_.Finalize();
	spawnPoints_.clear();
	collisionManager_ = nullptr;
	floorAABBs_ = nullptr;
	obstacleAABBs_ = nullptr;
}

void CrystalManager::Update(CharacterWorld& characters, float deltaTime)
{
	SyncCrystalsFromParameterManager();
	crystalParameterController_.ApplyReactionParameters(BuildReactionParameterBinding());
	crystalParameterController_.ApplyHpBarParameters(BuildHpBarParameterBinding());
	crystalParameterController_.ApplySkyColorParameters(BuildSkyColorParameterBinding());

	for (EnemySpawnCrystal& crystal : crystals_) crystal.Update(characters, deltaTime, reactionSettings_);
	UpdateDifficultyDirector(characters, deltaTime);
	ApplyDifficultyDirectorToCrystals();
	HandleCrystalBreakEvents();
	UpdateWorldColorChange(deltaTime);
	UpdateSkyColorChange(deltaTime);

	if (!enableCrystalEnemySpawn_ || AreAllCrystalsDestroyed() || GetAliveCrystalCount() <= 0) return;

	const float safeDeltaTime = std::clamp(deltaTime, 0.0f, kMaxUpdateDeltaTime);
	for (EnemySpawnCrystal& crystal : crystals_) crystal.AdvanceSpawnTimer(safeDeltaTime);

	for (int spawnedCount = 0; spawnedCount < maxSpawnPerInterval_; ++spawnedCount)
	{
		if (characters.GetAliveNormalEnemyCount() >= difficultyRuntime_.maxAliveEnemiesTotal) break;
		EnemySpawnCrystal* crystal = FindNextSpawnableCrystal();
		if (!crystal) break;

		if (crystal->GetSpawnPattern() == "Burst")
		{
			while (crystal->CanSpawnEnemy() && characters.GetAliveNormalEnemyCount() < difficultyRuntime_.maxAliveEnemiesTotal)
			{
				crystal->SpawnEnemy(characters, difficultyRuntime_.enemyMoveSpeedMultiplier, difficultyRuntime_.attackCooldownMultiplier, difficultyRuntime_.damageMultiplier);
			}
		}
		else
		{
			crystal->SpawnEnemy(characters, difficultyRuntime_.enemyMoveSpeedMultiplier, difficultyRuntime_.attackCooldownMultiplier, difficultyRuntime_.damageMultiplier);
		}
		crystal->ConsumeSpawnTimer();
	}
}

void CrystalManager::UpdatePresentationOnly(CharacterWorld& characters, float deltaTime)
{
	SyncCrystalsFromParameterManager();
	crystalParameterController_.ApplyReactionParameters(BuildReactionParameterBinding());
	for (EnemySpawnCrystal& crystal : crystals_) crystal.Update(characters, deltaTime, reactionSettings_);
	HandleCrystalBreakEvents();
	UpdateWorldColorChange(deltaTime);
	UpdateSkyColorChange(deltaTime);
}

void CrystalManager::Draw() const
{
	for (const EnemySpawnCrystal& crystal : crystals_) crystal.Draw();
}

void CrystalManager::UpdateHpBars(
	const Ken4lowEngine::Matrix4x4& viewMatrix,
	const Ken4lowEngine::Matrix4x4& projMatrix,
	float screenWidth,
	float screenHeight,
	float deltaTime,
	const EnemySpawnCrystal* aimedCrystal,
	bool showOnlyWhenAimed,
	float visibleHoldTime)
{
	crystalHpBarController_.Update(crystals_, viewMatrix, projMatrix, screenWidth, screenHeight, deltaTime, aimedCrystal, showOnlyWhenAimed, visibleHoldTime);
}

void CrystalManager::DrawHpBars() { crystalHpBarController_.Draw(); }

void CrystalManager::SetStage1BeginnerBalanceEnabled(bool enabled)
{
	stage1BeginnerBalanceEnabled_ = enabled;
	if (!stage1BeginnerBalanceEnabled_)
	{
		if (collisionManager_)
		{
			for (EnemySpawnCrystal& crystal : crystals_) collisionManager_->RemoveCollider(&crystal);
		}
		crystalParameterController_.UnregisterCrystalParameters();
		crystals_.clear();
		spawnPoints_.clear();
		crystalHpBarController_.Finalize();
		enableCrystalEnemySpawn_ = false;
		selectedCrystalIndex_ = 0;
		nextSpawnCrystalIndex_ = 0;
		hasCrystalBroken_ = false;
		isFinalPhaseReady_ = false;
		requestBossAppear_ = false; // Stage2以降ではStage1専用の見た目・Collider・敵生成・ボス要求を残さない。
		RestoreWorldColor();
		return;
	}
	for (size_t i = 0; i < spawnPoints_.size() && i < crystals_.size(); ++i)
	{
		ApplyStage1BeginnerBalance(spawnPoints_[i]);
		crystals_[i].ApplyInitialHpSettings(spawnPoints_[i]);
		SyncCrystalFromSpawnPoint(i);
	}
}

void CrystalManager::SetFirstAliveCrystalGuideHighlight(float alpha)
{
	const float highlightAlpha = std::clamp(alpha, 0.0f, 1.0f);
	bool applied = false;
	for (EnemySpawnCrystal& crystal : crystals_)
	{
		const bool shouldHighlight = !applied && crystal.IsAlive();
		crystal.SetGuideHighlight(shouldHighlight ? highlightAlpha : 0.0f);
		if (shouldHighlight) applied = true;
	}
}

int CrystalManager::GetAliveCrystalCount() const
{
	return static_cast<int>(std::count_if(crystals_.begin(), crystals_.end(), [](const EnemySpawnCrystal& crystal) { return crystal.IsAlive(); }));
}

const EnemySpawnCrystal* CrystalManager::GetFirstAliveCrystal() const
{
	for (const EnemySpawnCrystal& crystal : crystals_) if (crystal.IsAlive()) return &crystal;
	return nullptr;
}

bool CrystalManager::TryGetFirstAliveCrystalPosition(K4E::Vector3& outPosition) const
{
	if (const EnemySpawnCrystal* crystal = GetFirstAliveCrystal())
	{
		outPosition = crystal->GetPosition();
		return true;
	}
	return false;
}

int CrystalManager::GetAliveCrystalSpawnEnemyCount() const
{
	int totalCount = 0;
	for (const EnemySpawnCrystal& crystal : crystals_) totalCount += crystal.GetAliveSpawnedEnemyCount();
	return totalCount;
}

bool CrystalManager::AreAllCrystalsDestroyed() const
{
	if (!crystals_.empty()) return GetAliveCrystalCount() == 0;
	const int stageIndex = StageRepository::GetInstance().GetStartIndex().value_or(-1);
	return stageIndex == 4; // 最終StageだけはCrystal無しをBoss開始条件として扱い、純粋な一戦構成にする。
}

void CrystalManager::SetProgressDebugStatus(int aliveNormalEnemyCount, bool bossSpawnConditionMet, bool bossSpawned, const Ken4lowEngine::Vector3& bossSpawnPosition)
{
	debugAliveNormalEnemyCount_ = aliveNormalEnemyCount;
	debugBossSpawnConditionMet_ = bossSpawnConditionMet;
	debugBossSpawned_ = bossSpawned;
	debugBossSpawnPosition_ = bossSpawnPosition;
}
