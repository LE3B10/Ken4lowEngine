#define NOMINMAX
#include "GamePlayWorld.h"

#include "GamePlayStageContext.h"

#include "LightManager.h"
#include "SkyBoxManager.h"
#include "Wireframe.h"
#include "Player.h"
#include "EnemyBase.h"

#include <cmath>

using namespace Ken4lowEngine;

void GamePlayWorld::Initialize(GamePlayStageContext& stageContext)
{
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

	hudManager_ = std::make_unique<HUDManager>();
	hudManager_->SetPlayer(characters_.GetPlayer());
	hudManager_->Initialize();

	if (auto* player = characters_.GetPlayer())
	{
		player->SetHUDManager(hudManager_.get());
	}

	const auto stageAssets = stageContext.GetCurrentStageAssets();

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
	stageContext.SetupWaves(waveManager_.get());

	prevWaveNumber_ = 0;
	prevWaveInProgress_ = false;
	prevAllWavesCleared_ = false;
}

void GamePlayWorld::Finalize()
{
	waveManager_.reset();
	hudManager_.reset();

	EnemyBase::SetGlobalStageWorldAABBs(nullptr);

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

	UpdateShadowLightViewProjection();

	if (stage_)
	{
		stage_->UpdateShadowMatrix(shadowLightViewProjection_);
	}
	characters_.UpdateShadowMatrix(shadowLightViewProjection_);

	if (bulletManager_)
	{
		bulletManager_->Update(deltaTime);
	}

	CollisionUpdate();

	if (skyBox_)
	{
		skyBox_->Update();
	}

	if (hudManager_ && characters_.GetPlayer())
	{
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
	}

	if (bulletManager_)
	{
		bulletManager_->Draw();
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
	if (!hideDuringIntro && hudManager_)
	{
		hudManager_->Draw();
	}
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
	if (waveManager_)
	{
		waveManager_->Start();
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