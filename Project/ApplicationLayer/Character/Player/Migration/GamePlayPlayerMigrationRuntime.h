#pragma once

#include "ApplicationLayer/Character/Player/Actor/PlayerActor.h"
#include "ApplicationLayer/Character/Player/Migration/PlayerTutorialRestrictionBridge.h"
#include "BulletManager.h"
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
#include <unordered_set>
#include <utility>
#include <vector>

/// P13以降はCharacterWorld所有のPlayerActorをGamePlay入力・Physics・Bulletへ接続するRuntime Controller。
class GamePlayPlayerMigrationRuntime
{
public:
	bool Initialize(
		BulletManager* bulletManager,
		K4E::Stage* stage,
		K4E::ActorWorld* actorWorld,
		K4E::PhysicsWorld* physicsWorld,
		const K4E::Vector3& spawnPosition)
	{
		Finalize();
		bulletManager_ = bulletManager;
		stage_ = stage;
		actorWorld_ = actorWorld;
		physicsWorld_ = physicsWorld;
		if (!stage_ || !actorWorld_ || !physicsWorld_) return false;

		K4E::PlayerActor& player = actorWorld_->SpawnActor<K4E::PlayerActor>();
		player.SetName("GamePlayPlayer");
		player.SetLayer("Player");
		player.AddTag("Player");
		player.AddTag("P13Runtime");
		player_ = &player;

		stageColliders_ = stage_->GetWorldColliderPointers();
		player_->ResetForValidation(spawnPosition);
		if (K4E::WeaponComponent* weapon = player_->GetWeaponComponent())
		{
			weapon->ConfigureAmmoState(30, 30, 90, 120); // 旧Playerを生成せず、現行Primaryの初期弾数を新Weapon側で正本化する。
		}
		RefreshNearbyStageColliders(true);
		player_->SetGameplayHudVisible(true);
		if (auto* visual = player_->GetHumanoidVisualComponent()) visual->SetActive(false);

		if (K4E::Camera* camera = GetCamera())
		{
			camera->SetFarClip(1600.0f);
			K4E::CameraManager::GetInstance()->SetMainCamera(camera);
		}

		lastShotRevision_ = player_->GetWeaponComponent() ? player_->GetWeaponComponent()->GetShotRevision() : 0u;
		active_ = true;
		return true;
	}

	void Finalize()
	{
		active_ = false;
		ClearNearbyStageColliders();
		stageColliders_.clear();
		player_ = nullptr;
		bulletManager_ = nullptr;
		stage_ = nullptr;
		actorWorld_ = nullptr;
		physicsWorld_ = nullptr;
		lastShotRevision_ = 0u;
		lastStageRefreshPosition_ = {};
		hasStageRefreshPosition_ = false;
	}

	void Update(float deltaTime, bool allowInput = true)
	{
		if (!active_ || !player_ || !actorWorld_ || !physicsWorld_) return;

		K4E::Input* input = K4E::Input::GetInstance();
		K4E::PlayerInputComponent* playerInput = player_->GetPlayerInputComponent();
		if (playerInput)
		{
			const PlayerTutorialRestrictionBridge::State tutorialRestrictions = PlayerTutorialRestrictionBridge::GetState();
			playerInput->SetInputRestrictions(
				tutorialRestrictions.enabled,
				tutorialRestrictions.allowMove,
				tutorialRestrictions.allowShoot,
				tutorialRestrictions.allowReload);
		}

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

		actorWorld_->Update(deltaTime);
		RefreshNearbyStageColliders(false);
		physicsWorld_->Update(deltaTime);
		actorWorld_->PostPhysicsUpdate(deltaTime);
		SpawnBridgedShots();

		if (!K4E::CameraManager::GetInstance()->IsUsingDebugCamera())
		{
			if (K4E::Camera* camera = GetCamera()) K4E::CameraManager::GetInstance()->SetMainCamera(camera);
		}
	}

	void PrepareRenderState()
	{
		if (active_ && actorWorld_) actorWorld_->PrepareRenderState();
	}

	void Draw()
	{
		if (active_ && actorWorld_) actorWorld_->Draw();
	}

	void DrawShadow()
	{
		if (active_ && actorWorld_) actorWorld_->DrawShadow();
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

	K4E::Camera* GetCamera() const { return player_ ? player_->GetCamera() : nullptr; }
	K4E::Vector3 GetPlayerPosition() const { return player_ ? player_->GetWorldPosition() : K4E::Vector3{}; }

private:
	static float DistanceSquaredPointToAabbXZ(const K4E::Vector3& point, const K4E::AABB& bounds)
	{
		const float dx = point.x < bounds.min.x ? bounds.min.x - point.x : (point.x > bounds.max.x ? point.x - bounds.max.x : 0.0f);
		const float dz = point.z < bounds.min.z ? bounds.min.z - point.z : (point.z > bounds.max.z ? point.z - bounds.max.z : 0.0f);
		return dx * dx + dz * dz;
	}

	void RefreshNearbyStageColliders(bool force)
	{
		if (!player_ || !physicsWorld_) return;
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
			if (distanceSq <= kStageActivationRadius * kStageActivationRadius) candidates.emplace_back(distanceSq, collider);
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
				physicsWorld_->UnregisterCollider(*it);
				it = activeStageColliders_.erase(it);
			}
			else
			{
				++it;
			}
		}
		for (K4E::Collider* collider : desired)
		{
			if (activeStageColliders_.insert(collider).second) physicsWorld_->RegisterCollider(collider);
		}

		lastStageRefreshPosition_ = playerPosition;
		hasStageRefreshPosition_ = true;
	}

	void ClearNearbyStageColliders()
	{
		if (physicsWorld_)
		{
			for (K4E::Collider* collider : activeStageColliders_) physicsWorld_->UnregisterCollider(collider);
		}
		activeStageColliders_.clear();
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
	K4E::ActorWorld* actorWorld_ = nullptr;
	K4E::PhysicsWorld* physicsWorld_ = nullptr;
	K4E::PlayerActor* player_ = nullptr;
	BulletManager* bulletManager_ = nullptr;
	K4E::Stage* stage_ = nullptr;
	std::vector<K4E::Collider*> stageColliders_{};
	std::unordered_set<K4E::Collider*> activeStageColliders_{};
	K4E::Vector3 lastStageRefreshPosition_{};
	unsigned int lastShotRevision_ = 0u;
	bool hasStageRefreshPosition_ = false;
	bool active_ = false;
};
