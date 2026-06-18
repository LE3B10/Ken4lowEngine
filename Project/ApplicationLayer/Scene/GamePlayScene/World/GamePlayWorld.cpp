#define NOMINMAX
#include "DirectXCommon.h"
#include "GamePlayWorld.h"
#include "GamePlayStageContext.h"
#include "GameViewportConstants.h"

#include "LightManager.h"
#include "CameraManager.h"
#include "SkyBoxManager.h"
#include "JsonDataManager.h"
#include "Wireframe.h"
#include "Player.h"
#include "Derived/GuardianBoss/GuardianBoss.h"
#include "EnemyBase.h"
#include "EnemyHPBarProjector.h"
#include "GameplayPhysicsEventHandler.h"
#include "GpuParticleManager.h"
#include "ParticleManager.h"
#include "PhysicsTestBullet.h"
#include "CollisionTypeIdDef.h"
#include "Input.h"
#include <LogString.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

#ifdef USE_IMGUI
#include <Editor/EditorModeController.h>
#include <imgui.h>
#include <GameTimer.h>
#endif

using namespace Ken4lowEngine;

namespace
{
	static constexpr uint32_t kPhysicsLayerStage = 0u;
	static constexpr uint32_t kPhysicsLayerTestObject = 1u;
	static constexpr uint32_t kPhysicsLayerPlayer = 2u;
	static constexpr uint32_t kPhysicsLayerTestBullet = 3u;
	static constexpr uint32_t kPhysicsLayerTestTarget = 4u;
	static constexpr uint32_t kPhysicsLayerPlayerBullet = 5u;
	static constexpr uint32_t kPhysicsLayerEnemy = 6u;
	static constexpr uint32_t kPhysicsLayerBoss = 7u;

	const char* ToCollisionResponseName(K4E::CollisionResponseType response)
	{
		switch (response)
		{
		case K4E::CollisionResponseType::Ignore:
			return "Ignore";
		case K4E::CollisionResponseType::Trigger:
			return "Trigger";
		case K4E::CollisionResponseType::Block:
			return "Block";
		default:
			return "Unknown";
		}
	}
}

void GamePlayWorld::Initialize(GamePlayStageContext& stageContext)
{
	const auto stageAssets = stageContext.GetCurrentStageAssets();
	stage1BeginnerBalanceEnabled_ = stageContext.IsBeginningPlainStage();

	InitializeLighting();
	InitializeSkyBox();
	InitializeCollisionSystems();
	InitializeCharacterSystems();
	InitializeHUD();
	InitializeStageAndPhysics(stageAssets);
	InitializePlayerSpawn(stageContext);
	InitializeWaveSystem(stageContext);
	InitializeBossState(stageContext);
	InitializeStage1Crystals();
	InitializeRuntimeHelpers();
}

void GamePlayWorld::InitializeLighting()
{
	auto* lightManager = LightManager::GetInstance();

	// GamePlayScene開始時は保存済みプリセットを優先し、なければ確認用ライトへ戻す。
	lightManager->ResetToDefaultLighting();
	lightManager->SetShadowCasterLightIndex(-1);
	lightManager->SetManualShadowFocusPosition({ 0.0f, 0.0f, 0.0f });
}

void GamePlayWorld::InitializeSkyBox()
{
	skyBox_ = std::make_unique<K4E::SkyBox>();
	skyBox_->Initialize("SkyBox/skybox.dds");

	K4E::JsonAssetEntry skyBoxPresetEntry;
	if (K4E::JsonDataManager::SafeLoad("Resources/DataAssets/SkyBoxPresets/debug_skybox.json", skyBoxPresetEntry))
	{
		skyBoxPresets_.FromJson(skyBoxPresetEntry.data);
		if (const K4E::SkyBoxPreset* skyBoxPreset = skyBoxPresets_.FindActivePreset())
		{
			// GamePlayでもJSONの雲パスを適用し、変換済みDDSが実描画へ到達するようにする。
			K4E::ApplySkyBoxPreset(*skyBox_, *skyBoxPreset);
		}
	}
}

void GamePlayWorld::InitializeCollisionSystems()
{
	collisionManager_ = std::make_unique<CollisionManager>();
	collisionManager_->Initialize();
	collisionSystemPolicy_.InitializeDefaults();
	UpdateCollisionSystemPolicyFromGameplayFlags();

	// 弾、キャラクター、アイテム、ステージは同じCollisionManagerへ登録し、
	// World更新の最後にまとめて衝突解決できるようにする。
	bulletManager_ = std::make_unique<BulletManager>();
	bulletManager_->Initialize(collisionManager_.get());
}

void GamePlayWorld::InitializeCharacterSystems()
{
	GameContext ctx{};
	ctx.collisionManager_ = collisionManager_.get();
	ctx.bulletManager_ = bulletManager_.get();
	characters_.Initialize(ctx);

	itemManager_.Initialize();
	itemManager_.RegisterColliders(collisionManager_.get());
	characters_.SetEnemyKilledCallback([this](const K4E::Vector3& deathPosition)
		{
			itemManager_.TryDropFromEnemyDeath(deathPosition);
		});
}

void GamePlayWorld::InitializeHUD()
{
	hudManager_ = std::make_unique<HUDManager>();
	hudManager_->SetPlayer(characters_.GetPlayer());
	hudManager_->Initialize();

	// ステージ1は初心者向けにするため、HUD上もプライマリ武器1枠だけを表示する。
	hudManager_->SetWeaponSlotVisibleSlotCount(stage1BeginnerBalanceEnabled_ ? 1 : WeaponSlot::kSlotCount);

	if (auto* player = characters_.GetPlayer())
	{
		player->SetAllowedHotbarSlotCount(stage1BeginnerBalanceEnabled_ ? 1 : WeaponSlot::kSlotCount);
		player->SetHUDManager(hudManager_.get());
	}
}

void GamePlayWorld::InitializeStageAndPhysics(const GamePlayStageContext::StageAssetPaths& stageAssets)
{
	stage_ = std::make_unique<K4E::Stage>();
	stage_->Initialize(stageAssets.jsonPath, stageAssets.modelPath);
	stage_->RegisterColliders(collisionManager_.get());
	stage_->Update();

	InitializeGameplayPhysicsTest();
	InitializeGameplayPhysicsTriggerTest();
	gameplayPhysicsParameterBridge_.Initialize();
	gameplayPhysicsParameterBridge_.RegisterAppliers(this, [this]() { ApplyGameplayPhysicsParameterSettings(); });
	ApplyGameplayPhysicsParameterSettings();

	// 敵AIとプレイヤー移動はステージAABBを参照して壁抜けや地面補正を行う。
	// Stageが所有する配列なので、World解放時に必ずnullptrへ戻す。
	EnemyBase::SetGlobalStageWorldAABBs(&stage_->GetWorldAABBs());
	EnemyBase::SetGlobalStageFloorAABBs(&stage_->GetFloorAABBs());
	EnemyBase::SetGlobalStageNavigationObstacleAABBs(&stage_->GetNavigationObstacleAABBs());

	if (auto* player = characters_.GetPlayer())
	{
		player->SetStageWorldAABBs(&stage_->GetWorldAABBs());

		WorldCollisionSettings playerCollisionSettings{};
		playerCollisionSettings.half = { 0.5f, 1.0f, 0.5f };
		playerCollisionSettings.centerOffset = { 0.0f, 1.0f, 0.0f };
		player->SetWorldCollisionSettings(playerCollisionSettings);
	}
}

void GamePlayWorld::InitializePlayerSpawn(GamePlayStageContext& stageContext)
{
	stageContext.LoadSpawnPointsFromLevel(stageContext.GetCurrentStageAssets().jsonPath);

	// ステージ目的の実行中状態は専用マネージャへ集約する。
	stageObjectiveManager_ = std::make_unique<StageObjectiveManager>();
	stageObjectiveManager_->Initialize(stageContext);

	if (auto* player = characters_.GetPlayer())
	{
		if (auto* camera = player->GetCamera())
		{
			K4E::CameraManager::GetInstance()->SetMainCamera(camera);
			// 遠景のステージパーツが途中で消えないように、GamePlay中のみFarClipを安全側に広げる。
			camera->SetFarClip(1600.0f);
		}

		player->SetSpawnOffset({ 0.0f, 0.0f, 0.0f });

		if (stageContext.HasPlayerSpawnPoint())
		{
			constexpr float kPlayerSpawnLift = 1.0f;

			K4E::Vector3 spawn = stageContext.GetPlayerSpawnPoint();
			spawn.y += kPlayerSpawnLift;
			player->SetSpawnPosition(spawn);
		}

		characters_.Update(0.0f);
	}

	if (stage_)
	{
		stage_->Update();
	}

	CollisionUpdate();
}

void GamePlayWorld::InitializeWaveSystem(GamePlayStageContext& stageContext)
{
	waveManager_ = std::make_unique<WaveManager>();

	if (stageObjectiveManager_ && stageObjectiveManager_->UsesWaveSystem())
	{
		stageContext.SetupWaves(waveManager_.get());
	}
	else
	{
		waveManager_->SetWaves({});
	}

	prevWaveNumber_ = 0;
	prevWaveInProgress_ = false;
	prevAllWavesCleared_ = false;
}

void GamePlayWorld::InitializeBossState(GamePlayStageContext& stageContext)
{
	// ボス戦専用の状態はControllerへ集約し、World側のメンバ肥大化を抑える。
	bossBattleController_.Initialize(stageContext, stage1BeginnerBalanceEnabled_);
}

void GamePlayWorld::InitializeStage1Crystals()
{
	// 現状のクリスタルはLevelDataではなくC++側の固定配置で、全破壊後にボス出現へ進む。
	// 明示的なvectorを使い、初期化子の型差で意図しない変換が起きないようにする。
	std::vector<CrystalSpawnPoint> crystalSpawnPoints;

	CrystalSpawnPoint centerCrystalSpawnPoint;
	centerCrystalSpawnPoint.crystalName = "Crystal_01";
	centerCrystalSpawnPoint.position = { 0.0f, 2.0f, 20.0f };
	centerCrystalSpawnPoint.rotation = {};
	centerCrystalSpawnPoint.scale = { 1.5f, 2.5f, 1.5f };
	centerCrystalSpawnPoint.hp = 100;
	centerCrystalSpawnPoint.maxHp = 100;
	centerCrystalSpawnPoint.spawnEnemyType = EnemyType::Melee;
	centerCrystalSpawnPoint.spawnInterval = 2.0f;
	centerCrystalSpawnPoint.maxAliveEnemies = 10;
	centerCrystalSpawnPoint.spawnRadius = 4.0f;
	centerCrystalSpawnPoint.enableInfiniteSpawn = true;
	centerCrystalSpawnPoint.spawnBossTrigger = true;
	crystalSpawnPoints.push_back(centerCrystalSpawnPoint);

	CrystalSpawnPoint rightCrystalSpawnPoint;
	rightCrystalSpawnPoint.crystalName = "Crystal_02";
	rightCrystalSpawnPoint.position = { 10.0f, 2.0f, 30.0f };
	rightCrystalSpawnPoint.rotation = {};
	rightCrystalSpawnPoint.scale = { 1.5f, 2.5f, 1.5f };
	rightCrystalSpawnPoint.hp = 100;
	rightCrystalSpawnPoint.maxHp = 100;
	rightCrystalSpawnPoint.spawnEnemyType = EnemyType::MidRange;
	rightCrystalSpawnPoint.spawnInterval = 3.0f;
	rightCrystalSpawnPoint.maxAliveEnemies = 6;
	rightCrystalSpawnPoint.spawnRadius = 4.0f;
	rightCrystalSpawnPoint.enableInfiniteSpawn = true;
	rightCrystalSpawnPoint.spawnBossTrigger = true;
	crystalSpawnPoints.push_back(rightCrystalSpawnPoint);

	CrystalSpawnPoint leftCrystalSpawnPoint;
	leftCrystalSpawnPoint.crystalName = "BossCrystal_01";
	leftCrystalSpawnPoint.position = { -10.0f, 2.0f, 30.0f };
	leftCrystalSpawnPoint.rotation = {};
	leftCrystalSpawnPoint.scale = { 1.5f, 2.5f, 1.5f };
	leftCrystalSpawnPoint.hp = 100;
	leftCrystalSpawnPoint.maxHp = 100;
	leftCrystalSpawnPoint.spawnEnemyType = EnemyType::Melee;
	leftCrystalSpawnPoint.spawnInterval = 2.0f;
	leftCrystalSpawnPoint.maxAliveEnemies = 10;
	leftCrystalSpawnPoint.spawnRadius = 4.0f;
	leftCrystalSpawnPoint.enableInfiniteSpawn = true;
	leftCrystalSpawnPoint.spawnBossTrigger = true;
	crystalSpawnPoints.push_back(leftCrystalSpawnPoint);

	crystalManager_.SetSkyBox(skyBox_.get());
	crystalManager_.Initialize(crystalSpawnPoints, collisionManager_.get(), &stage_->GetFloorAABBs(), &stage_->GetNavigationObstacleAABBs());
	crystalManager_.SetStage1BeginnerBalanceEnabled(stage1BeginnerBalanceEnabled_);
	crystalManager_.SetProgressDebugStatus(characters_.GetAliveNormalEnemyCount(), bossBattleController_.IsSpawnConditionMet(), bossBattleController_.IsSpawned(), bossBattleController_.GetBossSpawnPosition());
}

void GamePlayWorld::InitializeRuntimeHelpers()
{
	enemyHpBarManager_.Initialize();
	aimTargetDetector_.Initialize();
	stage1TutorialController_.Start(BuildStage1TutorialDependencies(), stage1BeginnerBalanceEnabled_, bossBattleController_.IsDefeated());
}


void GamePlayWorld::Finalize()
{
	bossBattleController_.Finalize(BuildBossBattleDependencies());
	crystalManager_.Finalize();
	stageObjectiveManager_.reset();
	waveManager_.reset();
	hudManager_.reset();

	EnemyBase::SetGlobalStageWorldAABBs(nullptr);
	EnemyBase::SetGlobalStageFloorAABBs(nullptr);
	EnemyBase::SetGlobalStageNavigationObstacleAABBs(nullptr);
	itemManager_.Clear();

	UnregisterGameplayPhysicsBulletTriggerTargets();
	characters_.Finalize();

	UnregisterPlayerPhysicsGroundCheck();
	UnregisterGameplayPhysicsTriggerTest();
	gameplayPhysicsParameterBridge_.Finalize(this);
	UnbindGameplayPhysicsStageColliders();
	gameplayPhysicsWorld_.ClearColliders();
	physicsTestObject_.reset();
	physicsTestBullet_.reset();
	physicsTriggerTargetObject_.reset();
	gameplayPhysicsEventHandler_.reset();

	stage_.reset();
	bulletManager_.reset();

	if (collisionManager_)
	{
		collisionManager_->Reset();
	}
	collisionManager_.reset();

	skyBox_.reset();
}

void GamePlayWorld::Update(float deltaTime)
{
	UpdateStageRuntime();

	if (UpdateBlockingStage1Intro(deltaTime))
	{
		return;
	}

	if (UpdateBlockingBossIntro(deltaTime))
	{
		return;
	}

	UpdateGameplayActors(deltaTime);
	UpdateBossRuntime(deltaTime);
	UpdateItemRuntime(deltaTime);
	UpdateShadowRuntime();
	UpdateBulletAndCollisionRuntime(deltaTime);
	UpdateSkyBoxRuntime(deltaTime);
	UpdateAimTargetRuntime();
	UpdateHpBarRuntime(deltaTime);
	UpdateHudRuntime(deltaTime);
	UpdateWaveRuntime(deltaTime);
	UpdateStageObjectiveRuntime(deltaTime);
}

void GamePlayWorld::UpdateStageRuntime()
{
	if (stage_)
	{
		stage_->Update();
	}
}

bool GamePlayWorld::UpdateBlockingStage1Intro(float deltaTime)
{
	if (!stage1TutorialController_.IsActive())
	{
		return false;
	}

	stage1TutorialController_.Update(BuildStage1TutorialDependencies(), deltaTime);
	return true;
}

bool GamePlayWorld::UpdateBlockingBossIntro(float deltaTime)
{
	if (!bossBattleController_.IsIntroGameplayPaused())
	{
		return false;
	}

	// ボス登場演出中は通常ゲーム進行を止め、カメラとボス登場Transformだけを進める。
	bossBattleController_.UpdatePausedWorld(BuildBossBattleDependencies(), deltaTime);

	if (skyBox_)
	{
		skyBox_->Update();
		skyBox_->AdvanceCloudLayer(deltaTime);
	}

	UpdateShadowLightViewProjection();
	if (stage_)
	{
		stage_->UpdateShadowMatrix(shadowLightViewProjection_);
	}
	characters_.UpdateShadowMatrix(shadowLightViewProjection_);
	if (auto* boss = bossBattleController_.GetBoss())
	{
		boss->UpdateShadowMatrix(shadowLightViewProjection_);
	}
	return true;
}

void GamePlayWorld::UpdateGameplayActors(float deltaTime)
{
	// 先にキャラクターとクリスタルを更新し、敵残数とクリスタル破壊状態からボス出現条件を評価する。
	characters_.Update(deltaTime);
	crystalManager_.Update(characters_, deltaTime);
	bossBattleController_.UpdateSpawnProgress(BuildBossBattleDependencies());
	bossBattleController_.UpdateIntro(BuildBossBattleDependencies(), deltaTime);
}

void GamePlayWorld::UpdateBossRuntime(float deltaTime)
{
	bossBattleController_.UpdateRuntime(BuildBossBattleDependencies(), deltaTime);
}

void GamePlayWorld::UpdateItemRuntime(float deltaTime)
{
	if (auto* player = characters_.GetPlayer())
	{
		itemManager_.Update(player, deltaTime);
	}
	else
	{
		itemManager_.Update(deltaTime);
	}
}

void GamePlayWorld::UpdateShadowRuntime()
{
	UpdateShadowLightViewProjection();

	if (stage_)
	{
		stage_->UpdateShadowMatrix(shadowLightViewProjection_);
	}
	characters_.UpdateShadowMatrix(shadowLightViewProjection_);
	if (auto* boss = bossBattleController_.GetBoss())
	{
		boss->UpdateShadowMatrix(shadowLightViewProjection_);
	}
}

void GamePlayWorld::UpdateBulletAndCollisionRuntime(float deltaTime)
{
	if (bulletManager_)
	{
		// 弾更新と衝突更新はデバッグ表示用に処理時間を保存する。
		const auto begin = std::chrono::steady_clock::now();
		bulletManager_->Update(deltaTime);
		lastBulletUpdateMs_ = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - begin).count();
	}

	UpdateGameplayPhysicsTest(deltaTime);

	{
		const auto begin = std::chrono::steady_clock::now();
		CollisionUpdate();
		lastCollisionUpdateMs_ = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - begin).count();
	}
}

void GamePlayWorld::UpdateSkyBoxRuntime(float deltaTime)
{
	if (skyBox_)
	{
		skyBox_->Update();
		skyBox_->AdvanceCloudLayer(deltaTime);
	}
}

void GamePlayWorld::UpdateAimTargetRuntime()
{
	if (!collisionManager_)
	{
		return;
	}

	if (auto* player = characters_.GetPlayer())
	{
		if (auto* camera = player->GetCamera())
		{
			aimTargetDetector_.Update(*camera, *collisionManager_);
		}
	}
}

void GamePlayWorld::UpdateHpBarRuntime(float deltaTime)
{
	if (auto* player = characters_.GetPlayer())
	{
		if (auto* camera = player->GetCamera())
		{
			const std::vector<EnemyBase*> enemyList = characters_.GetEnemyRawList();
			// 敵HPバー投影は固定内部解像度1920x1080を基準にする。
			const float width = static_cast<float>(K4E::GameViewportConstants::Width);
			const float height = static_cast<float>(K4E::GameViewportConstants::Height);

			enemyHpBarManager_.Update(
				enemyList,
				camera->GetViewMatrix(),
				camera->GetProjectionMatrix(),
				width,
				height,
				deltaTime,
				aimTargetDetector_.GetTargetEnemy(),
				aimTargetDetector_.ShouldShowHpBarOnlyWhenAimed(),
				aimTargetDetector_.GetHpBarVisibleHoldTime()
			);
			crystalManager_.UpdateHpBars(
				camera->GetViewMatrix(),
				camera->GetProjectionMatrix(),
				width,
				height,
				deltaTime,
				aimTargetDetector_.GetTargetCrystal(),
				aimTargetDetector_.ShouldShowHpBarOnlyWhenAimed(),
				aimTargetDetector_.GetHpBarVisibleHoldTime());
		}
	}
}

void GamePlayWorld::UpdateHudRuntime(float deltaTime)
{
	if (!hudManager_ || !characters_.GetPlayer())
	{
		return;
	}

	// 照準先判定、HP、Wave状態などWorld側でしか分からない情報をHUDへ集約する。
	const bool isTargetingEnemy = aimTargetDetector_.HasDamageableTarget();
	hudManager_->SetCrosshairTargetColors(
		aimTargetDetector_.GetCrosshairNormalColor(),
		aimTargetDetector_.GetCrosshairTargetColor());
	hudManager_->SetCrosshairTargetingEnemy(isTargetingEnemy);

	hudManager_->SetHP(
		characters_.GetPlayer()->GetHP(),
		characters_.GetPlayer()->GetMaxHP());

	const bool bossBattleActive = bossBattleController_.IsBossBattleActive();
	bossBattleController_.UpdateHud(BuildBossBattleDependencies(), deltaTime);
	stage1TutorialController_.UpdateObjectiveGuideHud(
		BuildStage1TutorialDependencies(),
		stage1BeginnerBalanceEnabled_,
		bossBattleActive,
		bossBattleController_.IsSpawned(),
		bossBattleController_.HasIntroPlayed(),
		bossBattleController_.IsDefeated());
	bossBattleController_.UpdateBossGuideHud(*characters_.GetPlayer(), *hudManager_);
	hudManager_->Update(deltaTime);
}

void GamePlayWorld::UpdateWaveRuntime(float deltaTime)
{
	if (!stageObjectiveManager_ || !stageObjectiveManager_->UsesWaveSystem() || !waveManager_)
	{
		return;
	}

	// WaveManagerはウェーブ使用ステージだけで更新し、非ウェーブ目的と分離する。
	waveManager_->Update(characters_, deltaTime);

	const int currentWave = waveManager_->GetCurrentWaveNumber();
	const int totalWaves = waveManager_->GetTotalWaveCount();
	const bool isWaveInProgress = waveManager_->IsWaveInProgress();
	const bool isWaitingNextWave = waveManager_->IsWaitingNextWave();
	const bool isAllWavesCleared = waveManager_->IsAllWavesCleared();
	const bool isFinalWave = (currentWave >= totalWaves);

	if (hudManager_)
	{
		WaveUI::DisplayState state{};
		state.currentWave = currentWave;
		state.totalWaves = totalWaves;
		state.isWaveInProgress = isWaveInProgress;
		state.isWaitingNextWave = isWaitingNextWave;
		state.isAllWavesCleared = isAllWavesCleared;

		hudManager_->SetWaveDisplayState(state);

		if (isWaveInProgress && (!prevWaveInProgress_ || currentWave != prevWaveNumber_))
		{
			hudManager_->NotifyWaveStarted(currentWave, isFinalWave);
		}

		if (isAllWavesCleared && !prevAllWavesCleared_)
		{
			hudManager_->NotifyAllWavesCleared();
		}
	}

	prevWaveNumber_ = currentWave;
	prevWaveInProgress_ = isWaveInProgress;
	prevAllWavesCleared_ = isAllWavesCleared;
}

void GamePlayWorld::UpdateStageObjectiveRuntime(float deltaTime)
{
	if (stageObjectiveManager_)
	{
		stageObjectiveManager_->Update(deltaTime);
	}
}


void GamePlayWorld::UpdateIntroVisuals()
{
	if (stage_)
	{
		stage_->Update();
	}

	UpdateShadowLightViewProjection();

	if (stage_)
	{
		stage_->UpdateShadowMatrix(shadowLightViewProjection_);
	}

	characters_.UpdateShadowMatrix(shadowLightViewProjection_);

	if (skyBox_)
	{
		skyBox_->Update();
	}
}

void GamePlayWorld::UpdateEquipIntro(float deltaTime)
{
	if (stage_)
	{
		stage_->Update();
	}

	// EquipIntro中でも武器構えアニメーションだけは更新する。
	characters_.UpdatePlayerOnly(deltaTime);

	UpdateShadowLightViewProjection();
	if (stage_)
	{
		stage_->UpdateShadowMatrix(shadowLightViewProjection_);
	}
	characters_.UpdateShadowMatrix(shadowLightViewProjection_);

	if (skyBox_)
	{
		skyBox_->Update();
		skyBox_->AdvanceCloudLayer(deltaTime);
	}
}

void GamePlayWorld::Draw3D(bool hideCharactersDuringIntro)
{
#ifdef _DEBUG
	const bool debugVisualsEnabled =
#ifdef USE_IMGUI
		K4E::EditorModeController::GetInstance()->ShouldDrawDebugVisuals();
#else
		true;
#endif // USE_IMGUI
#endif // _DEBUG

	K4E::SkyBoxManager::GetInstance()->SetRenderSetting();
	if (skyBox_)
	{
		skyBox_->Draw();
		skyBox_->DrawCloudLayer();
	}

	if (!hideCharactersDuringIntro)
	{
		characters_.Draw();
		bossBattleController_.DrawBoss();
	}

	if (stage_)
	{
		stage_->Draw();
#ifdef _DEBUG
		if (debugVisualsEnabled)
		{
			stage_->DrawChunkDebug();
		}
#endif
	}

	crystalManager_.Draw();

	if (bulletManager_)
	{
		bulletManager_->Draw();
	}

	itemManager_.Draw();
	bossBattleController_.DrawClearItem();
	DrawGameplayPhysicsTest();

#ifdef _DEBUG
	if (debugVisualsEnabled && collisionManager_)
	{
		collisionManager_->Draw();
	}

	if (debugVisualsEnabled)
	{
		K4E::Wireframe::GetInstance()->DrawGrid(
			200.0f,
			50.0f,
			{ 0.25f, 0.25f, 0.25f, 1.0f });
	}
#endif // _DEBUG
}

void GamePlayWorld::DrawBossIntro3D()
{
	K4E::SkyBoxManager::GetInstance()->SetRenderSetting();
	if (skyBox_)
	{
		skyBox_->Draw();
		skyBox_->DrawCloudLayer();
	}

	if (stage_)
	{
		stage_->Draw();
	}

	bossBattleController_.DrawBossIntro3D();
}

void GamePlayWorld::DrawShadow(bool hideCharactersDuringIntro)
{
	if (stage_)
	{
		stage_->DrawShadow();
	}

	if (!hideCharactersDuringIntro)
	{
		characters_.DrawShadow();
		bossBattleController_.DrawShadow();
	}
}

void GamePlayWorld::DrawBossIntroShadow()
{
	if (stage_)
	{
		stage_->DrawShadow();
	}

	bossBattleController_.DrawBossIntroShadow();
}

void GamePlayWorld::DrawHUD(bool hideDuringIntro)
{
	if (hideDuringIntro) { return; }

	// クリスタル頭上HPバーは3D描画後、画面固定HUDより前に重ねる。
	crystalManager_.DrawHpBars();

	enemyHpBarManager_.Draw();

	if (hudManager_)
	{
		hudManager_->Draw();
	}
}

void GamePlayWorld::DrawImGui()
{
	// 互換用の一括描画はGame Debugの補助項目として残す。
	itemManager_.DrawImGui();
}

void GamePlayWorld::DrawGameDebugImGui()
{
#ifdef USE_IMGUI
	// Game DebugにはGamePlayScene全体の簡易ステータスをまとめる。
	// ステージ目的の進行状態はStageObjectiveManagerから参照して表示する。
	if (stageObjectiveManager_)
	{
		ImGui::Text("Stage Time: %.2f sec", stageObjectiveManager_->GetStageElapsedSec());
		ImGui::Text("Activated Devices: %d / %d", stageObjectiveManager_->GetActivatedDeviceCount(), stageObjectiveManager_->GetDevicePointCount());
		ImGui::Text("Defense Targets: %d", stageObjectiveManager_->GetDefenseTargetPointCount());
		ImGui::Text("Goal Points: %d", stageObjectiveManager_->GetGoalPointCount());
		ImGui::Text("Boss Spawn Point: %s", stageObjectiveManager_->HasBossSpawnPoint() ? "true" : "false");
		ImGui::Text("Reached Goal: %s", stageObjectiveManager_->HasReachedGoal() ? "true" : "false");
		ImGui::Text("Boss Defeated: %s", stageObjectiveManager_->IsBossDefeated() ? "true" : "false");
		ImGui::Text("Defense Target Destroyed: %s", stageObjectiveManager_->IsDefenseTargetDestroyed() ? "true" : "false");
	}
	ImGui::Text("Player Dead: %s", IsPlayerDead() ? "true" : "false");
	ImGui::Text("Enemies: %d", characters_.GetEnemyCount());
	DrawGameplayPhysicsTestImGui();
	bossBattleController_.DrawImGui(BuildBossBattleDependencies(), IsBossIntroPresentationActive());
	if (auto* player = characters_.GetPlayer())
	{
		ImGui::Text("Player HP: %.1f / %.1f", player->GetHP(), player->GetMaxHP());
	}
	else
	{
		ImGui::Text("Player HP: 0.0 / 0.0");
	}

	aimTargetDetector_.DrawImGui();

	crystalManager_.DrawImGui();

	auto* wireframe = K4E::Wireframe::GetInstance();
	bool debugDrawEnabled = wireframe->IsDebugDrawEnabled();
#ifdef _DEBUG
	if (ImGui::Checkbox("デバッグ描画有効", &debugDrawEnabled))
	{
		wireframe->SetDebugDrawEnabled(debugDrawEnabled);
	}
#else
	ImGui::Text("デバッグ描画有効: いいえ");
#endif
	ImGui::Text("Release時デバッグ描画無効: %s", K4E::Wireframe::IsDebugDrawSupported() ? "Debugビルド" : "はい");

	const float fps = K4E::GameTimer::GetInstance()->GetFPS();
	const auto* particleManager = K4E::ParticleManager::GetInstance();
	const auto* gpuParticleManager = K4E::GpuParticleManager::GetInstance();

	const size_t activeBulletCount = bulletManager_ ? bulletManager_->GetActiveCount() : 0;
	const size_t totalBulletCount = bulletManager_ ? bulletManager_->GetCount() : 0;
	const size_t activeParticleCount = particleManager ? particleManager->GetActiveParticleCount() : 0;
	const size_t totalParticleCount = particleManager ? particleManager->GetTotalParticleCount() : 0;
	const uint32_t gpuParticleActiveCount = gpuParticleManager ? gpuParticleManager->GetEstimatedActiveParticleCount() : 0;
	const size_t particleEmitterCount = gpuParticleManager ? gpuParticleManager->GetEmitterCount() : 0;
	const size_t activeParticleEmitterCount = gpuParticleManager ? gpuParticleManager->GetActiveEmitterCount() : 0;
	const size_t colliderCount = collisionManager_ ? collisionManager_->GetColliderCount() : 0;
	const size_t bulletColliderCount = collisionManager_
		? collisionManager_->GetColliderCountByType(static_cast<uint32_t>(CollisionTypeIdDef::kBullet))
		: 0;
	const size_t enemyBulletColliderCount = collisionManager_
		? collisionManager_->GetColliderCountByType(static_cast<uint32_t>(CollisionTypeIdDef::kEnemyBullet)) +
		collisionManager_->GetColliderCountByType(static_cast<uint32_t>(CollisionTypeIdDef::kBossBullet))
		: 0;
	const uint32_t drawCallCount = gpuParticleManager ? gpuParticleManager->GetLastDrawCallCount() : 0;

	ImGui::SeparatorText("Performance Counters");
	ImGui::Text("FPS: %.1f", fps);
	ImGui::Text("Active Bullet Count: %zu", activeBulletCount);
	ImGui::Text("Total Bullet Count: %zu", totalBulletCount);
	ImGui::Text("Active Particle Count: %zu", activeParticleCount);
	ImGui::Text("Total Particle Count: %zu", totalParticleCount);
	ImGui::Text("GPU Particle Active Count (estimated): %u", gpuParticleActiveCount);
	ImGui::Text("Particle Emitter Count: %zu (active: %zu)", particleEmitterCount, activeParticleEmitterCount);
	ImGui::Text("CollisionManager Collider Count: %zu", colliderCount);
	ImGui::Text("Bullet Collider Count: %zu (enemy/boss: %zu)", bulletColliderCount, enemyBulletColliderCount);
	ImGui::Text("Draw Call Count (GPU Particle): %u", drawCallCount);
	ImGui::SeparatorText("Simple Profile");
	ImGui::Text("BulletManager::Update: %.3f ms", lastBulletUpdateMs_);
	ImGui::Text("CollisionManager::CheckAllCollisions: %.3f ms", lastCollisionUpdateMs_);
#endif
}

void GamePlayWorld::DrawEnemyDebugImGui()
{
#ifdef USE_IMGUI
	// Enemy DebugにはEnemy HPBar Managerの軽量統計を追加する。
	if (ImGui::CollapsingHeader("HPBar Debug", ImGuiTreeNodeFlags_DefaultOpen))
	{
		enemyHpBarManager_.DrawImGuiContent();
	}
#endif
}

void GamePlayWorld::DrawCollisionDebugImGui()
{
#ifdef USE_IMGUI
	// Collision Debugには当たり判定関連の表示切替と補助情報を集約する。
	if (collisionManager_) { collisionManager_->DrawImGui(); }
#endif
}

void GamePlayWorld::SyncAfterPlayerSpawn()
{
	characters_.Update(0.0f);

	if (stage_)
	{
		stage_->Update();
	}

	CollisionUpdate();
}

void GamePlayWorld::StartWaves()
{
	if (stageObjectiveManager_ && stageObjectiveManager_->UsesWaveSystem() && waveManager_ && !waveManager_->HasStarted())
	{
		waveManager_->Start();
	}
}

void GamePlayWorld::WarmupStartGameplayForIntro()
{
	// カメラ切り替え時の一括初期化を避けるため、イントロ中に敵・武器・腕表示を事前生成して非表示で温める。
	SetStartGameplayVisualsVisible(false);

	StartWaves();
	if (stageObjectiveManager_ && stageObjectiveManager_->UsesWaveSystem() && waveManager_)
	{
		waveManager_->Update(characters_, 0.0f);
	}

	characters_.WarmupStartGameplayVisuals();
	CollisionUpdate();
}

void GamePlayWorld::SetStartGameplayVisualsVisible(bool visible)
{
	characters_.SetStartGameplayVisualsVisible(visible);
}

bool GamePlayWorld::IsPlayerDead()
{
	const auto* player = characters_.GetPlayer();
	return player && player->GetHP() <= 0;
}

bool GamePlayWorld::IsAllWavesCleared() const
{
	return waveManager_ && waveManager_->IsAllWavesCleared();
}

bool GamePlayWorld::IsStageObjectiveCleared() const
{
	if (bossBattleController_.IsGameClearRequested())
	{
		// クリアCube取得後はStageObjectiveManagerの目的種別に関係なく、ゲームクリアとして扱う。
		return true;
	}

	return stageObjectiveManager_ && stageObjectiveManager_->IsStageObjectiveCleared(IsAllWavesCleared());
}

bool GamePlayWorld::IsStageObjectiveFailed() const
{
	return stageObjectiveManager_ && stageObjectiveManager_->IsStageObjectiveFailed();
}


Stage1TutorialController::Dependencies GamePlayWorld::BuildStage1TutorialDependencies()
{
	Stage1TutorialController::Dependencies deps{};
	deps.characters = &characters_;
	deps.hudManager = hudManager_.get();
	deps.crystalManager = &crystalManager_;
	deps.itemManager = &itemManager_;
	deps.bulletManager = bulletManager_.get();
	deps.collisionManager = collisionManager_.get();
	deps.skyBox = skyBox_.get();
	deps.stage = stage_.get();
	deps.shadowLightViewProjection = &shadowLightViewProjection_;
	deps.collisionUpdate = [this]()
		{
			CollisionUpdate();
		};
	deps.updateShadowLightViewProjection = [this]()
		{
			UpdateShadowLightViewProjection();
		};
	return deps;
}

BossBattleController::Dependencies GamePlayWorld::BuildBossBattleDependencies()
{
	BossBattleController::Dependencies deps{};
	deps.characters = &characters_;
	deps.hudManager = hudManager_.get();
	deps.crystalManager = &crystalManager_;
	deps.collisionManager = collisionManager_.get();
	deps.stage = stage_.get();
	deps.shadowLightViewProjection = &shadowLightViewProjection_;
	deps.setBossDefeated = [this](bool defeated)
		{
			SetBossDefeated(defeated);
		};
	deps.updateShadowLightViewProjection = [this]()
		{
			UpdateShadowLightViewProjection();
		};
	return deps;
}

void GamePlayWorld::SetDefenseTargetDestroyed(bool destroyed)
{
	if (stageObjectiveManager_)
	{
		stageObjectiveManager_->SetDefenseTargetDestroyed(destroyed);
	}
}

void GamePlayWorld::AddActivatedDeviceCount(int amount)
{
	if (stageObjectiveManager_)
	{
		stageObjectiveManager_->AddActivatedDeviceCount(amount);
	}
}

void GamePlayWorld::SetReachedGoal(bool reached)
{
	if (stageObjectiveManager_)
	{
		stageObjectiveManager_->SetReachedGoal(reached);
	}
}

void GamePlayWorld::SetBossDefeated(bool defeated)
{
	if (stageObjectiveManager_)
	{
		stageObjectiveManager_->SetBossDefeated(defeated);
	}
}

void GamePlayWorld::CollisionUpdate()
{
	if (!collisionManager_) { return; }

	// PhysicsWorld移行時に二重処理を避けるため、Legacy側の判定はPolicyで担当を確認しながら段階的に整理する。
	collisionManager_->Update();
	collisionManager_->CheckAllCollisions();
}

void GamePlayWorld::InitializeGameplayPhysicsTest()
{
	// 本編挙動へ影響しない明示ONの物理テストとして、独立したPhysicsWorldとTestObjectを準備する。
	gameplayPhysicsWorld_.SetUseFixedStep(true);
	gameplayPhysicsWorld_.SetPositionSolveEnabled(true);
	gameplayPhysicsWorld_.SetFrictionSolveEnabled(true);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerStage, kPhysicsLayerStage, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerStage, kPhysicsLayerTestObject, K4E::CollisionResponseType::Block);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerStage, kPhysicsLayerPlayer, K4E::CollisionResponseType::Block);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerTestObject, kPhysicsLayerPlayer, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerTestBullet, kPhysicsLayerStage, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerTestBullet, kPhysicsLayerTestObject, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerTestBullet, kPhysicsLayerPlayer, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerTestBullet, kPhysicsLayerTestBullet, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerTestBullet, kPhysicsLayerTestTarget, K4E::CollisionResponseType::Trigger);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerTestTarget, kPhysicsLayerStage, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerTestTarget, kPhysicsLayerTestObject, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerTestTarget, kPhysicsLayerPlayer, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerTestTarget, kPhysicsLayerTestTarget, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerPlayerBullet, kPhysicsLayerEnemy, K4E::CollisionResponseType::Trigger);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerPlayerBullet, kPhysicsLayerBoss, K4E::CollisionResponseType::Trigger);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerPlayerBullet, kPhysicsLayerStage, K4E::CollisionResponseType::Trigger);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerPlayerBullet, kPhysicsLayerPlayer, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerPlayerBullet, kPhysicsLayerTestObject, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerPlayerBullet, kPhysicsLayerTestBullet, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerPlayerBullet, kPhysicsLayerTestTarget, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerPlayerBullet, kPhysicsLayerPlayerBullet, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerEnemy, kPhysicsLayerStage, K4E::CollisionResponseType::Ignore);
	gameplayPhysicsWorld_.GetResponseMatrix().SetResponse(kPhysicsLayerBoss, kPhysicsLayerStage, K4E::CollisionResponseType::Ignore);

	if (bulletManager_)
	{
		bulletManager_->SetPhysicsTriggerWorld(&gameplayPhysicsWorld_, kPhysicsLayerPlayerBullet);
	}

	physicsTestRigidbody_.SetBodyType(K4E::BodyType::Dynamic);
	physicsTestRigidbody_.SetMass(1.0f);
	physicsTestRigidbody_.SetUseGravity(true);
	physicsTestRigidbody_.SetRestitution(0.0f);
	physicsTestRigidbody_.SetDynamicFriction(0.2f);
	physicsTestRigidbody_.SetSleepEnabled(false);

	physicsTestCollider_.SetRigidbody(&physicsTestRigidbody_);
	physicsTestCollider_.SetCollisionLayer(kPhysicsLayerTestObject);
	gameplayPhysicsWorld_.RegisterRigidbody(&physicsTestRigidbody_);
	gameplayPhysicsWorld_.RegisterCollider(&physicsTestCollider_);

	playerGroundRigidbody_.SetBodyType(K4E::BodyType::Kinematic);
	playerGroundRigidbody_.SetUseGravity(false);
	playerGroundCollider_.SetRigidbody(&playerGroundRigidbody_);
	playerGroundCollider_.SetCollisionLayer(kPhysicsLayerPlayer);

	if (auto* player = characters_.GetPlayer())
	{
		physicsTestInitialPosition_ = player->GetCenterPosition() + K4E::Vector3{ 0.0f, 8.0f, 3.0f };
	}
	else
	{
		physicsTestInitialPosition_ = { 0.0f, 8.0f, 0.0f };
	}

	physicsTestObject_ = std::make_unique<K4E::Object3D>();
	physicsTestObject_->Initialize("Test/cube.gltf");
	physicsTestObject_->SetScale(physicsTestHalfSize_ * 2.0f);
	physicsTestObject_->SetColor({ 0.2f, 0.9f, 1.0f, 1.0f });

	ResetGameplayPhysicsTestObject();
}

void GamePlayWorld::ResetGameplayPhysicsTestObject()
{
	// PhysicsTestObjectを初期位置へ戻し、速度・力・接触状態をリセットする。
	physicsTestPosition_ = physicsTestInitialPosition_;
	physicsTestRigidbody_.SetVelocity({});
	physicsTestRigidbody_.ClearForces();
	physicsTestRigidbody_.ClearFrameState();
	physicsTestRigidbody_.WakeUp();
	SyncGameplayPhysicsTestCollider();

	if (physicsTestObject_)
	{
		physicsTestObject_->SetTranslate(physicsTestPosition_);
		physicsTestObject_->Update();
	}
}

void GamePlayWorld::InitializeGameplayPhysicsTriggerTest()
{
	// PhysicsWorldのTriggerEventを本編側で受け取るため、既存Bulletとは別のテスト弾とターゲットを準備する。
	physicsTestBullet_ = std::make_unique<PhysicsTestBullet>();
	physicsTestBullet_->Initialize(kPhysicsLayerTestBullet);

	gameplayPhysicsEventHandler_ = std::make_unique<GameplayPhysicsEventHandler>();
	gameplayPhysicsEventHandler_->Configure(physicsTestBullet_.get(), &physicsTriggerTargetCollider_);

	physicsTriggerTargetRigidbody_.SetBodyType(K4E::BodyType::Static);
	physicsTriggerTargetRigidbody_.SetUseGravity(false);
	physicsTriggerTargetCollider_.SetRigidbody(&physicsTriggerTargetRigidbody_);
	physicsTriggerTargetCollider_.SetCollisionLayer(kPhysicsLayerTestTarget);
	physicsTriggerTargetCollider_.SetTrigger(true);
	physicsTriggerTargetCollider_.SetEnabled(false);

	physicsTriggerTargetObject_ = std::make_unique<K4E::Object3D>();
	physicsTriggerTargetObject_->Initialize("Test/cube.gltf");
	physicsTriggerTargetObject_->SetScale(physicsTriggerTargetHalfSize_ * 2.0f);
	physicsTriggerTargetObject_->SetColor({ 0.2f, 1.0f, 0.3f, 1.0f });
	physicsTriggerTargetObject_->Update();

	ResetGameplayPhysicsTriggerTest();
}

void GamePlayWorld::SetGameplayPhysicsTriggerTestEnabled(bool enabled)
{
	enableGameplayPhysicsTriggerTest_ = enabled;
	usePhysicsForTriggerTest_ = enabled;
	UpdateCollisionSystemPolicyFromGameplayFlags();
	if (enableGameplayPhysicsTriggerTest_)
	{
		RegisterGameplayPhysicsTriggerTest();
		ResetGameplayPhysicsTriggerTest();
	}
	else
	{
		UnregisterGameplayPhysicsTriggerTest();
	}
}

void GamePlayWorld::RegisterGameplayPhysicsTriggerTest()
{
	// TriggerEvent確認用Collider/Listenerを登録し、PhysicsWorldから本編側へイベントを通知できるようにする。
	if (!physicsTestBullet_ || !gameplayPhysicsEventHandler_)
	{
		return;
	}

	if (!gameplayPhysicsTriggerTestRegistered_)
	{
		gameplayPhysicsWorld_.RegisterRigidbody(physicsTestBullet_->GetRigidbody());
		gameplayPhysicsWorld_.RegisterCollider(physicsTestBullet_->GetCollider());
		gameplayPhysicsWorld_.RegisterRigidbody(&physicsTriggerTargetRigidbody_);
		gameplayPhysicsWorld_.RegisterCollider(&physicsTriggerTargetCollider_);
		gameplayPhysicsTriggerTestRegistered_ = true;
	}
	RegisterGameplayPhysicsEventListener();
}

void GamePlayWorld::UnregisterGameplayPhysicsTriggerTest()
{
	// 破棄済みポインタ参照を防ぐため、Scene終了や無効化時にListenerとCollider登録を解除する。
	if (gameplayPhysicsEventListenerRegistered_ && gameplayPhysicsEventHandler_)
	{
		if (!usePhysicsForBulletTrigger_)
		{
			UnregisterGameplayPhysicsEventListener();
		}
	}
	if (gameplayPhysicsTriggerTestRegistered_)
	{
		if (physicsTestBullet_)
		{
			gameplayPhysicsWorld_.UnregisterCollider(physicsTestBullet_->GetCollider());
			gameplayPhysicsWorld_.UnregisterRigidbody(physicsTestBullet_->GetRigidbody());
		}
		gameplayPhysicsWorld_.UnregisterCollider(&physicsTriggerTargetCollider_);
		gameplayPhysicsWorld_.UnregisterRigidbody(&physicsTriggerTargetRigidbody_);
		gameplayPhysicsTriggerTestRegistered_ = false;
	}

	physicsTriggerTargetCollider_.SetEnabled(false);
	if (physicsTestBullet_)
	{
		physicsTestBullet_->Kill();
	}
}

void GamePlayWorld::RegisterGameplayPhysicsEventListener()
{
	// PhysicsWorldのTriggerEventをGameplay側へ渡すため、テスト弾/実Bullet共通のListenerを登録する。
	if (!gameplayPhysicsEventListenerRegistered_ && gameplayPhysicsEventHandler_)
	{
		gameplayPhysicsWorld_.AddPhysicsEventListener(gameplayPhysicsEventHandler_.get());
		gameplayPhysicsEventListenerRegistered_ = true;
	}
}

void GamePlayWorld::UnregisterGameplayPhysicsEventListener()
{
	// 破棄済みポインタ参照を防ぐため、不要になったListenerはPhysicsWorldから外す。
	if (gameplayPhysicsEventListenerRegistered_ && gameplayPhysicsEventHandler_)
	{
		gameplayPhysicsWorld_.RemovePhysicsEventListener(gameplayPhysicsEventHandler_.get());
		gameplayPhysicsEventListenerRegistered_ = false;
	}
}

void GamePlayWorld::ResetGameplayPhysicsTriggerTest()
{
	// テスト弾とターゲットをPlayer前方へ置き、TriggerEnterの再確認ができる状態に戻す。
	K4E::Vector3 basePosition{ 0.0f, 2.0f, 0.0f };
	if (auto* player = characters_.GetPlayer())
	{
		basePosition = player->GetCenterPosition();
		basePosition.y += 1.0f;
	}

	physicsTestBulletSpawnPosition_ = basePosition + K4E::Vector3{ 0.0f, 0.0f, 2.0f };
	physicsTriggerTargetPosition_ = basePosition + K4E::Vector3{ 0.0f, 0.0f, 8.0f };
	SyncGameplayPhysicsTriggerTarget();

	if (physicsTestBullet_)
	{
		physicsTestBullet_->Reset(physicsTestBulletSpawnPosition_, physicsTestBulletInitialVelocity_);
	}
	if (gameplayPhysicsEventHandler_)
	{
		gameplayPhysicsEventHandler_->Reset();
	}
}

void GamePlayWorld::UpdateGameplayPhysicsTriggerTest(float deltaTime)
{
	// 既存Bulletへ触れず、PhysicsWorldのTriggerEvent確認用テスト弾だけを更新する。
	if (!enableGameplayPhysicsTriggerTest_)
	{
		return;
	}

	if (!gameplayPhysicsTriggerTestRegistered_)
	{
		RegisterGameplayPhysicsTriggerTest();
	}

	if (physicsTestBullet_)
	{
		physicsTestBullet_->Update(deltaTime);
	}
	SyncGameplayPhysicsTriggerTarget();
}

void GamePlayWorld::DrawGameplayPhysicsTriggerTest()
{
#ifdef _DEBUG
	if (!enableGameplayPhysicsTriggerTest_)
	{
		return;
	}

	if (physicsTriggerTargetObject_)
	{
		const bool hit = gameplayPhysicsEventHandler_ && gameplayPhysicsEventHandler_->HasTriggerHit();
		physicsTriggerTargetObject_->SetColor(hit ? K4E::Vector4{ 1.0f, 0.2f, 0.2f, 1.0f } : K4E::Vector4{ 0.2f, 1.0f, 0.3f, 1.0f });
		physicsTriggerTargetObject_->SetTranslate(physicsTriggerTargetPosition_);
		physicsTriggerTargetObject_->Update();
		physicsTriggerTargetObject_->Draw();
	}
	if (physicsTestBullet_)
	{
		physicsTestBullet_->Draw();
	}

	K4E::Wireframe::GetInstance()->DrawAABB(
		physicsTriggerTargetCollider_.GetAABB(),
		gameplayPhysicsEventHandler_ && gameplayPhysicsEventHandler_->HasTriggerHit()
		? K4E::Vector4{ 1.0f, 0.2f, 0.2f, 1.0f }
	: K4E::Vector4{ 0.2f, 1.0f, 0.3f, 1.0f });
	if (physicsTestBullet_)
	{
		K4E::Wireframe::GetInstance()->DrawAABB(
			physicsTestBullet_->GetCollider()->GetAABB(),
			physicsTestBullet_->IsAlive() ? K4E::Vector4{ 1.0f, 0.9f, 0.15f, 1.0f } : K4E::Vector4{ 0.5f, 0.5f, 0.5f, 1.0f });
	}
#endif
}

void GamePlayWorld::DrawGameplayPhysicsTriggerTestImGui()
{
#ifdef USE_IMGUI
	ImGui::SeparatorText("Gameplay Physics Trigger Test");

	bool enableTriggerTest = enableGameplayPhysicsTriggerTest_;
	if (ImGui::Checkbox("Enable Trigger Test", &enableTriggerTest))
	{
		SetGameplayPhysicsTriggerTestEnabled(enableTriggerTest);
	}
	if (ImGui::Button("Spawn / Reset Test Bullet"))
	{
		RegisterGameplayPhysicsTriggerTest();
		enableGameplayPhysicsTriggerTest_ = true;
		ResetGameplayPhysicsTriggerTest();
	}

	const K4E::CollisionResponseType response =
		gameplayPhysicsWorld_.GetResponseMatrix().GetResponse(kPhysicsLayerTestBullet, kPhysicsLayerTestTarget);
	const K4E::Vector3 bulletPosition = physicsTestBullet_ ? physicsTestBullet_->GetPosition() : K4E::Vector3{};

	ImGui::Text("Bullet Alive: %s", physicsTestBullet_ && physicsTestBullet_->IsAlive() ? "true" : "false");
	ImGui::Text("Trigger Enter Count: %d", gameplayPhysicsEventHandler_ ? gameplayPhysicsEventHandler_->GetTriggerEnterCount() : 0);
	ImGui::Text("Latest Trigger Event: %s", gameplayPhysicsEventHandler_ ? gameplayPhysicsEventHandler_->GetLatestTriggerEvent().c_str() : "None");
	ImGui::Text("Bullet Position: %.3f, %.3f, %.3f", bulletPosition.x, bulletPosition.y, bulletPosition.z);
	ImGui::Text("Target Position: %.3f, %.3f, %.3f", physicsTriggerTargetPosition_.x, physicsTriggerTargetPosition_.y, physicsTriggerTargetPosition_.z);
	ImGui::Text("Response Type: %s", ToCollisionResponseName(response));
	ImGui::Text("Trigger Test Registered: %s", gameplayPhysicsTriggerTestRegistered_ ? "true" : "false");
#endif
}

void GamePlayWorld::SyncGameplayPhysicsTriggerTarget()
{
	// TriggerEvent確認用ターゲットの表示位置とPhysicsWorld Colliderを同期する。
	physicsTriggerTargetCollider_.SetEnabled(enableGameplayPhysicsTriggerTest_);
	physicsTriggerTargetCollider_.SetAABB({
		physicsTriggerTargetPosition_ - physicsTriggerTargetHalfSize_,
		physicsTriggerTargetPosition_ + physicsTriggerTargetHalfSize_,
		});

	if (physicsTriggerTargetObject_)
	{
		physicsTriggerTargetObject_->SetTranslate(physicsTriggerTargetPosition_);
		physicsTriggerTargetObject_->Update();
	}
}

void GamePlayWorld::SyncGameplayPhysicsBulletTriggerTargets()
{
	if (!usePhysicsForBulletTrigger_)
	{
		UnregisterGameplayPhysicsBulletTriggerTargets();
		return;
	}

	RegisterGameplayPhysicsEventListener();
	BindGameplayPhysicsStageColliders();

	std::vector<K4E::Collider*> desiredTargets{};
	const std::vector<EnemyBase*> enemies = characters_.GetEnemyRawList();
	desiredTargets.reserve(enemies.size() + 1);
	for (EnemyBase* enemy : enemies)
	{
		if (!enemy || enemy->IsDead() || enemy->IsRemovable())
		{
			continue;
		}

		enemy->SetCollisionLayer(kPhysicsLayerEnemy);
		desiredTargets.push_back(enemy);
	}
	if (auto* boss = bossBattleController_.GetBoss(); boss && bossBattleController_.IsColliderRegistered() && boss->IsAlive())
	{
		boss->SetCollisionLayer(kPhysicsLayerBoss);
		desiredTargets.push_back(boss);
	}

	for (K4E::Collider* registered : physicsBulletTargetColliders_)
	{
		if (std::find(desiredTargets.begin(), desiredTargets.end(), registered) == desiredTargets.end())
		{
			// 破棄済みEnemy/Boss Collider参照を防ぐため、不要になった実Bullet Trigger対象を解除する。
			gameplayPhysicsWorld_.UnregisterCollider(registered);
		}
	}
	for (K4E::Collider* target : desiredTargets)
	{
		gameplayPhysicsWorld_.RegisterCollider(target);
	}
	physicsBulletTargetColliders_ = std::move(desiredTargets);
}

void GamePlayWorld::UnregisterGameplayPhysicsBulletTriggerTargets()
{
	for (K4E::Collider* collider : physicsBulletTargetColliders_)
	{
		// 破棄済みEnemy/Boss Collider参照を防ぐため、PhysicsWorldからTarget登録を解除する。
		gameplayPhysicsWorld_.UnregisterCollider(collider);
	}
	physicsBulletTargetColliders_.clear();
	if (bulletManager_)
	{
		bulletManager_->SetUsePhysicsTriggerForNormalBullets(false);
	}
	if (!enableGameplayPhysicsTriggerTest_)
	{
		UnregisterGameplayPhysicsEventListener();
	}
}

void GamePlayWorld::UpdateGameplayPhysicsTest(float deltaTime)
{
	// 明示ONの確認機能がある場合だけ、本編とは別PhysicsWorldでテスト物体/Player床判定を更新する。
	if (!enableGameplayPhysicsTest_ && !enablePlayerPhysicsGroundCheck_ && !enablePlayerPhysicsDepenetration_ && !enableGameplayPhysicsTriggerTest_ && !usePhysicsForBulletTrigger_)
	{
		return;
	}

	if (enableGameplayPhysicsTest_)
	{
		physicsTestPosition_ += physicsTestRigidbody_.GetVelocity() * deltaTime;
		SyncGameplayPhysicsTestCollider();
	}
	UpdateGameplayPhysicsTriggerTest(deltaTime);
	SyncGameplayPhysicsBulletTriggerTargets();
	UpdatePlayerPhysicsGroundCheck();

	gameplayPhysicsWorld_.Update(deltaTime);

	if (enableGameplayPhysicsTest_)
	{
		physicsTestPosition_ = physicsTestCollider_.GetCenterPosition();

		if (physicsTestObject_)
		{
			physicsTestObject_->SetTranslate(physicsTestPosition_);
			physicsTestObject_->Update();
		}
	}

	playerPhysicsGrounded_ = EvaluatePlayerPhysicsGrounded();
	playerGroundRigidbody_.SetGrounded(playerPhysicsGrounded_);
	if (auto* player = characters_.GetPlayer())
	{
		if (enablePlayerPhysicsDepenetration_)
		{
			ApplyPlayerPhysicsCorrection(*player);
		}
		// PhysicsWorld側の接地状態をPlayerへ反映する。既存の移動/ジャンプ判定にはまだ使わない。
		player->SetGroundedByPhysics(enablePlayerPhysicsGroundCheck_ && playerPhysicsGrounded_);
	}
}

void GamePlayWorld::DrawGameplayPhysicsTest()
{
#ifdef _DEBUG
	// テスト有効時だけ本編ステージ上のPhysicsTestObjectとColliderを可視化する。
	if (!enableGameplayPhysicsTest_ && !enableGameplayPhysicsTriggerTest_ && !enableGameplayPhysicsDebugDraw_)
	{
		return;
	}

	if (enableGameplayPhysicsTest_ && physicsTestObject_)
	{
		physicsTestObject_->Draw();
	}

	if (enableGameplayPhysicsTest_)
	{
		K4E::Wireframe::GetInstance()->DrawAABB(
			physicsTestCollider_.GetAABB(),
			physicsTestRigidbody_.IsGrounded() ? K4E::Vector4{ 0.1f, 1.0f, 0.2f, 1.0f } : K4E::Vector4{ 0.2f, 0.9f, 1.0f, 1.0f });
	}
	DrawGameplayPhysicsTriggerTest();
	if (enableGameplayPhysicsDebugDraw_)
	{
		// Gameplay側でも共通Debug描画を使い、Player床判定/押し戻し/TriggerEventの調査に使う。
		gameplayPhysicsDebugDraw_.Draw(gameplayPhysicsWorld_);
	}
#endif
}

void GamePlayWorld::DrawGameplayPhysicsTestImGui()
{
#ifdef USE_IMGUI
	ImGui::SeparatorText("Gameplay Physics Test");

	bool enable = enableGameplayPhysicsTest_;
	if (ImGui::Checkbox("Enable Gameplay Physics Test", &enable))
	{
		enableGameplayPhysicsTest_ = enable;
		usePhysicsForPlayerStage_ = enableGameplayPhysicsTest_ || enablePlayerPhysicsGroundCheck_ || enablePlayerPhysicsDepenetration_;
		if (enableGameplayPhysicsTest_)
		{
			BindGameplayPhysicsStageColliders();
			ResetGameplayPhysicsTestObject();
		}
		else
		{
			if (!enablePlayerPhysicsGroundCheck_ && !enablePlayerPhysicsDepenetration_)
			{
				UnbindGameplayPhysicsStageColliders();
			}
		}
		UpdateCollisionSystemPolicyFromGameplayFlags();
	}
	bool enablePlayerGroundCheck = enablePlayerPhysicsGroundCheck_;
	if (ImGui::Checkbox("Enable Player Physics Ground Check", &enablePlayerGroundCheck))
	{
		enablePlayerPhysicsGroundCheck_ = enablePlayerGroundCheck;
		usePhysicsForPlayerGround_ = enablePlayerPhysicsGroundCheck_;
		usePhysicsForPlayerStage_ = enableGameplayPhysicsTest_ || enablePlayerPhysicsGroundCheck_ || enablePlayerPhysicsDepenetration_;
		if (enablePlayerPhysicsGroundCheck_)
		{
			BindGameplayPhysicsStageColliders();
			RegisterPlayerPhysicsGroundCheck();
		}
		else
		{
			if (!enablePlayerPhysicsDepenetration_)
			{
				UnregisterPlayerPhysicsGroundCheck();
			}
			if (!enableGameplayPhysicsTest_ && !enablePlayerPhysicsDepenetration_)
			{
				UnbindGameplayPhysicsStageColliders();
			}
		}
		UpdateCollisionSystemPolicyFromGameplayFlags();
	}
	bool enablePlayerDepenetration = enablePlayerPhysicsDepenetration_;
	if (ImGui::Checkbox("Enable Player Physics Depenetration", &enablePlayerDepenetration))
	{
		enablePlayerPhysicsDepenetration_ = enablePlayerDepenetration;
		usePhysicsForPlayerDepenetration_ = enablePlayerPhysicsDepenetration_;
		usePhysicsForPlayerStage_ = enableGameplayPhysicsTest_ || enablePlayerPhysicsGroundCheck_ || enablePlayerPhysicsDepenetration_;
		if (enablePlayerPhysicsDepenetration_)
		{
			BindGameplayPhysicsStageColliders();
			RegisterPlayerPhysicsGroundCheck();
		}
		else
		{
			playerPhysicsCorrectionDelta_ = {};
			if (!enablePlayerPhysicsGroundCheck_)
			{
				UnregisterPlayerPhysicsGroundCheck();
			}
			if (!enableGameplayPhysicsTest_ && !enablePlayerPhysicsGroundCheck_)
			{
				UnbindGameplayPhysicsStageColliders();
			}
		}
		UpdateCollisionSystemPolicyFromGameplayFlags();
	}
	ImGui::Checkbox("Apply XZ Correction", &applyPlayerPhysicsCorrectionXZ_);
	ImGui::Checkbox("Apply Y Correction", &applyPlayerPhysicsCorrectionY_);
	if (ImGui::Checkbox("Use Physics For Normal Bullet Trigger", &usePhysicsForBulletTrigger_))
	{
		// PhysicsWorld移行済みBulletの二重処理を防ぐため、通常弾だけTriggerEvent経路へ切り替える。
		if (bulletManager_)
		{
			bulletManager_->SetUsePhysicsTriggerForNormalBullets(usePhysicsForBulletTrigger_);
		}
		if (usePhysicsForBulletTrigger_)
		{
			BindGameplayPhysicsStageColliders();
			RegisterGameplayPhysicsEventListener();
		}
		else
		{
			UnregisterGameplayPhysicsBulletTriggerTargets();
		}
		UpdateCollisionSystemPolicyFromGameplayFlags();
	}

	if (ImGui::Button("Bind Stage Colliders"))
	{
		BindGameplayPhysicsStageColliders();
	}
	ImGui::SameLine();
	if (ImGui::Button("Unbind Stage Colliders"))
	{
		UnbindGameplayPhysicsStageColliders();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset Physics Test Object"))
	{
		ResetGameplayPhysicsTestObject();
	}

	const K4E::Vector3 velocity = physicsTestRigidbody_.GetVelocity();
	Player* player = characters_.GetPlayer();
	const K4E::Vector3 playerPosition = player ? player->GetCenterPosition() : K4E::Vector3{};
	ImGui::Text("Stage Binder Bound: %s", gameplayStagePhysicsBinder_.IsBound() ? "true" : "false");
	ImGui::Text("Bound Stage Collider Count: %zu", gameplayStagePhysicsBinder_.GetBoundColliderCount());
	ImGui::Text("PhysicsWorld Collider Count: %zu", gameplayPhysicsWorld_.GetColliderCount());
	ImGui::Text("Contact Count: %zu", gameplayPhysicsWorld_.GetContacts().size());
	ImGui::Text("IsGrounded: %s", physicsTestRigidbody_.IsGrounded() ? "true" : "false");
	ImGui::Text("Position: %.3f, %.3f, %.3f", physicsTestPosition_.x, physicsTestPosition_.y, physicsTestPosition_.z);
	ImGui::Text("Velocity: %.3f, %.3f, %.3f", velocity.x, velocity.y, velocity.z);
	ImGui::SeparatorText("Player Physics Ground Check");
	ImGui::Text("Existing Grounded: %s", player ? (player->FSM_IsGrounded() ? "true" : "false") : "N/A");
	ImGui::Text("Physics Grounded: %s", player ? (player->IsGroundedByPhysics() ? "true" : "false") : "N/A");
	ImGui::Text("Player Position: %.3f, %.3f, %.3f", playerPosition.x, playerPosition.y, playerPosition.z);
	ImGui::Text("Player Position Before Physics: %.3f, %.3f, %.3f", playerPositionBeforePhysics_.x, playerPositionBeforePhysics_.y, playerPositionBeforePhysics_.z);
	ImGui::Text("Player Position After Physics: %.3f, %.3f, %.3f", playerPositionAfterPhysics_.x, playerPositionAfterPhysics_.y, playerPositionAfterPhysics_.z);
	ImGui::Text("Correction Delta: %.3f, %.3f, %.3f", playerPhysicsCorrectionDelta_.x, playerPhysicsCorrectionDelta_.y, playerPhysicsCorrectionDelta_.z);
	ImGui::Text("Player Collider Position: %.3f, %.3f, %.3f", playerGroundColliderPosition_.x, playerGroundColliderPosition_.y, playerGroundColliderPosition_.z);
	ImGui::Text("Player vs Stage Contact Count: %zu", playerStageContactCount_);
	ImGui::Text("Registered Player Collider: %s", playerGroundColliderRegistered_ ? "true" : "false");
	ImGui::SeparatorText("Gameplay Physics Bullet Trigger");
	ImGui::Text("Physics Trigger Bullet Count: %zu", bulletManager_ ? bulletManager_->GetPhysicsTriggerBulletCount() : 0);
	ImGui::Text("Physics Trigger Bullet Hit Count: %d", bulletManager_ ? bulletManager_->GetPhysicsTriggerHitCount() : 0);
	ImGui::Text("Handler Bullet Trigger Hit Count: %d", gameplayPhysicsEventHandler_ ? gameplayPhysicsEventHandler_->GetRealBulletTriggerHitCount() : 0);
	ImGui::Text("Last Bullet Trigger Hit: %s", gameplayPhysicsEventHandler_ ? gameplayPhysicsEventHandler_->GetLatestRealBulletTriggerHit().c_str() : "None");
	ImGui::Text("PlayerBullet vs Enemy Response: %s", ToCollisionResponseName(gameplayPhysicsWorld_.GetResponseMatrix().GetResponse(kPhysicsLayerPlayerBullet, kPhysicsLayerEnemy)));
	ImGui::Text("PlayerBullet vs Boss Response: %s", ToCollisionResponseName(gameplayPhysicsWorld_.GetResponseMatrix().GetResponse(kPhysicsLayerPlayerBullet, kPhysicsLayerBoss)));
	ImGui::SeparatorText("Gameplay Physics Debug");
	if (ImGui::Checkbox("Enable Gameplay Physics Debug Draw", &enableGameplayPhysicsDebugDraw_))
	{
		gameplayPhysicsDebugDraw_.GetSettings().drawPhysicsDebug = enableGameplayPhysicsDebugDraw_;
	}
	gameplayPhysicsDebugDraw_.DrawImGui(gameplayPhysicsWorld_);
	gameplayPhysicsParameterBridge_.DrawImGui();
	DrawCollisionSystemPolicyImGui();
	DrawGameplayPhysicsTriggerTestImGui();
#endif
}

void GamePlayWorld::SyncGameplayPhysicsTestCollider()
{
	// TestObjectの表示位置とPhysicsWorldで判定するAABBを同期する。
	physicsTestCollider_.SetAABB({
		physicsTestPosition_ - physicsTestHalfSize_,
		physicsTestPosition_ + physicsTestHalfSize_,
		});
}

void GamePlayWorld::BindGameplayPhysicsStageColliders()
{
	// StageCollisionBuilder由来のStageColliderを、テスト用PhysicsWorldへStatic Colliderとして登録する。
	if (!stage_)
	{
		return;
	}

	std::vector<K4E::Collider*> stageColliders = stage_->GetWorldColliderPointers();
	gameplayStagePhysicsBinder_.Bind(gameplayPhysicsWorld_, stageColliders);
	gameplayPhysicsStageBound_ = gameplayStagePhysicsBinder_.IsBound();
}

void GamePlayWorld::UnbindGameplayPhysicsStageColliders()
{
	// Unbind後にStageとのContactが消えることを確認できるよう、Binder経由の登録を解除する。
	gameplayStagePhysicsBinder_.Unbind();
	gameplayPhysicsStageBound_ = false;
}

void GamePlayWorld::RegisterPlayerPhysicsGroundCheck()
{
	// Player床判定用ColliderをPhysicsWorldへ登録する。二重登録を避け、ON時だけ参照を持たせる。
	if (playerGroundColliderRegistered_)
	{
		return;
	}

	gameplayPhysicsWorld_.RegisterRigidbody(&playerGroundRigidbody_);
	gameplayPhysicsWorld_.RegisterCollider(&playerGroundCollider_);
	playerGroundColliderRegistered_ = true;
}

void GamePlayWorld::UnregisterPlayerPhysicsGroundCheck()
{
	// Scene終了時に破棄済みCollider参照を残さないよう、Player床判定用Colliderを解除する。
	if (!playerGroundColliderRegistered_)
	{
		return;
	}

	gameplayPhysicsWorld_.UnregisterCollider(&playerGroundCollider_);
	gameplayPhysicsWorld_.UnregisterRigidbody(&playerGroundRigidbody_);
	playerGroundColliderRegistered_ = false;
	playerStageContactCount_ = 0;
	playerPhysicsGrounded_ = false;
	playerGroundRigidbody_.SetGrounded(false);
	if (auto* player = characters_.GetPlayer())
	{
		player->SetGroundedByPhysics(false);
	}
}

void GamePlayWorld::UpdatePlayerPhysicsGroundCheck()
{
	// Player移動は置き換えず、床判定用Colliderだけを現在のPlayer位置へ同期する。
	if (!enablePlayerPhysicsGroundCheck_ && !enablePlayerPhysicsDepenetration_)
	{
		return;
	}

	if (!playerGroundColliderRegistered_)
	{
		RegisterPlayerPhysicsGroundCheck();
	}

	if (Player* player = characters_.GetPlayer())
	{
		SyncPlayerPhysicsGroundCollider(*player);
	}
}

void GamePlayWorld::SyncPlayerPhysicsGroundCollider(Player& player)
{
	// 既存のPlayer移動結果をPhysicsWorldへ渡し、めり込み補正だけを受け取る。
	playerPositionBeforePhysics_ = player.GetCenterPosition();
	playerPositionAfterPhysics_ = playerPositionBeforePhysics_;
	playerPhysicsCorrectionDelta_ = {};
	playerGroundColliderPosition_ = playerPositionBeforePhysics_ + playerGroundColliderOffset_;
	playerGroundCollider_.SetAABB({
		playerGroundColliderPosition_ - playerGroundColliderHalfSize_,
		playerGroundColliderPosition_ + playerGroundColliderHalfSize_,
		});
}

void GamePlayWorld::ApplyPlayerPhysicsCorrection(Player& player)
{
	// PhysicsWorldで補正された位置をPlayerへ戻し、壁へのめり込みを解消する。
	const K4E::Vector3 correctedColliderPosition = playerGroundCollider_.GetCenterPosition();
	K4E::Vector3 rawDelta = correctedColliderPosition - playerGroundColliderPosition_;
	const float maxCorrectionPerFrame = std::max(playerCorrectionClamp_, 0.0f);
	rawDelta.x = std::clamp(rawDelta.x, -maxCorrectionPerFrame, maxCorrectionPerFrame);
	rawDelta.y = std::clamp(rawDelta.y, -maxCorrectionPerFrame, maxCorrectionPerFrame);
	rawDelta.z = std::clamp(rawDelta.z, -maxCorrectionPerFrame, maxCorrectionPerFrame);

	K4E::Vector3 appliedDelta{};
	if (applyPlayerPhysicsCorrectionXZ_)
	{
		appliedDelta.x = rawDelta.x;
		appliedDelta.z = rawDelta.z;
	}
	if (applyPlayerPhysicsCorrectionY_)
	{
		appliedDelta.y = rawDelta.y;
	}

	playerPhysicsCorrectionDelta_ = appliedDelta;
	playerPositionAfterPhysics_ = playerPositionBeforePhysics_ + appliedDelta;
	if (K4E::Vector3::LengthSquared(appliedDelta) > 0.0f)
	{
		player.ApplyPhysicsCorrectedPosition(playerPositionAfterPhysics_);
		SyncPlayerPhysicsGroundCollider(player);
	}
}

bool GamePlayWorld::EvaluatePlayerPhysicsGrounded()
{
	// PhysicsWorldのContact normalから、Player ColliderがStage上面に接しているかだけを評価する。
	playerStageContactCount_ = 0;
	bool grounded = false;
	constexpr float kGroundNormalThreshold = 0.5f;
	for (const K4E::Contact& contact : gameplayPhysicsWorld_.GetContacts())
	{
		const bool playerIsA = contact.colliderA == &playerGroundCollider_;
		const bool playerIsB = contact.colliderB == &playerGroundCollider_;
		if (!playerIsA && !playerIsB)
		{
			continue;
		}

		++playerStageContactCount_;
		if ((playerIsA && contact.normal.y < -kGroundNormalThreshold) ||
			(playerIsB && contact.normal.y > kGroundNormalThreshold))
		{
			grounded = true;
		}
	}

	return grounded;
}

void GamePlayWorld::ApplyGameplayPhysicsParameterSettings()
{
	// JSON/ImGuiで調整した値をGameplay側PhysicsWorld/DebugDraw/テスト機能フラグへ反映する。
	gameplayPhysicsParameterBridge_.ApplyTo(gameplayPhysicsWorld_);
	gameplayPhysicsParameterBridge_.ApplyTo(gameplayPhysicsDebugDraw_);

	const K4E::GameplayPhysicsSettings settings = gameplayPhysicsParameterBridge_.GetGameplaySettings();
	const bool previousGameplayPhysicsTest = enableGameplayPhysicsTest_;
	const bool previousGroundCheck = enablePlayerPhysicsGroundCheck_;
	const bool previousDepenetration = enablePlayerPhysicsDepenetration_;
	const bool previousTriggerTest = enableGameplayPhysicsTriggerTest_;

	enableGameplayPhysicsTest_ = settings.enableGameplayPhysicsTest;
	enablePlayerPhysicsGroundCheck_ = settings.enablePlayerPhysicsGroundCheck;
	enablePlayerPhysicsDepenetration_ = settings.enablePlayerPhysicsDepenetration;
	applyPlayerPhysicsCorrectionXZ_ = settings.applyPlayerPhysicsCorrectionXZ;
	applyPlayerPhysicsCorrectionY_ = settings.applyPlayerPhysicsCorrectionY;
	playerCorrectionClamp_ = settings.playerCorrectionClamp;
	enableGameplayPhysicsTriggerTest_ = settings.enableGameplayPhysicsTriggerTest;
	enableGameplayPhysicsDebugDraw_ = gameplayPhysicsDebugDraw_.GetSettings().drawPhysicsDebug;
	usePhysicsForPlayerStage_ = settings.usePhysicsForPlayerStage || settings.enableGameplayPhysicsTest || settings.enablePlayerPhysicsGroundCheck || settings.enablePlayerPhysicsDepenetration;
	usePhysicsForPlayerGround_ = settings.usePhysicsForPlayerGround || settings.enablePlayerPhysicsGroundCheck;
	usePhysicsForPlayerDepenetration_ = settings.usePhysicsForPlayerDepenetration || settings.enablePlayerPhysicsDepenetration;
	usePhysicsForTriggerTest_ = settings.usePhysicsForTriggerTest || settings.enableGameplayPhysicsTriggerTest;
	usePhysicsForBulletTrigger_ = settings.usePhysicsForBulletTrigger;
	usePhysicsForEnemyStage_ = settings.usePhysicsForEnemyStage;
	if (bulletManager_)
	{
		bulletManager_->SetPhysicsTriggerWorld(&gameplayPhysicsWorld_, kPhysicsLayerPlayerBullet);
		bulletManager_->SetUsePhysicsTriggerForNormalBullets(usePhysicsForBulletTrigger_);
	}
	UpdateCollisionSystemPolicyFromGameplayFlags();

	if (enableGameplayPhysicsTest_ && !previousGameplayPhysicsTest)
	{
		BindGameplayPhysicsStageColliders();
		ResetGameplayPhysicsTestObject();
	}
	if ((enablePlayerPhysicsGroundCheck_ || enablePlayerPhysicsDepenetration_) && (!previousGroundCheck && !previousDepenetration))
	{
		BindGameplayPhysicsStageColliders();
		RegisterPlayerPhysicsGroundCheck();
	}
	if (enableGameplayPhysicsTriggerTest_ != previousTriggerTest)
	{
		SetGameplayPhysicsTriggerTestEnabled(enableGameplayPhysicsTriggerTest_);
	}
	if (usePhysicsForBulletTrigger_)
	{
		BindGameplayPhysicsStageColliders();
		RegisterGameplayPhysicsEventListener();
	}
	else
	{
		UnregisterGameplayPhysicsBulletTriggerTargets();
	}
	if (!enableGameplayPhysicsTest_ && !enablePlayerPhysicsGroundCheck_ && !enablePlayerPhysicsDepenetration_)
	{
		UnregisterPlayerPhysicsGroundCheck();
		if (!enableGameplayPhysicsTriggerTest_ && !usePhysicsForBulletTrigger_)
		{
			UnbindGameplayPhysicsStageColliders();
		}
	}
}

void GamePlayWorld::UpdateCollisionSystemPolicyFromGameplayFlags()
{
	// 段階移行中に旧判定と新Physics判定を切り替えるため、現在のGameplay Physicsフラグを担当表へ反映する。
	collisionSystemPolicy_.SetOwner(
		K4E::CollisionSystemPair::PlayerStage,
		usePhysicsForPlayerStage_ || usePhysicsForPlayerGround_ || usePhysicsForPlayerDepenetration_
		? K4E::CollisionSystemOwner::PhysicsWorld
		: K4E::CollisionSystemOwner::LegacyCollisionManager);
	collisionSystemPolicy_.SetOwner(
		K4E::CollisionSystemPair::TriggerTest,
		usePhysicsForTriggerTest_ || enableGameplayPhysicsTriggerTest_
		? K4E::CollisionSystemOwner::PhysicsWorld
		: K4E::CollisionSystemOwner::Disabled);
	collisionSystemPolicy_.SetOwner(
		K4E::CollisionSystemPair::PhysicsTestObjectStage,
		enableGameplayPhysicsTest_
		? K4E::CollisionSystemOwner::PhysicsWorld
		: K4E::CollisionSystemOwner::Disabled);
	collisionSystemPolicy_.SetOwner(
		K4E::CollisionSystemPair::PlayerBulletEnemy,
		usePhysicsForBulletTrigger_
		? K4E::CollisionSystemOwner::PhysicsWorld
		: K4E::CollisionSystemOwner::LegacyCollisionManager);
	collisionSystemPolicy_.SetOwner(
		K4E::CollisionSystemPair::PlayerBulletBoss,
		usePhysicsForBulletTrigger_
		? K4E::CollisionSystemOwner::PhysicsWorld
		: K4E::CollisionSystemOwner::LegacyCollisionManager);
	collisionSystemPolicy_.SetOwner(
		K4E::CollisionSystemPair::EnemyStage,
		usePhysicsForEnemyStage_
		? K4E::CollisionSystemOwner::PhysicsWorld
		: K4E::CollisionSystemOwner::LegacyCollisionManager);
}

void GamePlayWorld::DrawCollisionSystemPolicyImGui()
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("Collision System Policy"))
	{
		return;
	}

	// Debug表示で、既存CollisionManagerとPhysicsWorldの責任範囲を一覧確認する。
	ImGui::Text("PhysicsWorld Collider Count: %zu", gameplayPhysicsWorld_.GetColliderCount());
	ImGui::Text("Legacy CollisionManager Collider Count: %zu", collisionManager_ ? collisionManager_->GetColliderCount() : 0);
	ImGui::Text("Player vs Stage Owner: %s", K4E::CollisionSystemPolicy::ToString(collisionSystemPolicy_.GetOwner(K4E::CollisionSystemPair::PlayerStage)));
	ImGui::Text("Bullet vs Enemy Owner: %s", K4E::CollisionSystemPolicy::ToString(collisionSystemPolicy_.GetOwner(K4E::CollisionSystemPair::PlayerBulletEnemy)));
	ImGui::Text("BossAttack vs Player Owner: %s", K4E::CollisionSystemPolicy::ToString(collisionSystemPolicy_.GetOwner(K4E::CollisionSystemPair::BossAttackPlayer)));
	ImGui::Text("Trigger Test Owner: %s", K4E::CollisionSystemPolicy::ToString(collisionSystemPolicy_.GetOwner(K4E::CollisionSystemPair::TriggerTest)));

	if (collisionSystemPolicy_.GetOwner(K4E::CollisionSystemPair::PlayerStage) == K4E::CollisionSystemOwner::PhysicsWorld)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "Player vs Stage: existing movement remains active. Keep Physics correction flags explicit.");
	}
	if (collisionSystemPolicy_.GetOwner(K4E::CollisionSystemPair::PlayerBulletEnemy) == K4E::CollisionSystemOwner::PhysicsWorld ||
		collisionSystemPolicy_.GetOwner(K4E::CollisionSystemPair::PlayerBulletBoss) == K4E::CollisionSystemOwner::PhysicsWorld)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "Bullet Trigger migration TODO: disable legacy damage before enabling real PhysicsWorld bullet hits.");
	}

	if (ImGui::TreeNode("Policy Table"))
	{
		for (const K4E::CollisionSystemRule& rule : collisionSystemPolicy_.GetRules())
		{
			ImGui::Text("%s | %s | %s",
				rule.pairName,
				K4E::CollisionSystemPolicy::ToString(rule.owner),
				rule.migrationStatus);
			if (rule.doubleProcessingRisk)
			{
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.25f, 1.0f), "Double-check");
			}
			ImGui::TextDisabled("%s", rule.note);
		}
		ImGui::TreePop();
	}
#endif
}

void GamePlayWorld::UpdateShadowLightViewProjection()
{
	K4E::Vector3 center = { 0.0f, 0.0f, 0.0f };
	if (auto* player = characters_.GetPlayer())
	{
		if (auto* wt = player->GetWorldTransform())
		{
			center = wt->translate_;
		}
	}

	shadowLightViewProjection_ = K4E::LightManager::GetInstance()->BuildShadowLightViewProjection(center);
}

bool GamePlayWorld::TryGetDirectionalLightFromManager(K4E::Vector3& outDirection) const
{
	const auto& lights = K4E::LightManager::GetInstance()->GetPunctualLights();

	for (const auto& L : lights)
	{
		if (L.lightType == 1)
		{
			outDirection = K4E::Vector3::Normalize(L.direction);
			return true;
		}
	}

	return false;
}

bool GamePlayWorld::IsSightBlocked(const K4E::Segment& seg) const
{
	(void)seg; // 未使用
	return false;
}
