#define NOMINMAX
#include "DirectXCommon.h"
#include "GamePlayWorld.h"

#include "GamePlayStageContext.h"
#include "GameViewportConstants.h"

#include "LightManager.h"
#include "SkyBoxManager.h"
#include "Wireframe.h"
#include "Player.h"
#include "EnemyBase.h"
#include "GpuParticleManager.h"
#include "ParticleManager.h"
#include "CollisionTypeIdDef.h"

#include <chrono>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace Ken4lowEngine;

void GamePlayWorld::Initialize(GamePlayStageContext& stageContext)
{
	const auto stageAssets = stageContext.GetCurrentStageAssets();
	stageRule_ = stageContext.GetCurrentStageRule();

	activatedDeviceCount_ = 0;
	defendElapsedSec_ = 0.0f;
	stageElapsedSec_ = 0.0f;
	reachedGoal_ = false;
	bossDefeated_ = false;
	defenseTargetDestroyed_ = false;

	K4E::Vector3 dummy{};
	if (!TryGetDirectionalLightFromManager(dummy))
	{
		LightManager::GetInstance()->AddDefaultDirectionalLight();
	}

	skyBox_ = std::make_unique<K4E::SkyBox>();
	skyBox_->Initialize("SkyBox/skybox.dds");

	collisionManager_ = std::make_unique<CollisionManager>();
	collisionManager_->Initialize();

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

	EnemyBase::SetGlobalStageWorldAABBs(&stage_->GetWorldAABBs());

	if (auto* player = characters_.GetPlayer())
	{
		player->SetStageWorldAABBs(&stage_->GetWorldAABBs());

		WorldCollisionSettings playerCollisionSettings{};
		playerCollisionSettings.half = { 0.5f, 1.0f, 0.5f };
		playerCollisionSettings.centerOffset = { 0.0f, 1.0f, 0.0f };
		player->SetWorldCollisionSettings(playerCollisionSettings);
	}

	stageContext.LoadSpawnPointsFromLevel(stageAssets.jsonPath);

	if (auto* player = characters_.GetPlayer())
	{
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

	if (stageRule_.useWaveSystem)
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

	enemyHpBarManager_.Initialize();
}

void GamePlayWorld::Finalize()
{
	waveManager_.reset();
	hudManager_.reset();

	EnemyBase::SetGlobalStageWorldAABBs(nullptr);
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

	characters_.Update(deltaTime);
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

	if (bulletManager_)
	{
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
		const bool isTargetingEnemy = CheckCrosshairTargetingEnemy();
		hudManager_->SetCrosshairTargetingEnemy(isTargetingEnemy);

		hudManager_->SetHP(
			characters_.GetPlayer()->GetHP(),
			characters_.GetPlayer()->GetMaxHP());
		hudManager_->Update(deltaTime);
	}

	if (waveManager_)
	{
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

	UpdateStageObjective(deltaTime);
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

void GamePlayWorld::Draw3D(bool hideCharactersDuringIntro)
{
	K4E::SkyBoxManager::GetInstance()->SetRenderSetting();
	if (skyBox_)
	{
		skyBox_->Draw();
	}

	if (!hideCharactersDuringIntro)
	{
		characters_.Draw();
	}

	if (stage_)
	{
		stage_->Draw();
		stage_->DrawChunkDebug();
	}

	if (bulletManager_)
	{
		bulletManager_->Draw();
	}

	itemManager_.Draw();

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

void GamePlayWorld::DrawShadow(bool hideCharactersDuringIntro)
{
	if (stage_)
	{
		stage_->DrawShadow();
	}

	if (!hideCharactersDuringIntro)
	{
		characters_.DrawShadow();
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
	ImGui::Text("Stage Time: %.2f sec", stageElapsedSec_);
	ImGui::Text("Activated Devices: %d", activatedDeviceCount_);
	ImGui::Text("Reached Goal: %s", reachedGoal_ ? "true" : "false");
	ImGui::Text("Boss Defeated: %s", bossDefeated_ ? "true" : "false");
	ImGui::Text("Defense Target Destroyed: %s", defenseTargetDestroyed_ ? "true" : "false");
	ImGui::Text("Player Dead: %s", IsPlayerDead() ? "true" : "false");
	ImGui::Text("Enemies: %d", characters_.GetEnemyCount());

	const float fps = ImGui::GetIO().Framerate;
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
	if (waveManager_ && !waveManager_->HasStarted())
	{
		waveManager_->Start();
	}
}

void GamePlayWorld::WarmupStartGameplayForIntro()
{
	// カメラ切り替え時の一括初期化を避けるため、イントロ中に敵・武器・腕表示を事前生成して非表示で温める。
	SetStartGameplayVisualsVisible(false);

	StartWaves();
	if (waveManager_)
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
	switch (stageRule_.objectiveType)
	{
	case GamePlayStageContext::StageObjectiveType::ClearAllWaves:
		return IsAllWavesCleared();

	case GamePlayStageContext::StageObjectiveType::ActivateDevices:
		return activatedDeviceCount_ >= stageRule_.requiredDeviceCount;

	case GamePlayStageContext::StageObjectiveType::DefendTarget:
		return !defenseTargetDestroyed_ && defendElapsedSec_ >= stageRule_.defendTimeSec;

	case GamePlayStageContext::StageObjectiveType::ReachGoal:
		return reachedGoal_;

	case GamePlayStageContext::StageObjectiveType::DefeatBoss:
		return bossDefeated_;
	}

	return false;
}

bool GamePlayWorld::IsStageObjectiveFailed() const
{
	switch (stageRule_.objectiveType)
	{
	case GamePlayStageContext::StageObjectiveType::DefendTarget:
		return defenseTargetDestroyed_;

	case GamePlayStageContext::StageObjectiveType::ReachGoal:
		return (stageRule_.timeLimitSec > 0.0f) && (stageElapsedSec_ >= stageRule_.timeLimitSec) && !reachedGoal_;

	default:
		return false;
	}
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
	defenseTargetDestroyed_ = destroyed;
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

	return collisionManager_->SegmentCast(
		(uint32_t)CollisionTypeIdDef::kEnemy,
		seg,
		nullptr
	);
}

void GamePlayWorld::AddActivatedDeviceCount(int amount)
{
	activatedDeviceCount_ += amount;
	if (activatedDeviceCount_ < 0)
	{
		activatedDeviceCount_ = 0;
	}
}

void GamePlayWorld::SetReachedGoal(bool reached)
{
	reachedGoal_ = reached;
}

void GamePlayWorld::SetBossDefeated(bool defeated)
{
	bossDefeated_ = defeated;
}

void GamePlayWorld::CollisionUpdate()
{
	if (!collisionManager_) { return; }

	collisionManager_->Update();
	collisionManager_->CheckAllCollisions();
}

void GamePlayWorld::UpdateShadowLightViewProjection()
{
	K4E::Vector3 lightDir = shadowLightDirection_;

	K4E::Vector3 managerDir{};
	if (TryGetDirectionalLightFromManager(managerDir))
	{
		lightDir = managerDir;
	}

	lightDir = K4E::Vector3::Normalize(lightDir);

	K4E::Vector3 center = { 0.0f, 0.0f, 0.0f };
	if (auto* player = characters_.GetPlayer())
	{
		if (auto* wt = player->GetWorldTransform())
		{
			center = wt->translate_;
		}
	}

	K4E::Vector3 eye = center - lightDir * shadowDistance_;
	K4E::Vector3 up = { 0.0f, 1.0f, 0.0f };

	if (std::abs(K4E::Vector3::Dot(lightDir, up)) > 0.99f)
	{
		up = { 0.0f, 0.0f, 1.0f };
	}

	K4E::Matrix4x4 view = K4E::Matrix4x4::MakeLookAtMatrix(eye, center, up);

	K4E::Matrix4x4 proj = K4E::Matrix4x4::MakeOrthographicMatrix(
		-shadowOrthoHalfWidth_,
		shadowOrthoHalfHeight_,
		shadowOrthoHalfWidth_,
		-shadowOrthoHalfHeight_,
		shadowNearZ_,
		shadowFarZ_);

	shadowLightViewProjection_ = K4E::Matrix4x4::Multiply(view, proj);
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

void GamePlayWorld::UpdateStageObjective(float deltaTime)
{
	stageElapsedSec_ += deltaTime;

	switch (stageRule_.objectiveType)
	{
	case GamePlayStageContext::StageObjectiveType::DefendTarget:
		if (!defenseTargetDestroyed_)
		{
			defendElapsedSec_ += deltaTime;
		}
		break;

	case GamePlayStageContext::StageObjectiveType::ReachGoal:
		// 到達判定は外部から SetReachedGoal で入れる想定
		break;

	case GamePlayStageContext::StageObjectiveType::ActivateDevices:
		// 装置起動数は外部から AddActivatedDeviceCount で入れる想定
		break;

	case GamePlayStageContext::StageObjectiveType::DefeatBoss:
		// ボス撃破は外部から SetBossDefeated で入れる想定
		break;

	case GamePlayStageContext::StageObjectiveType::ClearAllWaves:
	default:
		break;
	}
}
