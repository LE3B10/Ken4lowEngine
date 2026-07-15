#pragma once

#include "ApplicationLayer/Character/Player/Actor/PlayerActor.h"
#include "BulletManager.h"
#include "Player.h"
#include "Stage.h"

#include <ActorWorld.h>
#include <AudioManager.h>
#include <Camera.h>
#include <CameraManager.h>
#include <Input.h>
#include <InputSnapshot.h>
#include <PhysicsWorld.h>
#include <StagePhysicsBinder.h>

#include <algorithm>
#include <cmath>

/// P10中だけ新PlayerActorをGamePlay実ステージへ投入し、旧Playerを互換Proxyとして残す移行ランタイム。
class GamePlayPlayerMigrationRuntime
{
public:
	bool Initialize(Player* legacyPlayer, BulletManager* bulletManager, K4E::Stage* stage)
	{
		Finalize();
		legacyPlayer_ = legacyPlayer;
		bulletManager_ = bulletManager;
		stage_ = stage;
		if (!legacyPlayer_ || !stage_) return false;

		playerPhysicsWorld_.SetUseFixedStep(false);
		playerActorWorld_.SetPhysicsWorld(&playerPhysicsWorld_);

		K4E::PlayerActor& player = playerActorWorld_.SpawnActor<K4E::PlayerActor>();
		player.SetName("GamePlayPlayer");
		player.SetLayer("Player");
		player.AddTag("Player");
		player.AddTag("P10Migration");
		playerActorWorld_.Initialize();
		player_ = &player;

		playerStagePhysicsBinder_.Bind(playerPhysicsWorld_, stage_->GetWorldColliderPointers());
		player_->ResetForValidation(legacyPlayer_->GetCenterPosition());
		player_->SetGameplayHudVisible(false); // 既存Wave/Tutorial HUDを維持し、P10中のPlayer HUD二重表示を避ける。
		if (auto* visual = player_->GetHumanoidVisualComponent()) visual->SetActive(false);

		if (K4E::PlayerCameraComponent* cameraComponent = player_->GetPlayerCameraComponent())
		{
			if (K4E::Camera* camera = cameraComponent->GetCamera())
			{
				camera->SetFarClip(1600.0f);
				K4E::CameraManager::GetInstance()->SetMainCamera(camera);
			}
		}

		legacyPlayer_->SetDebugCamera(true);
		legacyPlayer_->SetStartGameplayVisualsVisible(false);
		lastLegacyHp_ = legacyPlayer_->GetHP();
		lastShotRevision_ = player_->GetWeaponComponent() ? player_->GetWeaponComponent()->GetShotRevision() : 0u;
		active_ = true;
		SyncLegacyProxyTransform();
		return true;
	}

	void Finalize()
	{
		active_ = false;
		playerStagePhysicsBinder_.Unbind();
		playerActorWorld_.Finalize();
		player_ = nullptr;
		legacyPlayer_ = nullptr;
		bulletManager_ = nullptr;
		stage_ = nullptr;
		lastLegacyHp_ = 0.0f;
		lastShotRevision_ = 0u;
	}

	void Update(float deltaTime, bool allowInput = true)
	{
		if (!active_ || !player_) return;

		K4E::Input* input = K4E::Input::GetInstance();
		K4E::PlayerInputComponent* playerInput = player_->GetPlayerInputComponent();
		const bool canControl = allowInput && input && playerInput && input->IsGameInputEnabled() && !K4E::CameraManager::GetInstance()->IsUsingDebugCamera();
		if (canControl)
		{
			const InputSnapshot snapshot = K4E::BuildInputSnapshot(*input);
			playerInput->ApplyInputSnapshot(snapshot, kMouseLookSensitivity);
		}
		else if (playerInput)
		{
			playerInput->ResetInputState();
		}

		playerActorWorld_.Update(deltaTime);
		playerPhysicsWorld_.Update(deltaTime);
		playerActorWorld_.PostPhysicsUpdate(deltaTime);
		SyncLegacyProxyTransform();
		SpawnBridgedShots();

		if (!K4E::CameraManager::GetInstance()->IsUsingDebugCamera())
		{
			if (K4E::Camera* camera = GetCamera()) K4E::CameraManager::GetInstance()->SetMainCamera(camera);
		}
	}

	void SyncAfterLegacyCollision()
	{
		if (!active_ || !player_ || !legacyPlayer_) return;

		const float legacyHp = legacyPlayer_->GetHP();
		const float delta = legacyHp - lastLegacyHp_;
		if (delta < -0.001f)
		{
			player_->ApplyPlayerDamage(-delta); // 既存Enemy/BossのCollision結果だけを新Player Healthへ移す。
		}
		else if (delta > 0.001f)
		{
			player_->HealPlayer(delta);
		}
		lastLegacyHp_ = legacyHp;
	}

	void PrepareRenderState()
	{
		if (active_) playerActorWorld_.PrepareRenderState();
	}

	void Draw()
	{
		if (active_) playerActorWorld_.Draw();
	}

	void DrawShadow()
	{
		if (active_) playerActorWorld_.DrawShadow();
	}

	void SetDebugCameraEnabled(bool enabled)
	{
		if (!active_ || !player_) return;
		if (K4E::PlayerInputComponent* input = player_->GetPlayerInputComponent())
		{
			if (enabled) input->ResetInputState();
		}
	}

	bool IsActive() const { return active_ && player_ != nullptr; }
	K4E::PlayerActor* GetPlayer() { return player_; }
	const K4E::PlayerActor* GetPlayer() const { return player_; }
	IPlayerRuntime* GetPlayerRuntime() { return player_; }
	const IPlayerRuntime* GetPlayerRuntime() const { return player_; }

	K4E::Camera* GetCamera() const
	{
		if (!player_) return nullptr;
		const K4E::PlayerCameraComponent* cameraComponent = player_->GetPlayerCameraComponent();
		return cameraComponent ? cameraComponent->GetCamera() : nullptr;
	}

	K4E::Vector3 GetPlayerPosition() const
	{
		if (!player_ || !player_->GetRootComponent()) return {};
		return player_->GetRootComponent()->GetWorldPosition();
	}

private:
	void SyncLegacyProxyTransform()
	{
		if (!player_ || !legacyPlayer_) return;
		legacyPlayer_->ApplyPhysicsCorrectedPosition(GetPlayerPosition());
		if (const K4E::PlayerCameraComponent* camera = player_->GetPlayerCameraComponent())
		{
			legacyPlayer_->SetViewLookAngles(camera->GetPitch(), camera->GetYaw());
			legacyPlayer_->SyncViewToPlayer();
		}
	}

	void SpawnBridgedShots()
	{
		if (!player_ || !bulletManager_) return;
		K4E::WeaponComponent* weapon = player_->GetWeaponComponent();
		K4E::Camera* camera = GetCamera();
		if (!weapon || !camera) return;

		const unsigned int currentRevision = weapon->GetShotRevision();
		while (lastShotRevision_ < currentRevision)
		{
			const K4E::Vector3 forward = K4E::Vector3::Normalize(camera->GetForward());
			const K4E::Vector3 start = camera->GetTranslate() + forward * 1.2f;
			const float speed = 90.0f;
			const float lifeTime = (std::max)(0.1f, weapon->GetRange() / speed);
			const int damage = (std::max)(1, static_cast<int>(std::lround(weapon->GetDamage())));
			bulletManager_->Spawn(start, forward, speed, damage, lifeTime, GetPlayerPosition());
			K4E::AudioManager::GetInstance()->PlaySE("player_fire.mp3", 0.1f);
			++lastShotRevision_;
		}
	}

private:
	static constexpr float kMouseLookSensitivity = 0.0025f;
	K4E::ActorWorld playerActorWorld_{};
	K4E::PhysicsWorld playerPhysicsWorld_{};
	K4E::StagePhysicsBinder playerStagePhysicsBinder_{};
	K4E::PlayerActor* player_ = nullptr;
	Player* legacyPlayer_ = nullptr;
	BulletManager* bulletManager_ = nullptr;
	K4E::Stage* stage_ = nullptr;
	float lastLegacyHp_ = 0.0f;
	unsigned int lastShotRevision_ = 0u;
	bool active_ = false;
};
