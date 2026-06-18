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

	gameplayPhysicsDebugController_.Initialize(BuildGameplayPhysicsDebugDependencies());

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
	ammoRecoveryItemSpawner_.Initialize();
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
	ammoRecoveryItemSpawner_.Reset();
	itemManager_.Clear();

	gameplayPhysicsDebugController_.Finalize();
	characters_.Finalize();

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
		const bool suppressAmmoRecoverySpawn =
			stage1TutorialController_.IsGameplayBlocked() ||
			bossBattleController_.IsIntroGameplayPaused() ||
			bossBattleController_.IsGameClearRequested();
		// 弾薬回復Itemの時間スポーンは専用クラスに任せ、World側は進行状態だけを渡す。
		ammoRecoveryItemSpawner_.Update(deltaTime, player, itemManager_, stage_.get(), suppressAmmoRecoverySpawn);
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

	gameplayPhysicsDebugController_.Update(BuildGameplayPhysicsDebugDependencies(), deltaTime);

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
	gameplayPhysicsDebugController_.Draw();

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
	gameplayPhysicsDebugController_.DrawImGui(BuildGameplayPhysicsDebugDependencies());
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
	ammoRecoveryItemSpawner_.DrawImGui();

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

void GamePlayWorld::SetDebugCameraEnabled(bool enabled)
{
	// GamePlay Debugのカメラ切替状態を、Worldが所有・参照する描画系へまとめて同期する。
	if (auto* player = characters_.GetPlayer())
	{
		player->SetDebugCamera(enabled);
	}
	if (skyBox_)
	{
		skyBox_->SetDebugCamera(enabled);
	}
	K4E::Wireframe::GetInstance()->SetDebugCamera(enabled);
	if (auto* particleManager = K4E::ParticleManager::GetInstance())
	{
		particleManager->SetDebugCamera(enabled);
	}
	if (auto* gpuParticleManager = K4E::GpuParticleManager::GetInstance())
	{
		gpuParticleManager->SetDebugCameraEnabled(enabled);
	}
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


GameplayPhysicsDebugController::Dependencies GamePlayWorld::BuildGameplayPhysicsDebugDependencies()
{
	GameplayPhysicsDebugController::Dependencies deps{};
	deps.characters = &characters_;
	deps.bulletManager = bulletManager_.get();
	deps.collisionManager = collisionManager_.get();
	deps.stage = stage_.get();
	deps.getBoss = [this]() { return bossBattleController_.GetBoss(); };
	deps.isBossColliderRegistered = [this]() { return bossBattleController_.IsColliderRegistered(); };
	return deps;
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
