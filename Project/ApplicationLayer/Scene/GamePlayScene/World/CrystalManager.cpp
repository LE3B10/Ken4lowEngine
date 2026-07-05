#define NOMINMAX
#include "CrystalManager.h"

#include "CharacterWorld.h"
#include "CameraManager.h"
#include "CollisionManager.h"
#include "EnemyBase.h"
#include "Player.h"
#include "SkyBox.h"

#include <algorithm>
#include <cmath>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
	float Clamp01(float value)
	{
		return std::clamp(value, 0.0f, 1.0f);
	}

	float SaturateInverseLerp(float minValue, float maxValue, float value)
	{
		if (std::fabs(maxValue - minValue) <= 0.0001f)
		{
			return value >= maxValue ? 1.0f : 0.0f;
		}
		return Clamp01((value - minValue) / (maxValue - minValue));
	}
}

CrystalManager::~CrystalManager()
{
	Finalize();
}

void CrystalManager::Initialize(const std::vector<CrystalSpawnPoint>& spawnPoints, CollisionManager* collisionManager, const std::vector<Ken4lowEngine::AABB>* floorAABBs, const std::vector<Ken4lowEngine::AABB>* obstacleAABBs)
{
	Finalize();

	baseLightingSettings_ = Ken4lowEngine::LightManager::GetInstance()->GetLightingSettings();
	// ParameterManager関連は専用Controllerに任せ、CrystalManagerは生成と進行管理に集中する。
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
	// クリスタル数に合わせてHPバーを用意し、表示管理は専用Controllerへ任せる。
	crystalHpBarController_.Initialize(crystals_.size());

	if (collisionManager_)
	{
		for (EnemySpawnCrystal& crystal : crystals_)
		{
			collisionManager_->AddCollider(&crystal);
		}
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
		for (EnemySpawnCrystal& crystal : crystals_)
		{
			collisionManager_->RemoveCollider(&crystal);
		}
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

	for (EnemySpawnCrystal& crystal : crystals_)
	{
		crystal.Update(characters, deltaTime, reactionSettings_);
	}
	UpdateDifficultyDirector(characters, deltaTime);
	ApplyDifficultyDirectorToCrystals();
	HandleCrystalBreakEvents();
	UpdateWorldColorChange(deltaTime);
	UpdateSkyColorChange(deltaTime);

	if (!enableCrystalEnemySpawn_ || AreAllCrystalsDestroyed() || GetAliveCrystalCount() <= 0)
	{
		return;
	}

	// フレーム落ち時にスポーン更新が一気に進まないよう、Crystal更新用deltaTimeを制限する。
	const float safeDeltaTime = std::clamp(deltaTime, 0.0f, kMaxUpdateDeltaTime);
	for (EnemySpawnCrystal& crystal : crystals_)
	{
		crystal.AdvanceSpawnTimer(safeDeltaTime);
	}

	// 各クリスタルのTransformをスポーン基準にして、個別の間隔/初回遅延で敵を補充する。
	for (int spawnedCount = 0; spawnedCount < maxSpawnPerInterval_; ++spawnedCount)
	{
		// 全体敵数上限で画面内の敵数を抑え、Pressure上昇時も無限増殖しないようにする。
		if (characters.GetAliveNormalEnemyCount() >= difficultyRuntime_.maxAliveEnemiesTotal)
		{
			break;
		}

		EnemySpawnCrystal* crystal = FindNextSpawnableCrystal();
		if (!crystal)
		{
			break;
		}

		if (crystal->GetSpawnPattern() == "Burst")
		{
			while (crystal->CanSpawnEnemy() && characters.GetAliveNormalEnemyCount() < difficultyRuntime_.maxAliveEnemiesTotal)
			{
				crystal->SpawnEnemy(
					characters,
					difficultyRuntime_.enemyMoveSpeedMultiplier,
					difficultyRuntime_.attackCooldownMultiplier,
					difficultyRuntime_.damageMultiplier);
			}
		}
		else
		{
			crystal->SpawnEnemy(
				characters,
				difficultyRuntime_.enemyMoveSpeedMultiplier,
				difficultyRuntime_.attackCooldownMultiplier,
				difficultyRuntime_.damageMultiplier);
		}
		crystal->ConsumeSpawnTimer();
	}
}

void CrystalManager::UpdatePresentationOnly(CharacterWorld& characters, float deltaTime)
{
	SyncCrystalsFromParameterManager();
	crystalParameterController_.ApplyReactionParameters(BuildReactionParameterBinding());

	for (EnemySpawnCrystal& crystal : crystals_)
	{
		crystal.Update(characters, deltaTime, reactionSettings_);
	}

	HandleCrystalBreakEvents();
	UpdateWorldColorChange(deltaTime);
	UpdateSkyColorChange(deltaTime);
}

void CrystalManager::Draw() const
{
	for (const EnemySpawnCrystal& crystal : crystals_)
	{
		crystal.Draw();
	}
}

void CrystalManager::UpdateHpBars(const Ken4lowEngine::Matrix4x4& viewMatrix, const Ken4lowEngine::Matrix4x4& projMatrix, float screenWidth, float screenHeight, float deltaTime, const EnemySpawnCrystal* aimedCrystal, bool showOnlyWhenAimed, float visibleHoldTime)
{
	// クリスタルHPバーの投影・表示判定は専用Controllerへ委譲する。
	crystalHpBarController_.Update(crystals_, viewMatrix, projMatrix, screenWidth, screenHeight, deltaTime, aimedCrystal, showOnlyWhenAimed, visibleHoldTime);
}

void CrystalManager::DrawHpBars()
{
	crystalHpBarController_.Draw();
}

void CrystalManager::SetStage1BeginnerBalanceEnabled(bool enabled)
{
	stage1BeginnerBalanceEnabled_ = enabled;

	if (!stage1BeginnerBalanceEnabled_)
	{
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
		// ステージ1開始案内では、最初に壊す対象だけを強調して迷いを減らす。
		const bool shouldHighlight = !applied && crystal.IsAlive();
		crystal.SetGuideHighlight(shouldHighlight ? highlightAlpha : 0.0f);
		if (shouldHighlight)
		{
			applied = true;
		}
	}
}

int CrystalManager::GetAliveCrystalCount() const
{
	return static_cast<int>(std::count_if(crystals_.begin(), crystals_.end(),
		[](const EnemySpawnCrystal& crystal) { return crystal.IsAlive(); }));
}

const EnemySpawnCrystal* CrystalManager::GetFirstAliveCrystal() const
{
	for (const EnemySpawnCrystal& crystal : crystals_)
	{
		if (crystal.IsAlive())
		{
			return &crystal;
		}
	}
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
	for (const EnemySpawnCrystal& crystal : crystals_)
	{
		totalCount += crystal.GetAliveSpawnedEnemyCount();
	}
	return totalCount;
}

bool CrystalManager::AreAllCrystalsDestroyed() const
{
	return !crystals_.empty() && GetAliveCrystalCount() == 0;
}

void CrystalManager::SetProgressDebugStatus(int aliveNormalEnemyCount, bool bossSpawnConditionMet, bool bossSpawned, const Ken4lowEngine::Vector3& bossSpawnPosition)
{
	debugAliveNormalEnemyCount_ = aliveNormalEnemyCount;
	debugBossSpawnConditionMet_ = bossSpawnConditionMet;
	debugBossSpawned_ = bossSpawned;
	debugBossSpawnPosition_ = bossSpawnPosition;
}

EnemySpawnCrystal* CrystalManager::GetSelectedCrystal()
{
	return selectedCrystalIndex_ < crystals_.size() ? &crystals_[selectedCrystalIndex_] : nullptr;
}

const EnemySpawnCrystal* CrystalManager::GetSelectedCrystal() const
{
	return selectedCrystalIndex_ < crystals_.size() ? &crystals_[selectedCrystalIndex_] : nullptr;
}

EnemySpawnCrystal* CrystalManager::FindNextSpawnableCrystal()
{
	for (size_t offset = 0; offset < crystals_.size(); ++offset)
	{
		const size_t crystalIndex = (nextSpawnCrystalIndex_ + offset) % crystals_.size();
		if (crystals_[crystalIndex].IsSpawnReady())
		{
			nextSpawnCrystalIndex_ = (crystalIndex + 1) % crystals_.size();
			return &crystals_[crystalIndex];
		}
	}
	return nullptr;
}

void CrystalManager::ApplyStage1BeginnerBalance(CrystalSpawnPoint& spawnPoint)
{
	// ステージ1では弾切れや被弾に慣れていないプレイヤーでも進めるよう、クリスタル由来の敵を控えめにする。
	spawnPoint.spawnEnemyType = EnemyType::Melee;
	spawnPoint.hp = 350;
	spawnPoint.maxHp = 350;
	spawnPoint.spawnInterval = 7.0f;
	spawnPoint.initialDelay = 7.0f;
	spawnPoint.maxSpawnCount = 0;
	spawnPoint.maxAliveEnemies = difficultyRuntime_.maxAliveEnemiesPerCrystal;
	spawnPoint.spawnRadius = 4.0f;
	spawnPoint.spawnPattern = "Interval";
	spawnPoint.enableInfiniteSpawn = spawnPoint.isActive;
}

void CrystalManager::UpdateDifficultyDirector(CharacterWorld& characters, float deltaTime)
{
	if (!stage1BeginnerBalanceEnabled_ || !difficultyDirectorEnabled_)
	{
		difficultyRuntime_.level = PressureLevel::NormalPressure;
		difficultyRuntime_.pressureScore = 0.0f;
		difficultyRuntime_.maxAliveEnemiesPerCrystal = difficultySettings_.normalMaxAlivePerCrystal;
		difficultyRuntime_.maxAliveEnemiesTotal = difficultySettings_.normalMaxAliveEnemiesTotal;
		difficultyRuntime_.spawnIntervalMultiplier = difficultySettings_.normalSpawnIntervalMultiplier;
		difficultyRuntime_.enemyMoveSpeedMultiplier = difficultySettings_.normalEnemyMoveSpeedMultiplier;
		difficultyRuntime_.attackCooldownMultiplier = difficultySettings_.normalAttackCooldownMultiplier;
		difficultyRuntime_.damageMultiplier = difficultySettings_.normalDamageMultiplier;
		return;
	}

	Player* player = characters.GetPlayer();
	const float previousHpRate = difficultyRuntime_.playerHpRate;
	difficultyRuntime_.playerHpRate = player && player->GetMaxHP() > 0.0f
		? std::clamp(player->GetHP() / player->GetMaxHP(), 0.0f, 1.0f)
		: 1.0f;

	if (difficultyRuntime_.playerHpRate < previousHpRate - 0.001f)
	{
		difficultyRuntime_.noDamageTimer = 0.0f;
	}
	else
	{
		difficultyRuntime_.noDamageTimer += deltaTime;
	}

	const int aliveEnemies = characters.GetAliveNormalEnemyCount();
	if (aliveEnemies < difficultyRuntime_.previousAliveEnemyCount)
	{
		difficultyRuntime_.recentKillCount += difficultyRuntime_.previousAliveEnemyCount - aliveEnemies;
		difficultyRuntime_.fastKillTimer = 0.0f;
	}
	else
	{
		difficultyRuntime_.fastKillTimer += deltaTime;
		if (difficultyRuntime_.fastKillTimer > difficultySettings_.fastKillWindow)
		{
			difficultyRuntime_.recentKillCount = 0;
			difficultyRuntime_.fastKillTimer = 0.0f;
		}
	}
	difficultyRuntime_.previousAliveEnemyCount = aliveEnemies;

	const float hpComfort = SaturateInverseLerp(difficultySettings_.lowHpRate, difficultySettings_.comfortableHpRate, difficultyRuntime_.playerHpRate);
	const float noDamageComfort = SaturateInverseLerp(0.0f, difficultySettings_.noDamageComfortTime, difficultyRuntime_.noDamageTimer);
	const float killComfort = Clamp01(static_cast<float>(difficultyRuntime_.recentKillCount) / 3.0f);
	const float lowEnemyBonus = 1.0f - Clamp01(static_cast<float>(aliveEnemies) / static_cast<float>(std::max(1, difficultyRuntime_.maxAliveEnemiesTotal)));
	const float crystalPressure = Clamp01(static_cast<float>(GetAliveCrystalCount()) / static_cast<float>(std::max(1, GetCrystalCount())));

	// プレイヤーが余裕なら圧を上げ、危ない時は下げて緩急を作る。
	float pressureScore =
		hpComfort * 0.35f +
		noDamageComfort * 0.25f +
		killComfort * 0.20f +
		lowEnemyBonus * 0.15f +
		crystalPressure * 0.05f;
	if (difficultyRuntime_.playerHpRate <= difficultySettings_.lowHpRate)
	{
		pressureScore -= 0.35f;
	}
	difficultyRuntime_.pressureScore = std::clamp(pressureScore, 0.0f, 1.0f);

	if (difficultyRuntime_.playerHpRate <= difficultySettings_.lowHpRate)
	{
		difficultyRuntime_.level = PressureLevel::EasyPressure;
	}
	else if (difficultyRuntime_.pressureScore >= 0.82f && difficultyRuntime_.noDamageTimer >= difficultySettings_.noDamagePanicTime)
	{
		difficultyRuntime_.level = PressureLevel::PanicPressure;
	}
	else if (difficultyRuntime_.pressureScore >= 0.58f)
	{
		difficultyRuntime_.level = PressureLevel::HighPressure;
	}
	else
	{
		difficultyRuntime_.level = PressureLevel::NormalPressure;
	}

	switch (difficultyRuntime_.level)
	{
	case PressureLevel::EasyPressure:
		difficultyRuntime_.maxAliveEnemiesPerCrystal = difficultySettings_.easyMaxAlivePerCrystal;
		difficultyRuntime_.maxAliveEnemiesTotal = difficultySettings_.easyMaxAliveEnemiesTotal;
		difficultyRuntime_.spawnIntervalMultiplier = difficultySettings_.easySpawnIntervalMultiplier;
		difficultyRuntime_.enemyMoveSpeedMultiplier = difficultySettings_.easyEnemyMoveSpeedMultiplier;
		difficultyRuntime_.attackCooldownMultiplier = difficultySettings_.easyAttackCooldownMultiplier;
		difficultyRuntime_.damageMultiplier = difficultySettings_.easyDamageMultiplier;
		break;
	case PressureLevel::HighPressure:
		difficultyRuntime_.maxAliveEnemiesPerCrystal = difficultySettings_.highMaxAlivePerCrystal;
		difficultyRuntime_.maxAliveEnemiesTotal = difficultySettings_.highMaxAliveEnemiesTotal;
		difficultyRuntime_.spawnIntervalMultiplier = difficultySettings_.highSpawnIntervalMultiplier;
		difficultyRuntime_.enemyMoveSpeedMultiplier = difficultySettings_.highEnemyMoveSpeedMultiplier;
		difficultyRuntime_.attackCooldownMultiplier = difficultySettings_.highAttackCooldownMultiplier;
		difficultyRuntime_.damageMultiplier = difficultySettings_.highDamageMultiplier;
		break;
	case PressureLevel::PanicPressure:
		difficultyRuntime_.maxAliveEnemiesPerCrystal = difficultySettings_.panicMaxAlivePerCrystal;
		difficultyRuntime_.maxAliveEnemiesTotal = difficultySettings_.panicMaxAliveEnemiesTotal;
		difficultyRuntime_.spawnIntervalMultiplier = difficultySettings_.panicSpawnIntervalMultiplier;
		difficultyRuntime_.enemyMoveSpeedMultiplier = difficultySettings_.panicEnemyMoveSpeedMultiplier;
		difficultyRuntime_.attackCooldownMultiplier = difficultySettings_.panicAttackCooldownMultiplier;
		difficultyRuntime_.damageMultiplier = difficultySettings_.panicDamageMultiplier;
		break;
	case PressureLevel::NormalPressure:
	default:
		difficultyRuntime_.maxAliveEnemiesPerCrystal = difficultySettings_.normalMaxAlivePerCrystal;
		difficultyRuntime_.maxAliveEnemiesTotal = difficultySettings_.normalMaxAliveEnemiesTotal;
		difficultyRuntime_.spawnIntervalMultiplier = difficultySettings_.normalSpawnIntervalMultiplier;
		difficultyRuntime_.enemyMoveSpeedMultiplier = difficultySettings_.normalEnemyMoveSpeedMultiplier;
		difficultyRuntime_.attackCooldownMultiplier = difficultySettings_.normalAttackCooldownMultiplier;
		difficultyRuntime_.damageMultiplier = difficultySettings_.normalDamageMultiplier;
		break;
	}
}

void CrystalManager::ApplyDifficultyDirectorToCrystals()
{
	if (!stage1BeginnerBalanceEnabled_)
	{
		return;
	}

	for (size_t i = 0; i < crystals_.size() && i < spawnPoints_.size(); ++i)
	{
		// クリスタル単位は同時生存数だけを変え、累計スポーン数では通常進行を止めない。
		crystals_[i].SetMaxAliveEnemies(difficultyRuntime_.maxAliveEnemiesPerCrystal);
		crystals_[i].SetSpawnInterval(spawnPoints_[i].spawnInterval * difficultyRuntime_.spawnIntervalMultiplier);
	}
}

const char* CrystalManager::GetPressureLevelName() const
{
	switch (difficultyRuntime_.level)
	{
	case PressureLevel::EasyPressure: return "EasyPressure";
	case PressureLevel::HighPressure: return "HighPressure";
	case PressureLevel::PanicPressure: return "PanicPressure";
	case PressureLevel::NormalPressure:
	default: return "NormalPressure";
	}
}

void CrystalManager::SyncCrystalsFromParameterManager()
{
	for (size_t i = 0; i < spawnPoints_.size() && i < crystals_.size(); ++i)
	{
		crystalParameterController_.ApplyParameterToSpawnPoint(spawnPoints_[i]);
		if (stage1BeginnerBalanceEnabled_)
		{
			ApplyStage1BeginnerBalance(spawnPoints_[i]);
		}
		SyncCrystalFromSpawnPoint(i);
	}
}

void CrystalManager::SyncCrystalFromSpawnPoint(size_t index)
{
	if (index >= spawnPoints_.size() || index >= crystals_.size())
	{
		return;
	}

	// ParameterManagerのTransformをクリスタル本体へ反映し、描画・Collider・敵スポーン位置を同じ座標へ揃える。
	crystals_[index].ApplySpawnerSettings(spawnPoints_[index], floorAABBs_, obstacleAABBs_);
}

CrystalParameterController::ReactionBinding CrystalManager::BuildReactionParameterBinding()
{
	return {
		&reactionSettings_,
		&worldColorChangeTime_,
		&worldDarkness_,
		&worldRedTint_
	};
}

CrystalParameterController::HpBarBinding CrystalManager::BuildHpBarParameterBinding()
{
	CrystalHpBarController::Settings& settings = crystalHpBarController_.GetSettings();
	return {
		&settings.visible,
		&settings.alwaysVisible,
		&settings.offsetY,
		&settings.width,
		&settings.height,
		&settings.showTime
	};
}

CrystalParameterController::SkyColorBinding CrystalManager::BuildSkyColorParameterBinding()
{
	return {
		&skyColorChangeEnabled_,
		&changeSkyOnAllCrystalsBroken_,
		&skyColorChangeTime_,
		&normalSkyColor_,
		&brokenSkyColor_,
		&skyDarkness_,
		&skyRedTint_,
		&skyPurpleTint_
	};
}

void CrystalManager::HandleCrystalBreakEvents()
{
	for (EnemySpawnCrystal& crystal : crystals_)
	{
		if (!crystal.WasJustBroken())
		{
			continue;
		}

		hasCrystalBroken_ = true;
		requestBossAppear_ = AreAllCrystalsDestroyed();
		isFinalPhaseReady_ = requestBossAppear_;
		BeginWorldColorChange(); // クリスタル破壊イベントから世界色変化を開始する。
		if (requestBossAppear_ && changeSkyOnAllCrystalsBroken_)
		{
			BeginSkyColorChange(); // 全クリスタル破壊時に空色変化を開始する処理。
		}
		crystal.ClearJustBrokenFlag();
	}
}

void CrystalManager::BeginWorldColorChange()
{
	if (worldColorChanging_ || worldColorChangeComplete_)
	{
		return;
	}

	// 破壊時に急変させず、LightManagerの環境色を補間する世界演出へ入る。
	worldColorChanging_ = true;
	worldColorChangeComplete_ = false;
	worldColorChangeTimer_ = 0.0f;
	baseLightingSettings_ = Ken4lowEngine::LightManager::GetInstance()->GetLightingSettings();
}

void CrystalManager::UpdateWorldColorChange(float deltaTime)
{
	if (!worldColorChanging_)
	{
		return;
	}

	worldColorChangeTimer_ += deltaTime;
	const float t = std::clamp(worldColorChangeTimer_ / std::max(0.1f, worldColorChangeTime_), 0.0f, 1.0f);
	auto& lighting = Ken4lowEngine::LightManager::GetInstance()->GetMutableLightingSettingsForEditor();

	const float dark = std::clamp(worldDarkness_, 0.0f, 1.0f) * t;
	const float red = std::clamp(worldRedTint_, 0.0f, 1.0f) * t;
	lighting.ambientColor = {
		baseLightingSettings_.ambientColor.x * (1.0f - dark) + red,
		baseLightingSettings_.ambientColor.y * (1.0f - dark * 0.9f),
		baseLightingSettings_.ambientColor.z * (1.0f - dark),
		baseLightingSettings_.ambientColor.w
	};
	lighting.fogColor = {
		baseLightingSettings_.fogColor.x * (1.0f - dark) + red * 0.45f,
		baseLightingSettings_.fogColor.y * (1.0f - dark),
		baseLightingSettings_.fogColor.z * (1.0f - dark),
		baseLightingSettings_.fogColor.w
	};
	lighting.exposure = std::max(0.15f, baseLightingSettings_.exposure * (1.0f - dark * 0.55f));
	lighting.diffuseStrength = std::max(0.2f, baseLightingSettings_.diffuseStrength * (1.0f - dark * 0.35f));

	if (t >= 1.0f)
	{
		worldColorChanging_ = false;
		worldColorChangeComplete_ = true;
	}
}

void CrystalManager::BeginSkyColorChange()
{
	if (!skyColorChangeEnabled_ || skyColorChanging_ || skyColorChangeComplete_)
	{
		return;
	}

	skyColorChanging_ = true;
	skyColorChangeComplete_ = false;
	skyColorChangeTimer_ = 0.0f;
}

void CrystalManager::UpdateSkyColorChange(float deltaTime)
{
	if (!skyColorChanging_)
	{
		return;
	}

	skyColorChangeTimer_ += deltaTime;
	const float t = std::clamp(skyColorChangeTimer_ / std::max(0.1f, skyColorChangeTime_), 0.0f, 1.0f);
	// 空色をLerpで補間する処理。暗さ/赤み/紫みは最終色側に混ぜて不穏さを調整する。
	K4E::Vector4 target = brokenSkyColor_;
	target.x = std::clamp(target.x + skyRedTint_, 0.0f, 1.0f);
	target.y = std::clamp(target.y * (1.0f - skyDarkness_), 0.0f, 1.0f);
	target.z = std::clamp(target.z + skyPurpleTint_, 0.0f, 1.0f);
	const K4E::Vector4 color = LerpColor(normalSkyColor_, target, t);
	ApplySkyColor(color);

	if (t >= 1.0f)
	{
		skyColorChanging_ = false;
		skyColorChangeComplete_ = true;
	}
}

void CrystalManager::ApplySkyColor(const K4E::Vector4& color)
{
	if (!skyBox_)
	{
		return;
	}

	skyBox_->SetColor(color);
	const K4E::Vector4 top{ color.x, color.y, color.z, color.w };
	const K4E::Vector4 horizon{
		std::min(1.0f, color.x + 0.08f),
		std::min(1.0f, color.y + 0.04f),
		std::min(1.0f, color.z + 0.08f),
		color.w
	};
	const K4E::Vector4 bottom{
		std::max(0.0f, color.x * 0.65f),
		std::max(0.0f, color.y * 0.55f),
		std::max(0.0f, color.z * 0.70f),
		color.w
	};
	skyBox_->SetGradientColors(top, bottom, horizon);
}

K4E::Vector4 CrystalManager::LerpColor(const K4E::Vector4& a, const K4E::Vector4& b, float t) const
{
	t = std::clamp(t, 0.0f, 1.0f);
	return {
		a.x + (b.x - a.x) * t,
		a.y + (b.y - a.y) * t,
		a.z + (b.z - a.z) * t,
		a.w + (b.w - a.w) * t
	};
}

void CrystalManager::RestoreWorldColor()
{
	if (!worldColorChanging_ && !worldColorChangeComplete_ && !skyColorChanging_ && !skyColorChangeComplete_)
	{
		return;
	}

	if (worldColorChanging_ || worldColorChangeComplete_)
	{
		Ken4lowEngine::LightManager::GetInstance()->GetMutableLightingSettingsForEditor() = baseLightingSettings_;
	}
	worldColorChanging_ = false;
	worldColorChangeComplete_ = false;
	worldColorChangeTimer_ = 0.0f;
	skyColorChanging_ = false;
	skyColorChangeComplete_ = false;
	skyColorChangeTimer_ = 0.0f;
	ApplySkyColor(normalSkyColor_);
}

void CrystalManager::DrawImGui()
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("クリスタル デバッグ"))
	{
		return;
	}

	ImGui::Checkbox("クリスタル敵スポーン有効", &enableCrystalEnemySpawn_);
	float crystalYOffset = EnemySpawnCrystal::GetSpawnYOffset();
	if (ImGui::DragFloat("クリスタルY補正", &crystalYOffset, 0.01f, 0.0f, 0.5f, "%.2f"))
	{
		EnemySpawnCrystal::SetSpawnYOffset(crystalYOffset);
	}
	float enemySpawnYOffset = EnemyBase::GetSpawnYOffset();
	if (ImGui::DragFloat("敵スポーンY補正", &enemySpawnYOffset, 0.01f, 0.0f, 0.5f, "%.2f"))
	{
		EnemyBase::SetSpawnYOffset(enemySpawnYOffset);
	}
	bool deathExplosionEnabled = EnemyBase::IsDeathExplosionEnabled();
	if (ImGui::Checkbox("死亡部位 爆散有効", &deathExplosionEnabled))
	{
		EnemyBase::SetDeathExplosionEnabled(deathExplosionEnabled);
	}
	float deathExplodePower = EnemyBase::GetDeathExplodePower();
	if (ImGui::DragFloat("死亡部位 爆散力", &deathExplodePower, 0.1f, 0.0f, 8.0f, "%.2f"))
	{
		EnemyBase::SetDeathExplodePower(deathExplodePower);
	}
	float deathUpwardPower = EnemyBase::GetDeathUpwardPower();
	if (ImGui::DragFloat("死亡部位 上方向力", &deathUpwardPower, 0.1f, 0.0f, 5.0f, "%.2f"))
	{
		EnemyBase::SetDeathUpwardPower(deathUpwardPower);
	}
	float deathMaxSpeed = EnemyBase::GetDeathMaxSpeed();
	if (ImGui::DragFloat("死亡部位 最大速度", &deathMaxSpeed, 0.1f, 0.5f, 20.0f, "%.2f"))
	{
		EnemyBase::SetDeathMaxSpeed(deathMaxSpeed);
	}
	float deathMaxAngularSpeed = EnemyBase::GetDeathMaxAngularSpeed();
	if (ImGui::DragFloat("死亡部位 最大回転速度", &deathMaxAngularSpeed, 0.1f, 0.5f, 20.0f, "%.2f"))
	{
		EnemyBase::SetDeathMaxAngularSpeed(deathMaxAngularSpeed);
	}
	float deathPieceLifetime = EnemyBase::GetDeathPieceLifetime();
	if (ImGui::DragFloat("死亡部位 寿命", &deathPieceLifetime, 0.1f, 0.2f, 5.0f, "%.2f 秒"))
	{
		EnemyBase::SetDeathPieceLifetime(deathPieceLifetime);
	}
	ImGui::Text("現在のクリスタル由来敵数: %d", GetAliveCrystalSpawnEnemyCount());
	ImGui::DragInt("1回の最大スポーン数", &maxSpawnPerInterval_, 1.0f, 1, 9);
	maxSpawnPerInterval_ = std::max(1, maxSpawnPerInterval_);
	ImGui::SeparatorText("Difficulty Director");
	ImGui::Checkbox("Director有効", &difficultyDirectorEnabled_);
	ImGui::Text("Pressure Level: %s", GetPressureLevelName());
	ImGui::Text("pressureScore: %.2f", difficultyRuntime_.pressureScore);
	ImGui::Text("Player HP Rate: %.2f", difficultyRuntime_.playerHpRate);
	ImGui::Text("無被弾時間: %.2f 秒", difficultyRuntime_.noDamageTimer);
	ImGui::Text("最近の撃破数: %d", difficultyRuntime_.recentKillCount);
	ImGui::Text("現在敵数 / 全体上限: %d / %d", debugAliveNormalEnemyCount_, difficultyRuntime_.maxAliveEnemiesTotal);
	ImGui::Text("クリスタルごとの同時生存上限: %d", difficultyRuntime_.maxAliveEnemiesPerCrystal);
	ImGui::Text("スポーン間隔倍率: %.2f", difficultyRuntime_.spawnIntervalMultiplier);
	ImGui::Text("敵移動速度倍率: %.2f", difficultyRuntime_.enemyMoveSpeedMultiplier);
	ImGui::Text("攻撃CT倍率: %.2f", difficultyRuntime_.attackCooldownMultiplier);
	ImGui::Text("攻撃ダメージ倍率: %.2f", difficultyRuntime_.damageMultiplier);
	ImGui::DragFloat("低HP判定", &difficultySettings_.lowHpRate, 0.01f, 0.05f, 0.95f, "%.2f");
	ImGui::DragFloat("余裕HP判定", &difficultySettings_.comfortableHpRate, 0.01f, 0.05f, 1.0f, "%.2f");
	ImGui::DragFloat("余裕無被弾時間", &difficultySettings_.noDamageComfortTime, 0.1f, 1.0f, 60.0f, "%.1f 秒");
	ImGui::DragFloat("Panic無被弾時間", &difficultySettings_.noDamagePanicTime, 0.1f, 1.0f, 90.0f, "%.1f 秒");
	ImGui::DragInt("Easy 全体敵数上限", &difficultySettings_.easyMaxAliveEnemiesTotal, 1.0f, 1, 50);
	ImGui::DragInt("Normal 全体敵数上限", &difficultySettings_.normalMaxAliveEnemiesTotal, 1.0f, 1, 50);
	ImGui::DragInt("High 全体敵数上限", &difficultySettings_.highMaxAliveEnemiesTotal, 1.0f, 1, 50);
	ImGui::DragInt("Panic 全体敵数上限", &difficultySettings_.panicMaxAliveEnemiesTotal, 1.0f, 1, 50);
	ImGui::DragInt("Easy クリスタル同時上限", &difficultySettings_.easyMaxAlivePerCrystal, 1.0f, 0, 20);
	ImGui::DragInt("Normal クリスタル同時上限", &difficultySettings_.normalMaxAlivePerCrystal, 1.0f, 0, 20);
	ImGui::DragInt("High クリスタル同時上限", &difficultySettings_.highMaxAlivePerCrystal, 1.0f, 0, 20);
	ImGui::DragInt("Panic クリスタル同時上限", &difficultySettings_.panicMaxAlivePerCrystal, 1.0f, 0, 20);
	ImGui::DragFloat("Easy 間隔倍率", &difficultySettings_.easySpawnIntervalMultiplier, 0.01f, 0.1f, 5.0f, "%.2f");
	ImGui::DragFloat("High 間隔倍率", &difficultySettings_.highSpawnIntervalMultiplier, 0.01f, 0.1f, 5.0f, "%.2f");
	ImGui::DragFloat("Panic 間隔倍率", &difficultySettings_.panicSpawnIntervalMultiplier, 0.01f, 0.1f, 5.0f, "%.2f");
	ImGui::Text("クリスタル数: %d", GetCrystalCount());
	ImGui::Text("生存クリスタル数: %d", GetAliveCrystalCount());
	ImGui::Text("全クリスタル破壊済み: %s", AreAllCrystalsDestroyed() ? "はい" : "いいえ");
	ImGui::Text("クリスタル敵スポーン有効(実効): %s", (enableCrystalEnemySpawn_ && !AreAllCrystalsDestroyed()) ? "はい" : "いいえ");
	ImGui::Text("生存雑魚敵数: %d", debugAliveNormalEnemyCount_);
	ImGui::Text("ボス出現条件成立: %s", debugBossSpawnConditionMet_ ? "はい" : "いいえ");
	ImGui::Text("ボス出現済み: %s", debugBossSpawned_ ? "はい" : "いいえ");
	ImGui::Text("ボス出現位置: (%.2f, %.2f, %.2f)", debugBossSpawnPosition_.x, debugBossSpawnPosition_.y, debugBossSpawnPosition_.z);
	ImGui::Text("クリスタル破壊イベント発生済み: %s", hasCrystalBroken_ ? "はい" : "いいえ");
	ImGui::Text("最終局面準備完了: %s", isFinalPhaseReady_ ? "はい" : "いいえ");
	ImGui::Text("ボス登場要求フラグ: %s", requestBossAppear_ ? "はい" : "いいえ");
	ImGui::Text("世界色変化: %.2f / %.2f 秒", worldColorChangeTimer_, worldColorChangeTime_);
	ImGui::Text("世界色変化完了: %s", worldColorChangeComplete_ ? "はい" : "いいえ");
	crystalHpBarController_.DrawImGui();
	ImGui::SeparatorText("Sky Color Effect");
	ImGui::Text("空色変化ON: %s", skyColorChangeEnabled_ ? "ON" : "OFF");
	ImGui::Text("全クリスタル破壊時に変化: %s", changeSkyOnAllCrystalsBroken_ ? "ON" : "OFF");
	ImGui::Text("空色変化: %.2f / %.2f 秒 complete:%s", skyColorChangeTimer_, skyColorChangeTime_, skyColorChangeComplete_ ? "はい" : "いいえ");
	ImGui::Text("通常空色: %.2f %.2f %.2f %.2f", normalSkyColor_.x, normalSkyColor_.y, normalSkyColor_.z, normalSkyColor_.w);
	ImGui::Text("破壊後空色: %.2f %.2f %.2f %.2f", brokenSkyColor_.x, brokenSkyColor_.y, brokenSkyColor_.z, brokenSkyColor_.w);
	if (ImGui::Button("Sky Color Debug Start"))
	{
		BeginSkyColorChange();
	}
	ImGui::SameLine();
	if (ImGui::Button("Sky Color Reset"))
	{
		skyColorChanging_ = false;
		skyColorChangeComplete_ = false;
		skyColorChangeTimer_ = 0.0f;
		ApplySkyColor(normalSkyColor_);
	}
	ImGui::Text("更新用deltaTime上限: %.4f 秒", kMaxUpdateDeltaTime);
	ImGui::Text("敵Ground Snap有効: %s", EnemyBase::IsGroundSnapEnabled() ? "はい" : "いいえ");
	ImGui::Text("クリスタルGround Snap有効: %s", EnemySpawnCrystal::IsGroundSnapEnabled() ? "はい" : "いいえ");
	ImGui::Text("最大押し出し量: %.2f", EnemyBase::GetMaxPushOutPerFrame());
	ImGui::Text("敵の場外制限有効: %s", EnemyBase::IsWorldBoundsEnabled() ? "はい" : "いいえ");

	if (crystals_.empty())
	{
		ImGui::Text("クリスタル設定がありません。");
		return;
	}

	int selectedIndex = static_cast<int>(selectedCrystalIndex_);
	if (ImGui::SliderInt("選択中クリスタル", &selectedIndex, 0, static_cast<int>(crystals_.size()) - 1))
	{
		selectedCrystalIndex_ = static_cast<size_t>(selectedIndex);
	}

	EnemySpawnCrystal* crystal = GetSelectedCrystal();
	if (!crystal)
	{
		return;
	}

	bool enableInfiniteSpawn = crystal->IsInfiniteSpawnEnabled();
	if (ImGui::Checkbox("選択中クリスタルの敵スポーン有効", &enableInfiniteSpawn))
	{
		crystal->SetInfiniteSpawnEnabled(enableInfiniteSpawn);
	}

	// クリスタルのDebug生成も現行の近接・中距離だけを選べるようにする。
	constexpr const char* enemyTypeLabels[] = { "近接雑魚敵", "中距離雑魚敵" };
	int enemyTypeIndex = static_cast<int>(crystal->GetSpawnEnemyType());
	if (ImGui::Combo("出現敵タイプ", &enemyTypeIndex, enemyTypeLabels, IM_ARRAYSIZE(enemyTypeLabels)))
	{
		crystal->SetSpawnEnemyType(static_cast<EnemyType>(enemyTypeIndex));
	}

	int maxAliveEnemies = crystal->GetMaxAliveEnemies();
	if (ImGui::DragInt("選択中クリスタルの同時出現上限", &maxAliveEnemies, 1.0f, 0, 100))
	{
		crystal->SetMaxAliveEnemies(maxAliveEnemies);
	}

	const Ken4lowEngine::Vector3& crystalPosition = crystal->GetPosition();
	const Ken4lowEngine::Vector3& crystalScale = crystal->GetScale();
	const Ken4lowEngine::Vector3 cameraPosition = Ken4lowEngine::CameraManager::GetInstance()->GetActiveCameraPosition();
	ImGui::Text("選択中クリスタル座標: (%.2f, %.2f, %.2f)", crystalPosition.x, crystalPosition.y, crystalPosition.z);
	ImGui::Text("クリスタルScale: (%.2f, %.2f, %.2f)", crystalScale.x, crystalScale.y, crystalScale.z);
	ImGui::Text("選択中クリスタルHP: %d", crystal->GetHp());
	ImGui::Text("選択中クリスタル最大HP: %d", crystal->GetMaxHp());
	ImGui::Text("選択中クリスタルHP割合: %.2f", crystal->GetHpRate());
	ImGui::Text("生存状態: %s", crystal->IsAlive() ? "生存" : "破壊済み");
	const char* stateLabel = "Normal";
	switch (crystal->GetState())
	{
	case EnemySpawnCrystal::State::Damaged: stateLabel = "Damaged"; break;
	case EnemySpawnCrystal::State::Critical: stateLabel = "Critical"; break;
	case EnemySpawnCrystal::State::Breaking: stateLabel = "Breaking"; break;
	case EnemySpawnCrystal::State::Broken: stateLabel = "Broken"; break;
	case EnemySpawnCrystal::State::Normal:
	default: break;
	}
	ImGui::Text("クリスタル状態: %s", stateLabel);
	ImGui::Text("クリスタル破壊済み: %s", crystal->IsDestroyed() ? "はい" : "いいえ");
	ImGui::Text("クリスタルCollider有効: %s", crystal->IsColliderEnabled() ? "はい" : "いいえ");
	ImGui::Text("クリスタル被弾回数: %d", crystal->GetHitCount());
	ImGui::Text("カメラ座標: (%.2f, %.2f, %.2f)", cameraPosition.x, cameraPosition.y, cameraPosition.z);
	ImGui::Text("湧き間隔: %.2f 秒", crystal->GetSpawnInterval());
	ImGui::Text("初回湧き遅延: %.2f 秒", crystal->GetInitialDelay());
	ImGui::Text("湧きタイマー: %.2f 秒", crystal->GetSpawnTimer());
	ImGui::Text("湧き方: %s", crystal->GetSpawnPattern().c_str());
	ImGui::Text("最大湧き数: %d", crystal->GetMaxSpawnCount());
	ImGui::Text("湧き半径: %.2f", crystal->GetSpawnRadius());
	ImGui::Text("選択中クリスタル由来の生存敵数: %d", crystal->GetAliveSpawnedEnemyCount());
	ImGui::Text("選択中クリスタルの合計スポーン数: %d", crystal->GetTotalSpawnedCount());
	ImGui::SeparatorText("Crystal Alive Counts");
	for (size_t i = 0; i < crystals_.size(); ++i)
	{
		const EnemySpawnCrystal& entry = crystals_[i];
		ImGui::Text("[%zu] %s alive:%d maxAlive:%d interval:%.2f total:%d",
			i,
			entry.GetCrystalName().c_str(),
			entry.GetAliveSpawnedEnemyCount(),
			entry.GetMaxAliveEnemies(),
			entry.GetSpawnInterval(),
			entry.GetTotalSpawnedCount());
	}
	if (ImGui::Button("選択中クリスタルに10ダメージ"))
	{
		crystal->ApplyDamage(10);
	}
	if (ImGui::Button("全クリスタルに10ダメージ"))
	{
		for (EnemySpawnCrystal& target : crystals_)
		{
			target.ApplyDamage(10);
		}
	}
#endif
}
