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
#include "EnemyBase.h"
#include "GpuParticleManager.h"
#include "ParticleManager.h"
#include "CollisionPreset.h"
#include "CollisionTypeIdDef.h"
#include <LogString.h>

#include <cassert>
#include <chrono>
#include <cmath>
#include <vector>

#ifdef USE_IMGUI
#include <imgui.h>
#include <GameTimer.h>
#endif

using namespace Ken4lowEngine;


void BossClearItem::Initialize(const K4E::Vector3& position)
{
	K4E::Vector3 spawnPosition = position;
	spawnPosition.y += 1.25f;

	position_ = spawnPosition;
	basePosition_ = spawnPosition;
	rotation_ = {};
	floatTimer_ = 0.0f;
	spawned_ = true;
	collected_ = false;

	ApplyCollisionPreset(*this, ECollisionPresetId::Item); // BossClearItemは通常Itemと同じkItem判定を保つPreset移行対象にする。
#ifdef _DEBUG
	const uint32_t legacyItemTypeId = static_cast<uint32_t>(CollisionTypeIdDef::kItem);
	assert(GetTypeID() == legacyItemTypeId && "BossClearItem preset must keep legacy kItem TypeID.");
#endif
	SetOwner<BossClearItem>(this);
	SetOBBHalfSize(halfSize_);
	SetCenterPosition(position_);

	object3d_ = std::make_unique<K4E::Object3D>();
	object3d_->Initialize("Test/cube.gltf");
	object3d_->SetScale({ 1.8f, 1.8f, 1.8f });
	object3d_->SetTranslate(position_);
	object3d_->SetColor({ 1.0f, 0.85f, 0.05f, 1.0f });
	object3d_->Update();
}

void BossClearItem::Update(float deltaTime)
{
	if (!spawned_ || collected_)
	{
		return;
	}

	floatTimer_ += deltaTime * 3.0f;
	position_ = basePosition_;
	position_.y += std::sinf(floatTimer_) * 0.25f;
	rotation_.y += deltaTime * 1.2f;

	SetCenterPosition(position_);
	SetOrientation(rotation_);

	if (object3d_)
	{
		object3d_->SetTranslate(position_);
		object3d_->SetRotate(rotation_);
		object3d_->Update();
	}
}

void BossClearItem::Draw()
{
	if (spawned_ && !collected_ && object3d_)
	{
		object3d_->Draw();
	}
}

bool BossClearItem::CheckPickup(const Player& player) const
{
	if (!spawned_ || collected_)
	{
		return false;
	}

	const K4E::Vector3 diff = position_ - player.GetCenterPosition();
	return K4E::Vector3::Length(diff) <= pickupRadius_;
}

void BossClearItem::MarkCollected()
{
	collected_ = true;
	SetCenterPosition({ 1.0e9f, 1.0e9f, 1.0e9f });
}

void BossClearItem::OnCollision(K4E::Collider* other)
{
	(void)other;
}

void GamePlayWorld::Initialize(GamePlayStageContext& stageContext)
{
	const auto stageAssets = stageContext.GetCurrentStageAssets();
	auto* lightManager = LightManager::GetInstance();

	// GamePlayScene開始時は保存済みプリセットを優先し、なければ確認用ライトへ戻す。
	lightManager->ResetToDefaultLighting();
	lightManager->SetShadowCasterLightIndex(-1);
	lightManager->SetManualShadowFocusPosition({ 0.0f, 0.0f, 0.0f });

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

	collisionManager_ = std::make_unique<CollisionManager>();
	collisionManager_->Initialize();

	// 弾、キャラクター、アイテム、ステージは同じCollisionManagerへ登録し、
	// World更新の最後にまとめて衝突解決できるようにする。
	bulletManager_ = std::make_unique<BulletManager>();
	bulletManager_->Initialize(collisionManager_.get());

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

	hudManager_ = std::make_unique<HUDManager>();
	hudManager_->SetPlayer(characters_.GetPlayer());
	hudManager_->Initialize();

	if (auto* player = characters_.GetPlayer())
	{
		player->SetHUDManager(hudManager_.get());
	}

	stage_ = std::make_unique<K4E::Stage>();
	stage_->Initialize(stageAssets.jsonPath, stageAssets.modelPath);
	stage_->RegisterColliders(collisionManager_.get());
	stage_->Update();

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

	stageContext.LoadSpawnPointsFromLevel(stageAssets.jsonPath);

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
	bossSpawnPosition_ = stageContext.HasBossSpawnPoint() ? stageContext.GetBossSpawnPoint() : K4E::Vector3{ 0.0f, 2.25f, 30.0f };
	if (!stageContext.HasBossSpawnPoint())
	{
		// Blender側BossSpawnPointが未設定の間は、DebugSceneと同じ仮固定座標を使う。
		bossSpawnPosition_ = { 0.0f, 2.25f, 30.0f };
	}
	bossSpawned_ = false;
	bossColliderRegistered_ = false;
	bossSpawnConditionMet_ = false;
	bossDefeated_ = false;
	clearItemSpawned_ = false;
	clearItemCollected_ = false;
	isGameClear_ = false;
	clearItem_.reset();

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

	crystalManager_.Initialize(crystalSpawnPoints, collisionManager_.get(), &stage_->GetFloorAABBs(), &stage_->GetNavigationObstacleAABBs());
	crystalManager_.SetProgressDebugStatus(characters_.GetAliveNormalEnemyCount(), bossSpawnConditionMet_, bossSpawned_, bossSpawnPosition_);
	bossIntroController_.Initialize(bossSpawnPosition_);

	enemyHpBarManager_.Initialize();
}

void GamePlayWorld::Finalize()
{
	if (guardianBoss_ && collisionManager_)
	{
		collisionManager_->RemoveCollider(guardianBoss_.get());
	}
	if (clearItem_ && collisionManager_)
	{
		collisionManager_->RemoveCollider(clearItem_.get());
	}
	guardianBoss_.reset();
	clearItem_.reset();
	bossIntroController_.Finalize();
	crystalManager_.Finalize();
	stageObjectiveManager_.reset();
	waveManager_.reset();
	hudManager_.reset();

	EnemyBase::SetGlobalStageWorldAABBs(nullptr);
	EnemyBase::SetGlobalStageFloorAABBs(nullptr);
	EnemyBase::SetGlobalStageNavigationObstacleAABBs(nullptr);
	itemManager_.Clear();

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
	if (stage_)
	{
		stage_->Update();
	}

	if (bossIntroController_.IsGameplayPaused())
	{
		// ボス登場演出中は通常ゲーム進行を止め、カメラとボス登場Transformだけを進める。
		UpdateBossIntroPausedWorld(deltaTime);
		return;
	}

	// 先にキャラクターとクリスタルを更新し、敵残数とクリスタル破壊状態からボス出現条件を評価する。
	characters_.Update(deltaTime);
	crystalManager_.Update(characters_, deltaTime);
	UpdateCrystalBossSpawnProgress();
	UpdateBossIntro(deltaTime);
	if (guardianBoss_)
	{
		if (auto* player = characters_.GetPlayer())
		{
			guardianBoss_->SetTargetPosition(player->GetCenterPosition());
			guardianBoss_->SetTargetPlayer(player);
		}
		guardianBoss_->Update(deltaTime);
	}
	UpdateBossClearProgress(deltaTime);
	crystalManager_.SetProgressDebugStatus(characters_.GetAliveNormalEnemyCount(), bossSpawnConditionMet_, bossSpawned_, bossSpawnPosition_);
	if (auto* player = characters_.GetPlayer())
	{
		itemManager_.Update(player, deltaTime);
	}
	else
	{
		itemManager_.Update(deltaTime);
	}

	UpdateShadowLightViewProjection();

	if (stage_)
	{
		stage_->UpdateShadowMatrix(shadowLightViewProjection_);
	}
	characters_.UpdateShadowMatrix(shadowLightViewProjection_);
	if (guardianBoss_)
	{
		guardianBoss_->UpdateShadowMatrix(shadowLightViewProjection_);
	}

	if (bulletManager_)
	{
		// 弾更新と衝突更新はデバッグ表示用に処理時間を保存する。
		const auto begin = std::chrono::steady_clock::now();
		bulletManager_->Update(deltaTime);
		lastBulletUpdateMs_ = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - begin).count();
	}

	{
		const auto begin = std::chrono::steady_clock::now();
		CollisionUpdate();
		lastCollisionUpdateMs_ = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - begin).count();
	}

	if (skyBox_)
	{
		skyBox_->Update();
		skyBox_->AdvanceCloudLayer(deltaTime);
	}

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
				deltaTime
			);
		}
	}

	if (hudManager_ && characters_.GetPlayer())
	{
		// 照準先判定、HP、Wave状態などWorld側でしか分からない情報をHUDへ集約する。
		const bool isTargetingEnemy = CheckCrosshairTargetingEnemy();
		hudManager_->SetCrosshairTargetingEnemy(isTargetingEnemy);

		hudManager_->SetHP(
			characters_.GetPlayer()->GetHP(),
			characters_.GetPlayer()->GetMaxHP());
		hudManager_->Update(deltaTime);
	}

	if (stageObjectiveManager_ && stageObjectiveManager_->UsesWaveSystem() && waveManager_)
	{
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
	K4E::SkyBoxManager::GetInstance()->SetRenderSetting();
	if (skyBox_)
	{
		skyBox_->Draw();
		skyBox_->DrawCloudLayer();
	}

	if (!hideCharactersDuringIntro)
	{
		characters_.Draw();
		if (guardianBoss_)
		{
			// Draw直前に現在の通常ViewProjectionでWVPを更新し、演出用ViewProjectionの残留を防ぐ。
			guardianBoss_->ForceSyncWorldTransform();
			guardianBoss_->Draw();
		}
	}

	if (stage_)
	{
		stage_->Draw();
#ifdef _DEBUG
		stage_->DrawChunkDebug();
#endif
	}

	crystalManager_.Draw();

	if (bulletManager_)
	{
		bulletManager_->Draw();
	}

	itemManager_.Draw();
	if (clearItem_)
	{
		clearItem_->Draw();
	}

#ifdef _DEBUG
	if (collisionManager_)
	{
		collisionManager_->Draw();
	}

	K4E::Wireframe::GetInstance()->DrawGrid(
		200.0f,
		50.0f,
		{ 0.25f, 0.25f, 0.25f, 1.0f });
#endif
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

	if (guardianBoss_)
	{
		// ボス登場演出中に通常3D描画を止め、ボスだけを現在の演出用ViewProjectionへ同期する。
		guardianBoss_->ForceSyncWorldTransform();
		guardianBoss_->Draw();
	}
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
		if (guardianBoss_)
		{
			guardianBoss_->DrawShadow();
		}
	}
}

void GamePlayWorld::DrawBossIntroShadow()
{
	if (stage_)
	{
		stage_->DrawShadow();
	}

	if (guardianBoss_)
	{
		guardianBoss_->DrawShadow();
	}
}

void GamePlayWorld::DrawHUD(bool hideDuringIntro)
{
	if (hideDuringIntro) { return; }

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
	ImGui::SeparatorText("ボス状態");
	ImGui::Text("ボス出現済み: %s", bossSpawned_ ? "はい" : "いいえ");
	ImGui::Text("ボスCollider登録済み: %s", bossColliderRegistered_ ? "はい" : "いいえ");
	ImGui::Text("ボス登場演出中: %s", bossIntroController_.IsRunning() ? "はい" : "いいえ");
	ImGui::Text("ボス登場による進行停止: %s", bossIntroController_.IsGameplayPaused() ? "はい" : "いいえ");
	{
		auto* debugPlayer = characters_.GetPlayer();
		bossIntroController_.SetDebugSnapshot(guardianBoss_.get(), debugPlayer ? debugPlayer->GetCamera() : nullptr);
	}
	bossIntroController_.DrawImGui();
	ImGui::SeparatorText("Boss Intro Draw Debug");
	const bool bossIntroPresentationActive = IsBossIntroPresentationActive();
	ImGui::Text("isBossIntroActive: %s", bossIntroController_.IsRunning() ? "true" : "false");
	ImGui::Text("Camera Kind: %s", bossIntroPresentationActive ? "BossIntro Camera" : "Gameplay Camera");
	ImGui::Text("ViewProjection Kind: %s", bossIntroPresentationActive ? "BossIntro ViewProjection" : "Gameplay ViewProjection");
	ImGui::Text("Draw Gameplay 3D: %s", bossIntroPresentationActive ? "false" : "true");
	ImGui::Text("Draw Gameplay UI: %s", bossIntroPresentationActive ? "false" : "true");
	ImGui::Text("Draw BossIntro 3D: %s", bossIntroPresentationActive ? "true" : "false");
	ImGui::Text("Draw Gameplay Route: %s", bossIntroPresentationActive ? "false" : "true");
	if (guardianBoss_)
	{
		ImGui::Text("ボス生存中: %s", guardianBoss_->IsAlive() ? "はい" : "いいえ");
		ImGui::Text("ボスHP: %.1f", guardianBoss_->GetHP());
		ImGui::Text("ボス最大HP: %.1f", guardianBoss_->GetMaxHP());
		ImGui::Text("ボスHP割合: %.1f%%", guardianBoss_->GetHPRate() * 100.0f);
		ImGui::Text("近接攻撃ヒット回数: %d", guardianBoss_->GetMeleeHitCount());
		ImGui::Text("銃弾ヒット回数: %d", guardianBoss_->GetBulletHitCount());
		ImGui::Text("最後にボスへ与えたダメージ: %.1f", guardianBoss_->GetLastReceivedDamage());
		ImGui::Text("ボス攻撃ヒット回数: %d", guardianBoss_->GetBossAttackHitCount());
		ImGui::Text("最後にプレイヤーが受けたボスダメージ: %.1f", guardianBoss_->GetLastPlayerDamage());
	}
	else
	{
		ImGui::Text("ボス生存中: いいえ");
		ImGui::Text("ボスHP: 0.0");
		ImGui::Text("ボス最大HP: 0.0");
		ImGui::Text("ボスHP割合: 0.0%%");
		ImGui::Text("近接攻撃ヒット回数: 0");
		ImGui::Text("銃弾ヒット回数: 0");
		ImGui::Text("最後にボスへ与えたダメージ: 0.0");
		ImGui::Text("ボス攻撃ヒット回数: 0");
		ImGui::Text("最後にプレイヤーが受けたボスダメージ: 0.0");
	}
	if (auto* player = characters_.GetPlayer())
	{
		ImGui::Text("Player HP: %.1f / %.1f", player->GetHP(), player->GetMaxHP());
	}
	else
	{
		ImGui::Text("Player HP: 0.0 / 0.0");
	}

	ImGui::SeparatorText("クリアCube状態");
	ImGui::Text("クリアCube出現済み: %s", clearItemSpawned_ ? "はい" : "いいえ");
	ImGui::Text("クリアCube取得済み: %s", clearItemCollected_ ? "はい" : "いいえ");
	const K4E::Vector3 clearPos = clearItem_ ? clearItem_->GetPosition() : K4E::Vector3{};
	ImGui::Text("クリアCube座標: %.2f, %.2f, %.2f", clearPos.x, clearPos.y, clearPos.z);
	ImGui::Text("ゲームクリア判定: %s", isGameClear_ ? "はい" : "いいえ");
	ImGui::Text("ボス撃破済み: %s", bossDefeated_ ? "はい" : "いいえ");
	if (guardianBoss_)
	{
		guardianBoss_->DrawImGui();
	}
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

void GamePlayWorld::UpdateCrystalBossSpawnProgress()
{
	bossSpawnConditionMet_ = crystalManager_.AreAllCrystalsDestroyed() && !bossSpawned_;

	// 全クリスタル破壊を検知し、即スポーンではなくボス登場遅延を開始する。
	if (bossSpawnConditionMet_ && !bossIntroController_.HasPlayed() && !bossIntroController_.IsRunning())
	{
		bossIntroController_.RequestStart(bossSpawnPosition_);
	}
}

void GamePlayWorld::UpdateBossIntro(float deltaTime)
{
	if (bossIntroController_.ConsumeDebugResetRequest())
	{
		ResetBossIntroForDebug();
	}

	if (bossIntroController_.ConsumeDebugStartRequest())
	{
		ResetBossIntroForDebug();
		bossIntroController_.RequestStart(bossIntroController_.GetBossAppearPosition());
	}

	if (bossIntroController_.ConsumeDebugClearBossParentRequest() && guardianBoss_)
	{
		// 検証用: 親子Transformが原因か切り分けるため、ワールド座標を維持して親を外す。
		guardianBoss_->ClearRootParentKeepingWorldPosition();
		guardianBoss_->ForceSyncWorldTransform();
	}

	if (bossIntroController_.ConsumeDebugForceBossToAppearRequest() && guardianBoss_)
	{
		guardianBoss_->ClearRootParentKeepingWorldPosition();
		// 検証用: ボスを最終ワールド座標へ固定して、カメラ追従に見える原因がTransformか確認する。
		guardianBoss_->SetPosition(bossIntroController_.GetBossAppearPosition());
		guardianBoss_->SetYaw(3.141592f);
		guardianBoss_->ForceSyncWorldTransform();
	}

	if (bossIntroController_.ConsumeDebugUseGameplayViewProjectionRequest())
	{
		if (auto* debugPlayer = characters_.GetPlayer())
		{
			if (auto* gameplayCamera = debugPlayer->GetCamera())
			{
				// 検証用: 演出用ViewProjectionから通常ViewProjectionへ戻して、描画行列側の問題を切り分ける。
				K4E::CameraManager::GetInstance()->SetMainCamera(gameplayCamera);
				gameplayCamera->Update();
			}
		}
		if (guardianBoss_)
		{
			guardianBoss_->ForceSyncWorldTransform();
		}
	}

	if (!bossIntroController_.IsRunning())
	{
		return;
	}

	auto* player = characters_.GetPlayer();
	auto* camera = player ? player->GetCamera() : nullptr;
	if (camera)
	{
		K4E::CameraManager::GetInstance()->SetMainCamera(camera);
	}
	bossIntroController_.Update(deltaTime, guardianBoss_.get(), camera);

	if (bossIntroController_.ConsumeBossSpawnRequest())
	{
		bossSpawnPosition_ = bossIntroController_.GetBossAppearPosition();
		SpawnGuardianBoss(false);
	}

	if (bossIntroController_.ConsumeBossColliderEnableRequest())
	{
		RegisterGuardianBossCollider();
		if (player)
		{
			player->SyncViewToPlayer();
			if (auto* resumedCamera = player->GetCamera())
			{
				// Completed後は演出用更新を止め、通常FPSカメラを描画用MainCameraへ戻す。
				K4E::CameraManager::GetInstance()->SetMainCamera(resumedCamera);
				resumedCamera->Update();
			}
		}
	}
}

void GamePlayWorld::UpdateBossIntroPausedWorld(float deltaTime)
{
	crystalManager_.UpdatePresentationOnly(characters_, deltaTime);
	UpdateBossIntro(deltaTime);
	if (stage_)
	{
		// 演出用カメラと通常カメラを切り替えた後、最低限表示するステージのWVPを現在カメラへ合わせる。
		stage_->Update();
	}
	UpdateBossClearProgress(0.0f);
	crystalManager_.SetProgressDebugStatus(characters_.GetAliveNormalEnemyCount(), bossSpawnConditionMet_, bossSpawned_, bossSpawnPosition_);

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
	if (guardianBoss_)
	{
		guardianBoss_->UpdateShadowMatrix(shadowLightViewProjection_);
	}
}

void GamePlayWorld::SpawnGuardianBoss(bool registerCollider)
{
	if (bossSpawned_)
	{
		return;
	}

	guardianBoss_ = std::make_unique<GuardianBoss>();
	guardianBoss_->Initialize();
	guardianBoss_->SetPosition(registerCollider ? bossSpawnPosition_ : bossIntroController_.GetBossStartPosition());
	guardianBoss_->SetYaw(3.141592f);
	if (auto* player = characters_.GetPlayer())
	{
		guardianBoss_->SetTargetPosition(player->GetCenterPosition());
		guardianBoss_->SetTargetPlayer(player);
	}
	guardianBoss_->Update(0.0f);
	bossSpawned_ = true;
	if (registerCollider)
	{
		RegisterGuardianBossCollider();
	}
}

void GamePlayWorld::RegisterGuardianBossCollider()
{
	if (!guardianBoss_ || !collisionManager_ || bossColliderRegistered_)
	{
		return;
	}

	guardianBoss_->ClearRootParentKeepingWorldPosition();
	guardianBoss_->SetPosition(bossIntroController_.GetBossAppearPosition());
	guardianBoss_->SetYaw(3.141592f);
	guardianBoss_->ForceSyncWorldTransform();
	// 登場完了後にボスAI/攻撃/当たり判定を有効化するため、このタイミングでCollider登録する。
	collisionManager_->AddCollider(guardianBoss_.get());
	bossColliderRegistered_ = true;
	Log("[GuardianBoss] Collider registered as kBoss.\n");
}

void GamePlayWorld::ResetBossIntroForDebug()
{
	if (guardianBoss_ && collisionManager_ && bossColliderRegistered_)
	{
		collisionManager_->RemoveCollider(guardianBoss_.get());
	}

	guardianBoss_.reset();
	bossSpawned_ = false;
	bossColliderRegistered_ = false;
	bossDefeated_ = false;
	bossSpawnConditionMet_ = false;
	bossIntroController_.Reset();
}

void GamePlayWorld::UpdateBossClearProgress(float deltaTime)
{
	if (guardianBoss_ && guardianBoss_->IsDead() && !bossDefeated_)
	{
		// ボス死亡だけでは即クリアにせず、取得アイテムを出すための中間状態にする。
		bossDefeated_ = true;
		SetBossDefeated(false);
	}

	if (bossDefeated_ && !clearItemSpawned_ && guardianBoss_)
	{
		// 撃破位置を基準にクリアアイテムを出し、プレイヤーが取りに行く余地を残す。
		SpawnClearItem(guardianBoss_->GetPosition());
	}

	if (clearItem_ && !clearItemCollected_)
	{
		clearItem_->Update(deltaTime);
		if (auto* player = characters_.GetPlayer())
		{
			if (clearItem_->CheckPickup(*player))
			{
				CollectClearItem();
			}
		}
	}
}

void GamePlayWorld::SpawnClearItem(const K4E::Vector3& bossPosition)
{
	if (clearItemSpawned_)
	{
		return;
	}

	K4E::Vector3 spawnPosition = bossPosition;
	spawnPosition.z -= 2.0f;
	spawnPosition.y = std::max(spawnPosition.y, 0.75f);

	clearItem_ = std::make_unique<BossClearItem>();
	clearItem_->Initialize(spawnPosition);
	if (collisionManager_)
	{
		collisionManager_->AddCollider(clearItem_.get());
	}

	clearItemSpawned_ = true;
	Log("[GameClear] BossClearItem spawned.\n");
}

void GamePlayWorld::CollectClearItem()
{
	if (clearItemCollected_ || isGameClear_)
	{
		return;
	}

	clearItemCollected_ = true;
	isGameClear_ = true;
	if (clearItem_)
	{
		clearItem_->MarkCollected();
		if (collisionManager_)
		{
			collisionManager_->RemoveCollider(clearItem_.get());
		}
	}

	// ボス撃破後に出現するクリアCubeを取得したらゲームクリアへ進める。
	SetBossDefeated(true);
	Log("[GameClear] Clear item collected.\n");
}

bool GamePlayWorld::IsAllWavesCleared() const
{
	return waveManager_ && waveManager_->IsAllWavesCleared();
}

bool GamePlayWorld::IsStageObjectiveCleared() const
{
	return isGameClear_ || (stageObjectiveManager_ && stageObjectiveManager_->IsStageObjectiveCleared(IsAllWavesCleared()));
}

bool GamePlayWorld::IsStageObjectiveFailed() const
{
	return stageObjectiveManager_ && stageObjectiveManager_->IsStageObjectiveFailed();
}

void GamePlayWorld::SetDebugCameraEnabled(bool enabled)
{
	if (skyBox_)
	{
		skyBox_->SetDebugCamera(enabled);
	}

	if (auto* player = characters_.GetPlayer())
	{
		player->SetDebugCamera(enabled);
	}
}

void GamePlayWorld::SetDefenseTargetDestroyed(bool destroyed)
{
	if (stageObjectiveManager_)
	{
		stageObjectiveManager_->SetDefenseTargetDestroyed(destroyed);
	}
}

bool GamePlayWorld::CheckCrosshairTargetingEnemy() const
{
	if (!collisionManager_) return false;

	const auto* player = characters_.GetPlayer();
	if (!player) return false;

	const auto* camera = player->GetCamera();
	if (!camera) return false;

	const K4E::Vector3 origin = camera->GetTranslate();
	const K4E::Vector3 forward = K4E::Vector3::Normalize(camera->GetForward());
	const float aimRange = 1000.0f;

	K4E::Segment seg{};
	seg.origin = origin;
	seg.diff = forward * aimRange;

	// HUDの照準色変更用なので、命中詳細ではなく「敵に遮られているか」だけを問い合わせる。
	return collisionManager_->SegmentCast(
		(uint32_t)CollisionTypeIdDef::kEnemy,
		seg,
		nullptr
	);
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

	collisionManager_->Update();
	collisionManager_->CheckAllCollisions();
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
