#pragma once

#include "ApplicationLayer/Character/Player/Actor/PlayerActor.h"
#include "BulletManager.h"
#include "Player.h"
#include "Stage.h"

#include <ActorWorld.h>
#include <AudioManager.h>
#include <Camera.h>
#include <CameraManager.h>
#include <Collider.h>
#include <Input.h>
#include <InputSnapshot.h>
#include <PhysicsWorld.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>
#include <vector>

/// P10/P11中だけ新PlayerActorをGamePlay実ステージへ投入し、旧Playerを互換Proxyとして残す移行ランタイム。
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

		stageColliders_ = stage_->GetWorldColliderPointers();
		player_->ResetForValidation(legacyPlayer_->GetCenterPosition());
		if (K4E::WeaponComponent* weapon = player_->GetWeaponComponent())
		{
			weapon->ConfigureAmmoState(
				legacyPlayer_->GetCurrentWeaponMagazineCapacity(),
				legacyPlayer_->GetCurrentWeaponMagazineAmmo(),
				legacyPlayer_->GetCurrentWeaponReserveAmmo()); // P11開始時は既存GamePlayの武器残弾を新Weaponへ引き継ぐ。
		}
		RefreshNearbyStageColliders(true); // Player周辺だけをPhysicsWorldへ登録し、Stage全ColliderのO(N^2)判定を避ける。
		player_->SetGameplayHudVisible(false);
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
		lastLegacyReserveAmmo_ = legacyPlayer_->GetCurrentWeaponReserveAmmo();
		lastShotRevision_ = player_->GetWeaponComponent() ? player_->GetWeaponComponent()->GetShotRevision() : 0u;
		active_ = true;
		SyncLegacyProxyTransform();
		SyncLegacyProxyWeaponState();
		return true;
	}

	void Finalize()
	{
		active_ = false;
		ClearNearbyStageColliders();
		stageColliders_.clear();
		playerActorWorld_.Finalize();
		player_ = nullptr;
		legacyPlayer_ = nullptr;
		bulletManager_ = nullptr;
		stage_ = nullptr;
		lastLegacyHp_ = 0.0f;
		lastLegacyReserveAmmo_ = 0;
		lastShotRevision_ = 0u;
		lastStageRefreshPosition_ = {};
		hasStageRefreshPosition_ = false;
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
		RefreshNearbyStageColliders(false);
		playerPhysicsWorld_.Update(deltaTime);
		playerActorWorld_.PostPhysicsUpdate(deltaTime);
		SyncLegacyProxyTransform();
		SyncLegacyProxyWeaponState();
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
		const float hpDelta = legacyHp - lastLegacyHp_;
		if (hpDelta < -0.001f)
		{
			player_->ApplyPlayerDamage(-hpDelta);
		}
		else if (hpDelta > 0.001f)
		{
			player_->HealPlayer(hpDelta);
		}
		lastLegacyHp_ = legacyHp;

		const int legacyReserveAmmo = legacyPlayer_->GetCurrentWeaponReserveAmmo();
		const int reserveDelta = legacyReserveAmmo - lastLegacyReserveAmmo_;
		if (reserveDelta != 0)
		{
			if (K4E::WeaponComponent* weapon = player_->GetWeaponComponent())
			{
				weapon->AddReserveAmmo(reserveDelta); // Item/Tutorialが旧Proxyへ加えた弾薬差分だけを新Weaponへ移す。
			}
		}
		lastLegacyReserveAmmo_ = legacyReserveAmmo;
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
	size_t GetRegisteredStageColliderCount() const { return activeStageColliders_.size(); }
	size_t GetTotalStageColliderCount() const { return stageColliders_.size(); }

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
	static float DistanceSquaredPointToAabbXZ(const K4E::Vector3& point, const K4E::AABB& bounds)
	{
		const float dx = point.x < bounds.min.x ? bounds.min.x - point.x : (point.x > bounds.max.x ? point.x - bounds.max.x : 0.0f);
		const float dz = point.z < bounds.min.z ? bounds.min.z - point.z : (point.z > bounds.max.z ? point.z - bounds.max.z : 0.0f);
		return dx * dx + dz * dz;
	}

	void RefreshNearbyStageColliders(bool force)
	{
		if (!player_) return;
		const K4E::Vector3 playerPosition = GetPlayerPosition();
		if (!force && hasStageRefreshPosition_)
		{
			const float dx = playerPosition.x - lastStageRefreshPosition_.x;
			const float dz = playerPosition.z - lastStageRefreshPosition_.z;
			if (dx * dx + dz * dz < kStageRefreshDistance * kStageRefreshDistance) return;
		}

		std::vector<std::pair<float, K4E::Collider*>> candidates;
		candidates.reserve(stageColliders_.size());
		for (K4E::Collider* collider : stageColliders_)
		{
			if (!collider || !collider->IsCollisionEnabledForQuery()) continue;
			const float distanceSq = DistanceSquaredPointToAabbXZ(playerPosition, collider->GetAABB());
			if (distanceSq <= kStageActivationRadius * kStageActivationRadius)
			{
				candidates.emplace_back(distanceSq, collider);
			}
		}

		if (candidates.size() > kMaxActiveStageColliders)
		{
			std::nth_element(
				candidates.begin(),
				candidates.begin() + static_cast<std::ptrdiff_t>(kMaxActiveStageColliders),
				candidates.end(),
				[](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
			candidates.resize(kMaxActiveStageColliders);
		}

		std::unordered_set<K4E::Collider*> desired;
		desired.reserve(candidates.size());
		for (const auto& candidate : candidates) desired.insert(candidate.second);

		for (auto it = activeStageColliders_.begin(); it != activeStageColliders_.end();)
		{
			if (!desired.contains(*it))
			{
				playerPhysicsWorld_.UnregisterCollider(*it);
				it = activeStageColliders_.erase(it);
			}
			else
			{
				++it;
			}
		}
		for (K4E::Collider* collider : desired)
		{
			if (activeStageColliders_.insert(collider).second)
			{
				playerPhysicsWorld_.RegisterCollider(collider);
			}
		}

		lastStageRefreshPosition_ = playerPosition;
		hasStageRefreshPosition_ = true;
	}

	void ClearNearbyStageColliders()
	{
		for (K4E::Collider* collider : activeStageColliders_)
		{
			playerPhysicsWorld_.UnregisterCollider(collider);
		}
		activeStageColliders_.clear();
	}

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

	void SyncLegacyProxyWeaponState()
	{
		if (!player_ || !legacyPlayer_) return;
		K4E::WeaponComponent* weapon = player_->GetWeaponComponent();
		if (!weapon) return;

		legacyPlayer_->GetWeaponComponent().ApplyMigrationProxyState(
			weapon->GetMagazineAmmo(),
			weapon->GetReserveAmmo(),
			weapon->IsReloading(),
			weapon->GetReloadTimer());
		lastLegacyReserveAmmo_ = weapon->GetReserveAmmo();
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
	static constexpr float kStageActivationRadius = 18.0f;
	static constexpr float kStageRefreshDistance = 3.0f;
	static constexpr size_t kMaxActiveStageColliders = 96;
	K4E::ActorWorld playerActorWorld_{};
	K4E::PhysicsWorld playerPhysicsWorld_{};
	K4E::PlayerActor* player_ = nullptr;
	Player* legacyPlayer_ = nullptr;
	BulletManager* bulletManager_ = nullptr;
	K4E::Stage* stage_ = nullptr;
	std::vector<K4E::Collider*> stageColliders_{};
	std::unordered_set<K4E::Collider*> activeStageColliders_{};
	K4E::Vector3 lastStageRefreshPosition_{};
	float lastLegacyHp_ = 0.0f;
	int lastLegacyReserveAmmo_ = 0;
	unsigned int lastShotRevision_ = 0u;
	bool hasStageRefreshPosition_ = false;
	bool active_ = false;
};
