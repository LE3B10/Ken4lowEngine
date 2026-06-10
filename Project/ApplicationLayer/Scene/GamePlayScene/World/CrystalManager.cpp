#define NOMINMAX
#include "CrystalManager.h"

#include "CharacterWorld.h"
#include "CameraManager.h"
#include "CollisionManager.h"
#include "EnemyBase.h"
#include "EnemyHPBarProjector.h"
#include "ParameterManager.h"
#include "SkyBox.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
	constexpr float kMinimumSpawnInterval = 0.05f;
	constexpr const char* kCrystalSpawnerRootGroup = "CrystalSpawner";
	constexpr const char* kCrystalReactionGroup = "CrystalEffect/CrystalReaction";
	constexpr const char* kCrystalHpBarGroup = "CrystalHpBar";
	constexpr const char* kSkyColorGroup = "SkyColorEffect";

	const std::vector<std::string>& CrystalEnemyTypeOptions()
	{
		static const std::vector<std::string> options = {
			"Legacy",
			"Melee",
			"MidRange",
			"GuardianBoss"
		};
		return options;
	}

	const std::vector<std::string>& CrystalSpawnPatternOptions()
	{
		static const std::vector<std::string> options = {
			"Single",
			"Interval",
			"Burst"
		};
		return options;
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
	RegisterReactionParameters();
	ApplyReactionParameters();
	RegisterHpBarParameters();
	ApplyHpBarParameters();
	RegisterSkyColorParameters();
	ApplySkyColorParameters();
	ApplySkyColor(normalSkyColor_);

	collisionManager_ = collisionManager;
	floorAABBs_ = floorAABBs;
	obstacleAABBs_ = obstacleAABBs;
	spawnPoints_ = spawnPoints;
	parameterGroupNames_.clear();
	parameterGroupNames_.reserve(spawnPoints_.size());

	for (CrystalSpawnPoint& spawnPoint : spawnPoints_)
	{
		RegisterCrystalParameters(spawnPoint);
		ApplyParameterToSpawnPoint(spawnPoint);
	}

	crystals_.reserve(spawnPoints_.size());
	for (const CrystalSpawnPoint& spawnPoint : spawnPoints_)
	{
		EnemySpawnCrystal crystal;
		crystal.Initialize(spawnPoint, floorAABBs_, obstacleAABBs_);
		crystals_.push_back(std::move(crystal));
	}
	crystalHpBars_.clear();
	crystalHpBars_.reserve(crystals_.size());
	for (size_t i = 0; i < crystals_.size(); ++i)
	{
		auto bar = std::make_unique<EnemyHPBar>();
		bar->Initialize();
		crystalHpBars_.push_back(std::move(bar));
	}
	crystalHpBarDebugInfos_.resize(crystals_.size());
	crystalHpBarAimTimers_.assign(crystals_.size(), 0.0f);

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

	UnregisterCrystalParameters();
	UnregisterReactionParameters();
	UnregisterHpBarParameters();
	UnregisterSkyColorParameters();
	RestoreWorldColor();
	crystals_.clear();
	crystalHpBars_.clear();
	crystalHpBarDebugInfos_.clear();
	crystalHpBarAimTimers_.clear();
	spawnPoints_.clear();
	parameterGroupNames_.clear();
	collisionManager_ = nullptr;
	floorAABBs_ = nullptr;
	obstacleAABBs_ = nullptr;
}

void CrystalManager::Update(CharacterWorld& characters, float deltaTime)
{
	SyncCrystalsFromParameterManager();
	ApplyReactionParameters();
	ApplyHpBarParameters();
	ApplySkyColorParameters();

	for (EnemySpawnCrystal& crystal : crystals_)
	{
		crystal.Update(characters, deltaTime, reactionSettings_);
	}
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
		EnemySpawnCrystal* crystal = FindNextSpawnableCrystal();
		if (!crystal)
		{
			break;
		}

		if (crystal->GetSpawnPattern() == "Burst")
		{
			while (crystal->CanSpawnEnemy())
			{
				crystal->SpawnEnemy(characters);
			}
		}
		else
		{
			crystal->SpawnEnemy(characters);
		}
		crystal->ConsumeSpawnTimer();
	}
}

void CrystalManager::UpdatePresentationOnly(CharacterWorld& characters, float deltaTime)
{
	SyncCrystalsFromParameterManager();
	ApplyReactionParameters();

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
	crystalHpBarDrawCalled_ = false;
	crystalHpBarVisibleCount_ = 0;
	if (crystalHpBarDebugInfos_.size() != crystals_.size())
	{
		crystalHpBarDebugInfos_.resize(crystals_.size());
	}
	if (crystalHpBarAimTimers_.size() != crystals_.size())
	{
		crystalHpBarAimTimers_.assign(crystals_.size(), 0.0f);
	}
	if (crystalHpBars_.size() != crystals_.size())
	{
		crystalHpBars_.clear();
		crystalHpBars_.reserve(crystals_.size());
		for (size_t i = 0; i < crystals_.size(); ++i)
		{
			auto bar = std::make_unique<EnemyHPBar>();
			bar->Initialize();
			crystalHpBars_.push_back(std::move(bar));
		}
		crystalHpBarAimTimers_.assign(crystals_.size(), 0.0f);
	}

	if (!crystalHpBarVisible_)
	{
		for (auto& bar : crystalHpBars_)
		{
			if (bar) { bar->SetVisible(false); }
		}
		return;
	}

	for (size_t i = 0; i < crystals_.size(); ++i)
	{
		const EnemySpawnCrystal& crystal = crystals_[i];
		CrystalHpBarDebugInfo debug{};
		debug.hp = crystal.GetHp();
		debug.maxHp = crystal.GetMaxHp();
		debug.hpRate = std::clamp(crystal.GetHpRate(), 0.0f, 1.0f);
		debug.active = crystal.IsAlive();
		debug.broken = crystal.IsDestroyed();

		bool visible = true;
		if (&crystal == aimedCrystal)
		{
			crystalHpBarAimTimers_[i] = std::max(0.0f, visibleHoldTime);
		}
		else if (crystalHpBarAimTimers_[i] > 0.0f)
		{
			crystalHpBarAimTimers_[i] = std::max(0.0f, crystalHpBarAimTimers_[i] - deltaTime);
		}
		if (crystal.IsDestroyed())
		{
			visible = false;
			debug.hiddenReason = "Broken";
		}
		else if (showOnlyWhenAimed && &crystal != aimedCrystal && crystalHpBarAimTimers_[i] <= 0.0f)
		{
			// 照準対象だけHPバーを表示する処理。短い保持時間で外れ際のチラつきを抑える。
			visible = false;
			debug.hiddenReason = "Not aimed";
		}
		else if (!crystalHpBarAlwaysVisible_ && crystal.GetHp() >= crystal.GetMaxHp())
		{
			visible = false;
			debug.hiddenReason = "Full HP";
		}

		const Ken4lowEngine::Vector3& pos = crystal.GetPosition();
		const Ken4lowEngine::Vector3& scale = crystal.GetScale();
		// クリスタル頭上HPバーの表示位置をParameterManager調整値込みで計算する。
		const Ken4lowEngine::Vector3 hpBarWorldPos{ pos.x, pos.y + std::abs(scale.y) * 0.65f + crystalHpBarOffsetY_, pos.z };
		debug.worldPosition = hpBarWorldPos;
		const HpBarProjectResult projected = ProjectWorldToScreen(hpBarWorldPos, viewMatrix, projMatrix, screenWidth, screenHeight);
		debug.screenPosition = projected.screenPos;
		debug.inFront = projected.inFront;
		debug.inScreen = projected.inScreen;
		if (visible && (!projected.inFront || !projected.inScreen))
		{
			visible = false;
			debug.hiddenReason = projected.inFront ? "Out of screen" : "Behind camera";
		}

		// クリスタルHP率を計算し、最大HPが不正でも0除算しないようGetter側の安全値を使う。
		const float hpRate = debug.hpRate;
		debug.visible = visible;
		if (visible)
		{
			++crystalHpBarVisibleCount_;
		}
		if (i < crystalHpBars_.size() && crystalHpBars_[i])
		{
			crystalHpBars_[i]->Update(projected.screenPos, hpRate, visible, deltaTime, crystalHpBarWidth_, crystalHpBarHeight_);
		}
		crystalHpBarDebugInfos_[i] = debug;
	}
}

void CrystalManager::DrawHpBars()
{
	crystalHpBarDrawCalled_ = true;
	for (auto& bar : crystalHpBars_)
	{
		if (bar)
		{
			bar->Draw();
		}
	}
}

int CrystalManager::GetAliveCrystalCount() const
{
	return static_cast<int>(std::count_if(crystals_.begin(), crystals_.end(),
		[](const EnemySpawnCrystal& crystal) { return crystal.IsAlive(); }));
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

void CrystalManager::RegisterCrystalParameters(CrystalSpawnPoint& spawnPoint)
{
	const std::string groupName = BuildCrystalGroupName(spawnPoint);
	parameterGroupNames_.push_back(groupName);

	auto* parameters = Ken4lowEngine::ParameterManager::GetInstance();
	parameters->CreateGroup(groupName);

	// クリスタル自体をスポナーとしてParameterManagerへ登録し、既存のJson保存/読み込みに乗せる。
	parameters->AddItem(groupName, "isActive", spawnPoint.isActive);
	parameters->AddItem(groupName, "position", spawnPoint.position, Ken4lowEngine::Vector3{ -200.0f, -50.0f, -200.0f }, Ken4lowEngine::Vector3{ 200.0f, 80.0f, 200.0f });
	parameters->AddItem(groupName, "rotation", spawnPoint.rotation, Ken4lowEngine::Vector3{ -3.141592f, -3.141592f, -3.141592f }, Ken4lowEngine::Vector3{ 3.141592f, 3.141592f, 3.141592f });
	parameters->AddItem(groupName, "scale", spawnPoint.scale, Ken4lowEngine::Vector3{ 0.1f, 0.1f, 0.1f }, Ken4lowEngine::Vector3{ 10.0f, 10.0f, 10.0f });
	parameters->AddItem(groupName, "hp", spawnPoint.hp, 1, 10000);
	parameters->AddItem(groupName, "maxHp", spawnPoint.maxHp, 1, 10000);
	parameters->AddStringItem(groupName, "enemyType", ToEnemyTypeName(spawnPoint.spawnEnemyType), CrystalEnemyTypeOptions());
	parameters->AddItem(groupName, "spawnInterval", spawnPoint.spawnInterval, 0.05f, 60.0f);
	parameters->AddItem(groupName, "initialDelay", spawnPoint.initialDelay, 0.0f, 60.0f);
	parameters->AddItem(groupName, "maxSpawnCount", spawnPoint.maxSpawnCount, 0, 1000);
	parameters->AddItem(groupName, "maxAliveCount", spawnPoint.maxAliveEnemies, 0, 100);
	parameters->AddItem(groupName, "spawnRadius", spawnPoint.spawnRadius, 0.0f, 50.0f);
	parameters->AddStringItem(groupName, "spawnPattern", spawnPoint.spawnPattern, CrystalSpawnPatternOptions());

	parameters->SetDisplayName(groupName, "isActive", "有効");
	parameters->SetDisplayName(groupName, "position", "座標");
	parameters->SetDisplayName(groupName, "rotation", "回転");
	parameters->SetDisplayName(groupName, "scale", "スケール");
	parameters->SetDisplayName(groupName, "hp", "初期HP");
	parameters->SetDisplayName(groupName, "maxHp", "最大HP");
	parameters->SetDisplayName(groupName, "enemyType", "敵タイプ");
	parameters->SetDisplayName(groupName, "spawnInterval", "湧き間隔");
	parameters->SetDisplayName(groupName, "initialDelay", "初回湧き遅延");
	parameters->SetDisplayName(groupName, "maxSpawnCount", "最大湧き数");
	parameters->SetDisplayName(groupName, "maxAliveCount", "同時出現数");
	parameters->SetDisplayName(groupName, "spawnRadius", "湧き半径");
	parameters->SetDisplayName(groupName, "spawnPattern", "湧き方");

	parameters->RegisterParameterApplier(groupName, this, [this]() { SyncCrystalsFromParameterManager(); });
	parameters->LoadFile(groupName);
}

void CrystalManager::UnregisterCrystalParameters()
{
	auto* parameters = Ken4lowEngine::ParameterManager::GetInstance();
	for (const std::string& groupName : parameterGroupNames_)
	{
		parameters->UnregisterParameterApplier(groupName, this);
	}
}

void CrystalManager::ApplyParameterToSpawnPoint(CrystalSpawnPoint& spawnPoint)
{
	const std::string groupName = BuildCrystalGroupName(spawnPoint);
	auto* parameters = Ken4lowEngine::ParameterManager::GetInstance();

	spawnPoint.isActive = parameters->GetValue<bool>(groupName, "isActive");
	spawnPoint.position = parameters->GetValue<Ken4lowEngine::Vector3>(groupName, "position");
	spawnPoint.rotation = parameters->GetValue<Ken4lowEngine::Vector3>(groupName, "rotation");
	spawnPoint.scale = parameters->GetValue<Ken4lowEngine::Vector3>(groupName, "scale");
	spawnPoint.hp = std::max(1, parameters->GetValue<int32_t>(groupName, "hp"));
	spawnPoint.maxHp = std::max(1, parameters->GetValue<int32_t>(groupName, "maxHp"));
	spawnPoint.spawnEnemyType = ParseCrystalEnemyType(parameters->GetValue<std::string>(groupName, "enemyType"));
	spawnPoint.spawnInterval = std::max(kMinimumSpawnInterval, parameters->GetValue<float>(groupName, "spawnInterval"));
	spawnPoint.initialDelay = std::max(0.0f, parameters->GetValue<float>(groupName, "initialDelay"));
	spawnPoint.maxSpawnCount = std::max(0, parameters->GetValue<int32_t>(groupName, "maxSpawnCount"));
	spawnPoint.maxAliveEnemies = std::max(0, parameters->GetValue<int32_t>(groupName, "maxAliveCount"));
	spawnPoint.spawnRadius = std::max(0.0f, parameters->GetValue<float>(groupName, "spawnRadius"));
	spawnPoint.spawnPattern = parameters->GetValue<std::string>(groupName, "spawnPattern");
	spawnPoint.enableInfiniteSpawn = spawnPoint.isActive;
}

void CrystalManager::SyncCrystalsFromParameterManager()
{
	for (size_t i = 0; i < spawnPoints_.size() && i < crystals_.size(); ++i)
	{
		ApplyParameterToSpawnPoint(spawnPoints_[i]);
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

void CrystalManager::RegisterReactionParameters()
{
	auto* parameters = Ken4lowEngine::ParameterManager::GetInstance();
	parameters->CreateGroup(kCrystalReactionGroup);

	parameters->AddItem(kCrystalReactionGroup, "hitFlashTime", reactionSettings_.hitFlashTime, 0.0f, 2.0f);
	parameters->AddItem(kCrystalReactionGroup, "hitShakePower", reactionSettings_.hitShakePower, 0.0f, 2.0f);
	parameters->AddItem(kCrystalReactionGroup, "hitShakeTime", reactionSettings_.hitShakeTime, 0.0f, 2.0f);
	parameters->AddItem(kCrystalReactionGroup, "breakingDuration", reactionSettings_.breakingDuration, 0.05f, 5.0f);
	parameters->AddItem(kCrystalReactionGroup, "breakEffectScale", reactionSettings_.breakEffectScale, 1.0f, 4.0f);
	parameters->AddItem(kCrystalReactionGroup, "worldColorChangeTime", worldColorChangeTime_, 0.1f, 10.0f);
	parameters->AddItem(kCrystalReactionGroup, "worldDarkness", worldDarkness_, 0.0f, 1.0f);
	parameters->AddItem(kCrystalReactionGroup, "worldRedTint", worldRedTint_, 0.0f, 1.0f);
	parameters->AddItem(kCrystalReactionGroup, "criticalHpRate", reactionSettings_.criticalHpRate, 0.0f, 1.0f);
	parameters->AddItem(kCrystalReactionGroup, "damagedHpRate", reactionSettings_.damagedHpRate, 0.0f, 1.0f);

	parameters->SetDisplayName(kCrystalReactionGroup, "hitFlashTime", "ヒット点滅時間");
	parameters->SetDisplayName(kCrystalReactionGroup, "hitShakePower", "ヒット揺れ強度");
	parameters->SetDisplayName(kCrystalReactionGroup, "hitShakeTime", "ヒット揺れ時間");
	parameters->SetDisplayName(kCrystalReactionGroup, "breakingDuration", "破壊演出時間");
	parameters->SetDisplayName(kCrystalReactionGroup, "breakEffectScale", "破壊拡大率");
	parameters->SetDisplayName(kCrystalReactionGroup, "worldColorChangeTime", "世界色変化時間");
	parameters->SetDisplayName(kCrystalReactionGroup, "worldDarkness", "世界暗さ");
	parameters->SetDisplayName(kCrystalReactionGroup, "worldRedTint", "世界赤み");
	parameters->SetDisplayName(kCrystalReactionGroup, "criticalHpRate", "瀕死HP割合");
	parameters->SetDisplayName(kCrystalReactionGroup, "damagedHpRate", "損傷HP割合");

	parameters->RegisterParameterApplier(kCrystalReactionGroup, this, [this]() { ApplyReactionParameters(); });
	parameters->LoadFile(kCrystalReactionGroup);
}

void CrystalManager::UnregisterReactionParameters()
{
	Ken4lowEngine::ParameterManager::GetInstance()->UnregisterParameterApplier(kCrystalReactionGroup, this);
}

void CrystalManager::ApplyReactionParameters()
{
	auto* parameters = Ken4lowEngine::ParameterManager::GetInstance();
	reactionSettings_.hitFlashTime = parameters->GetValue<float>(kCrystalReactionGroup, "hitFlashTime");
	reactionSettings_.hitShakePower = parameters->GetValue<float>(kCrystalReactionGroup, "hitShakePower");
	reactionSettings_.hitShakeTime = parameters->GetValue<float>(kCrystalReactionGroup, "hitShakeTime");
	reactionSettings_.breakingDuration = parameters->GetValue<float>(kCrystalReactionGroup, "breakingDuration");
	reactionSettings_.breakEffectScale = parameters->GetValue<float>(kCrystalReactionGroup, "breakEffectScale");
	worldColorChangeTime_ = parameters->GetValue<float>(kCrystalReactionGroup, "worldColorChangeTime");
	worldDarkness_ = parameters->GetValue<float>(kCrystalReactionGroup, "worldDarkness");
	worldRedTint_ = parameters->GetValue<float>(kCrystalReactionGroup, "worldRedTint");
	reactionSettings_.criticalHpRate = parameters->GetValue<float>(kCrystalReactionGroup, "criticalHpRate");
	reactionSettings_.damagedHpRate = parameters->GetValue<float>(kCrystalReactionGroup, "damagedHpRate");
	reactionSettings_.criticalHpRate = std::clamp(reactionSettings_.criticalHpRate, 0.0f, 1.0f);
	reactionSettings_.damagedHpRate = std::clamp(reactionSettings_.damagedHpRate, reactionSettings_.criticalHpRate, 1.0f);
}

void CrystalManager::RegisterHpBarParameters()
{
	auto* parameters = Ken4lowEngine::ParameterManager::GetInstance();
	parameters->CreateGroup(kCrystalHpBarGroup);
	parameters->AddItem(kCrystalHpBarGroup, "crystalHpBarVisible", crystalHpBarVisible_);
	parameters->AddItem(kCrystalHpBarGroup, "crystalHpBarAlwaysVisible", crystalHpBarAlwaysVisible_);
	parameters->AddItem(kCrystalHpBarGroup, "crystalHpBarOffsetY", crystalHpBarOffsetY_, -2.0f, 8.0f);
	parameters->AddItem(kCrystalHpBarGroup, "crystalHpBarWidth", crystalHpBarWidth_, 20.0f, 240.0f);
	parameters->AddItem(kCrystalHpBarGroup, "crystalHpBarHeight", crystalHpBarHeight_, 2.0f, 40.0f);
	parameters->AddItem(kCrystalHpBarGroup, "crystalHpBarShowTime", crystalHpBarShowTime_, 0.0f, 10.0f);
	parameters->SetDisplayName(kCrystalHpBarGroup, "crystalHpBarVisible", "クリスタルHPバー表示");
	parameters->SetDisplayName(kCrystalHpBarGroup, "crystalHpBarAlwaysVisible", "常時表示");
	parameters->SetDisplayName(kCrystalHpBarGroup, "crystalHpBarOffsetY", "頭上オフセットY");
	parameters->SetDisplayName(kCrystalHpBarGroup, "crystalHpBarWidth", "バー幅");
	parameters->SetDisplayName(kCrystalHpBarGroup, "crystalHpBarHeight", "バー高さ");
	parameters->SetDisplayName(kCrystalHpBarGroup, "crystalHpBarShowTime", "被弾後表示時間");
	parameters->RegisterParameterApplier(kCrystalHpBarGroup, this, [this]() { ApplyHpBarParameters(); });
	parameters->LoadFile(kCrystalHpBarGroup);
}

void CrystalManager::UnregisterHpBarParameters()
{
	Ken4lowEngine::ParameterManager::GetInstance()->UnregisterParameterApplier(kCrystalHpBarGroup, this);
}

void CrystalManager::ApplyHpBarParameters()
{
	auto* parameters = Ken4lowEngine::ParameterManager::GetInstance();
	crystalHpBarVisible_ = parameters->GetValue<bool>(kCrystalHpBarGroup, "crystalHpBarVisible");
	crystalHpBarAlwaysVisible_ = parameters->GetValue<bool>(kCrystalHpBarGroup, "crystalHpBarAlwaysVisible");
	crystalHpBarOffsetY_ = parameters->GetValue<float>(kCrystalHpBarGroup, "crystalHpBarOffsetY");
	crystalHpBarWidth_ = std::max(1.0f, parameters->GetValue<float>(kCrystalHpBarGroup, "crystalHpBarWidth"));
	crystalHpBarHeight_ = std::max(1.0f, parameters->GetValue<float>(kCrystalHpBarGroup, "crystalHpBarHeight"));
	crystalHpBarShowTime_ = std::max(0.0f, parameters->GetValue<float>(kCrystalHpBarGroup, "crystalHpBarShowTime"));
}

void CrystalManager::RegisterSkyColorParameters()
{
	auto* parameters = Ken4lowEngine::ParameterManager::GetInstance();
	parameters->CreateGroup(kSkyColorGroup);
	parameters->AddItem(kSkyColorGroup, "skyColorChangeEnabled", skyColorChangeEnabled_);
	parameters->AddItem(kSkyColorGroup, "skyColorChangeTime", skyColorChangeTime_, 0.1f, 10.0f);
	parameters->AddItem(kSkyColorGroup, "normalSkyColor", normalSkyColor_);
	parameters->AddItem(kSkyColorGroup, "brokenSkyColor", brokenSkyColor_);
	parameters->AddItem(kSkyColorGroup, "skyDarkness", skyDarkness_, 0.0f, 1.0f);
	parameters->AddItem(kSkyColorGroup, "skyRedTint", skyRedTint_, 0.0f, 1.0f);
	parameters->AddItem(kSkyColorGroup, "skyPurpleTint", skyPurpleTint_, 0.0f, 1.0f);
	parameters->AddItem(kSkyColorGroup, "changeSkyOnAllCrystalsBroken", changeSkyOnAllCrystalsBroken_);
	parameters->SetDisplayName(kSkyColorGroup, "skyColorChangeEnabled", "空色変化ON");
	parameters->SetDisplayName(kSkyColorGroup, "skyColorChangeTime", "空色変化時間");
	parameters->SetDisplayName(kSkyColorGroup, "normalSkyColor", "通常空色");
	parameters->SetDisplayName(kSkyColorGroup, "brokenSkyColor", "破壊後空色");
	parameters->SetDisplayName(kSkyColorGroup, "skyDarkness", "空の暗さ");
	parameters->SetDisplayName(kSkyColorGroup, "skyRedTint", "赤み");
	parameters->SetDisplayName(kSkyColorGroup, "skyPurpleTint", "紫み");
	parameters->SetDisplayName(kSkyColorGroup, "changeSkyOnAllCrystalsBroken", "全破壊時に空色変化");
	parameters->RegisterParameterApplier(kSkyColorGroup, this, [this]() { ApplySkyColorParameters(); });
	parameters->LoadFile(kSkyColorGroup);
}

void CrystalManager::UnregisterSkyColorParameters()
{
	Ken4lowEngine::ParameterManager::GetInstance()->UnregisterParameterApplier(kSkyColorGroup, this);
}

void CrystalManager::ApplySkyColorParameters()
{
	auto* parameters = Ken4lowEngine::ParameterManager::GetInstance();
	// ParameterManagerから空色設定を反映する処理。
	skyColorChangeEnabled_ = parameters->GetValue<bool>(kSkyColorGroup, "skyColorChangeEnabled");
	skyColorChangeTime_ = std::max(0.1f, parameters->GetValue<float>(kSkyColorGroup, "skyColorChangeTime"));
	normalSkyColor_ = parameters->GetValue<K4E::Vector4>(kSkyColorGroup, "normalSkyColor");
	brokenSkyColor_ = parameters->GetValue<K4E::Vector4>(kSkyColorGroup, "brokenSkyColor");
	skyDarkness_ = std::clamp(parameters->GetValue<float>(kSkyColorGroup, "skyDarkness"), 0.0f, 1.0f);
	skyRedTint_ = std::clamp(parameters->GetValue<float>(kSkyColorGroup, "skyRedTint"), 0.0f, 1.0f);
	skyPurpleTint_ = std::clamp(parameters->GetValue<float>(kSkyColorGroup, "skyPurpleTint"), 0.0f, 1.0f);
	changeSkyOnAllCrystalsBroken_ = parameters->GetValue<bool>(kSkyColorGroup, "changeSkyOnAllCrystalsBroken");
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

std::string CrystalManager::BuildCrystalGroupName(const CrystalSpawnPoint& spawnPoint) const
{
	return std::string(kCrystalSpawnerRootGroup) + "/" + spawnPoint.crystalName;
}

const char* CrystalManager::ToEnemyTypeName(EnemyType enemyType) const
{
	switch (enemyType)
	{
	case EnemyType::Melee:
		return "Melee";
	case EnemyType::MidRange:
		return "MidRange";
	case EnemyType::Legacy:
	default:
		return "Legacy";
	}
}

EnemyType CrystalManager::ParseCrystalEnemyType(const std::string& enemyTypeName) const
{
	if (enemyTypeName == "GuardianBoss")
	{
		// 今回はボス生成へ接続せず、将来のBossCrystal用指定としてLegacyへフォールバックする。
		return EnemyType::Legacy;
	}
	return ParseEnemyType(enemyTypeName);
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
	ImGui::SeparatorText("Crystal HP Bar");
	ImGui::Text("表示: %s", crystalHpBarVisible_ ? "ON" : "OFF");
	ImGui::Text("常時表示: %s", crystalHpBarAlwaysVisible_ ? "ON" : "OFF");
	ImGui::Text("OffsetY / Size: %.2f / %.1f x %.1f", crystalHpBarOffsetY_, crystalHpBarWidth_, crystalHpBarHeight_);
	ImGui::Text("被弾後表示時間: %.2f 秒", crystalHpBarShowTime_);
	ImGui::Text("Draw呼び出し: %s", crystalHpBarDrawCalled_ ? "はい" : "いいえ");
	ImGui::Text("表示対象数: %d / %d", crystalHpBarVisibleCount_, static_cast<int>(crystalHpBarDebugInfos_.size()));
	for (size_t i = 0; i < crystalHpBarDebugInfos_.size(); ++i)
	{
		const auto& info = crystalHpBarDebugInfos_[i];
		ImGui::Text(
			"CrystalHPBar[%d] HP:%d/%d Rate:%.2f World:(%.2f,%.2f,%.2f) Screen:(%.1f,%.1f) visible:%s reason:%s",
			static_cast<int>(i),
			info.hp,
			info.maxHp,
			info.hpRate,
			info.worldPosition.x,
			info.worldPosition.y,
			info.worldPosition.z,
			info.screenPosition.x,
			info.screenPosition.y,
			info.visible ? "true" : "false",
			info.hiddenReason.empty() ? "-" : info.hiddenReason.c_str());
	}
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

	constexpr const char* enemyTypeLabels[] = { "旧Enemy", "近接雑魚敵", "中距離雑魚敵" };
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
