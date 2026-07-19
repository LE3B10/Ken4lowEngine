#pragma once

#include "ApplicationLayer/Character/CharacterWorld/CharacterWorld.h"
#include "ApplicationLayer/Character/Player/IPlayerRuntime.h"
#include "ApplicationLayer/Scene/GamePlayScene/Core/GamePlayStageContext.h"
#include "Stage2DeviceActor.h"

#include <ActorWorld.h>
#include <Input.h>

#include <algorithm>
#include <array>
#include <functional>
#include <string>
#include <vector>

/// Stage 2装置をCharacterWorldのActorWorldへ生成し、探索戦闘・操作・Objective通知を管理する。
class Stage2CharacterDeviceManager final
{
public:
	struct PromptSnapshot
	{
		bool visible = false;
		std::string text;
		float normalizedProgress = 0.0f;
	};

	void Initialize(const GamePlayStageContext& stageContext)
	{
		Finalize();
		const auto rule = stageContext.GetCurrentStageRule();
		active_ = rule.objectiveType == GamePlayStageContext::StageObjectiveType::ActivateDevices;
		if (!active_) return;
		pendingDevicePoints_ = stageContext.GetDevicePoints();
		if (stageContext.GetCurrentStageAssets().jsonPath.find("wasureraretakoudou") != std::string::npos)
		{
			pendingDevicePoints_ = {
				{ "MineDevice_West", { -34.0f, 0.0f, -10.0f } },
				{ "MineDevice_East", { 38.0f, 0.0f, 45.0f } },
				{ "MineDevice_Deep", { 0.0f, 0.0f, 110.0f } }
			}; // 拡張した長方形マップでは装置間距離を広げ、前半・中盤・最奥へ分散する。
		}
		requiredDeviceCount_ = std::max(1, rule.requiredDeviceCount);
	}

	void Finalize()
	{
		for (Stage2DeviceActor* device : devices_)
		{
			if (device) device->Destroy();
		}
		devices_.clear();
		pendingDevicePoints_.clear();
		prompt_ = {};
		encounterTriggered_.fill(false);
		requiredDeviceCount_ = 0;
		activationSequence_ = 0;
		feedbackTimer_ = 0.0f;
		bossPhaseStarted_ = false;
		active_ = false;
	}

	void Update(
		IPlayerRuntime* player,
		K4E::Input* input,
		CharacterWorld& characters,
		float deltaTime,
		const std::function<void(const std::string&)>& onActivated)
	{
		prompt_ = {};
		if (!active_) return;
		SpawnDevicesIfNeeded(characters.GetActorWorld());
		UpdateAmbientEncounters(player, characters);

		feedbackTimer_ = std::max(0.0f, feedbackTimer_ - std::max(0.0f, deltaTime));
		Stage2DeviceActor* nearest = FindNearestDevice(player);
		const bool interactHeld = nearest && input && input->PushKey(DIK_E);

		for (Stage2DeviceActor* device : devices_)
		{
			if (!device) continue;
			const bool activatedNow = device->UpdateInteraction(device == nearest, device == nearest && interactHeld, deltaTime);
			device->Update(0.0f);
			if (!activatedNow) continue;

			++activationSequence_;
			feedbackTimer_ = 1.1f;
			if (activationSequence_ < requiredDeviceCount_)
			{
				SpawnActivationReinforcements(*device, characters);
			}
			else
			{
				bossPhaseStarted_ = true; // 最終装置では通常増援を止め、続くボス登場演出へ戦闘密度を切り替える。
			}
			if (onActivated) onActivated(device->GetDeviceId());
		}

		if (feedbackTimer_ > 0.0f)
		{
			prompt_.visible = true;
			prompt_.text = AreAllDevicesActivated() ? "全装置起動　強い反応を検知" : "装置を起動しました";
			prompt_.normalizedProgress = 1.0f;
		}
		else if (nearest && !nearest->IsActivated())
		{
			prompt_.visible = true;
			prompt_.text = nearest->GetInteractionProgress() > 0.01f ? "E 長押し：起動中" : "E 長押し：装置を起動";
			prompt_.normalizedProgress = nearest->GetInteractionProgress();
		}
	}

	void Draw() {}
	void DrawShadow() {}

	void UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection)
	{
		for (Stage2DeviceActor* device : devices_)
		{
			if (device) device->UpdateShadowMatrix(lightViewProjection);
		}
	}

	const PromptSnapshot& GetPromptSnapshot() const { return prompt_; }
	int GetActivatedCount() const
	{
		return static_cast<int>(std::count_if(devices_.begin(), devices_.end(), [](const Stage2DeviceActor* device)
			{
				return device && device->IsActivated();
			}));
	}
	bool AreAllDevicesActivated() const { return active_ && requiredDeviceCount_ > 0 && GetActivatedCount() >= requiredDeviceCount_; }
	bool IsActive() const { return active_; }

private:
	void SpawnDevicesIfNeeded(K4E::ActorWorld& actorWorld)
	{
		if (!devices_.empty() || pendingDevicePoints_.empty()) return;
		const int spawnCount = std::min(requiredDeviceCount_, static_cast<int>(pendingDevicePoints_.size()));
		for (int index = 0; index < spawnCount; ++index)
		{
			auto& actor = actorWorld.SpawnActor<Stage2DeviceActor>();
			const auto& point = pendingDevicePoints_[static_cast<size_t>(index)];
			const std::string id = point.name.empty() ? "Stage2Device_" + std::to_string(index + 1) : point.name;
			actor.Configure(id, point.position);
			actor.Update(0.0f);
			devices_.push_back(&actor); // Player・Enemyと同じActorWorldへ置き、描画とShadow Passを共通化する。
		}
	}

	Stage2DeviceActor* FindNearestDevice(const IPlayerRuntime* player) const
	{
		if (!player) return nullptr;
		const K4E::Vector3 playerPosition = player->GetWorldPosition();
		Stage2DeviceActor* nearest = nullptr;
		float nearestDistanceSq = 1.0e30f;
		for (Stage2DeviceActor* device : devices_)
		{
			if (!device || device->IsActivated()) continue;
			const float distanceSq = device->GetDistanceSquaredTo(playerPosition);
			const float radius = device->GetInteractionRadius();
			if (distanceSq <= radius * radius && distanceSq < nearestDistanceSq)
			{
				nearest = device;
				nearestDistanceSq = distanceSq;
			}
		}
		return nearest;
	}

	void UpdateAmbientEncounters(const IPlayerRuntime* player, CharacterWorld& characters)
	{
		if (!player || bossPhaseStarted_ || characters.GetAliveNormalEnemyCount() > 9) return;
		const K4E::Vector3 playerPosition = player->GetWorldPosition();
		const std::array<K4E::Vector3, 5> centers = {{
			{ 0.0f, 0.0f, -31.0f },
			{ -34.0f, 0.0f, -10.0f },
			{ 0.0f, 0.0f, 22.0f },
			{ 36.0f, 0.0f, 45.0f },
			{ 0.0f, 0.0f, 78.0f }
		}};
		const std::array<float, 5> radii = { 18.0f, 16.0f, 18.0f, 16.0f, 20.0f };

		for (size_t index = 0; index < centers.size(); ++index)
		{
			if (encounterTriggered_[index]) continue;
			const K4E::Vector3 delta = playerPosition - centers[index];
			const float distanceSq = delta.x * delta.x + delta.z * delta.z;
			if (distanceSq > radii[index] * radii[index]) continue;
			SpawnAmbientEncounter(index, characters);
			encounterTriggered_[index] = true; // 各区画へ初めて入った時だけ敵群を出し、長いマップの移動中にも戦闘を作る。
			break;
		}
	}

	void SpawnAmbientEncounter(size_t index, CharacterWorld& characters)
	{
		switch (index)
		{
		case 0:
			characters.SpawnEnemyAt({ -7.0f, 2.0f, -25.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ 7.0f, 2.0f, -20.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ 0.0f, 2.0f, -14.0f }, EnemyType::MidRange);
			break;
		case 1:
			characters.SpawnEnemyAt({ -43.0f, 2.0f, -17.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ -27.0f, 2.0f, -16.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ -42.0f, 2.0f, 1.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ -25.0f, 2.0f, 2.0f }, EnemyType::MidRange);
			break;
		case 2:
			characters.SpawnEnemyAt({ -8.0f, 2.0f, 18.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ 8.0f, 2.0f, 24.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ -5.0f, 2.0f, 32.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ 6.0f, 2.0f, 36.0f }, EnemyType::MidRange);
			break;
		case 3:
			characters.SpawnEnemyAt({ 27.0f, 2.0f, 37.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ 43.0f, 2.0f, 39.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ 29.0f, 2.0f, 55.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ 43.0f, 2.0f, 54.0f }, EnemyType::MidRange);
			break;
		case 4:
		default:
			characters.SpawnEnemyAt({ -21.0f, 2.0f, 82.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ 21.0f, 2.0f, 82.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ -12.0f, 2.0f, 94.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ 12.0f, 2.0f, 96.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ -20.0f, 2.0f, 103.0f }, EnemyType::MidRange);
			characters.SpawnEnemyAt({ 20.0f, 2.0f, 104.0f }, EnemyType::MidRange);
			break;
		}
	}

	void SpawnActivationReinforcements(const Stage2DeviceActor& device, CharacterWorld& characters)
	{
		const K4E::Vector3 center = device.GetPosition();
		characters.SpawnEnemyAt(center + K4E::Vector3{ 6.0f, 2.0f, 2.0f }, EnemyType::Melee);
		characters.SpawnEnemyAt(center + K4E::Vector3{ -6.0f, 2.0f, -3.0f }, EnemyType::Melee);
		characters.SpawnEnemyAt(center + K4E::Vector3{ 1.5f, 2.0f, 7.0f }, EnemyType::MidRange);
		if (activationSequence_ >= 2)
		{
			characters.SpawnEnemyAt(center + K4E::Vector3{ -2.0f, 2.0f, 8.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt(center + K4E::Vector3{ 8.0f, 2.0f, -5.0f }, EnemyType::MidRange); // 2基目は5体の増援にして中盤の圧力を上げる。
		}
	}

	std::vector<Stage2DeviceActor*> devices_;
	std::vector<GamePlayStageContext::DevicePointInfo> pendingDevicePoints_;
	std::array<bool, 5> encounterTriggered_{};
	PromptSnapshot prompt_{};
	int requiredDeviceCount_ = 0;
	int activationSequence_ = 0;
	float feedbackTimer_ = 0.0f;
	bool bossPhaseStarted_ = false;
	bool active_ = false;
};
