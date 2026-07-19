#pragma once

#include "Stage2DeviceSystem.h"

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

/// Stage 2装置をCharacterWorldのActorWorldへ生成し、操作とObjective通知だけを管理する。
class Stage2CharacterDeviceManager final
{
public:
	using PromptSnapshot = Stage2DeviceManager::PromptSnapshot;

	void Initialize(const GamePlayStageContext& stageContext)
	{
		Finalize();
		const auto rule = stageContext.GetCurrentStageRule();
		active_ = rule.objectiveType == GamePlayStageContext::StageObjectiveType::ActivateDevices;
		if (!active_) return;
		pendingDevicePoints_ = stageContext.GetDevicePoints();
		requiredDeviceCount_ = std::max(1, rule.requiredDeviceCount); // 実体生成はCharacterWorldを参照できる最初のUpdateまで遅延する。
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
		requiredDeviceCount_ = 0;
		activationSequence_ = 0;
		feedbackTimer_ = 0.0f;
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

		feedbackTimer_ = std::max(0.0f, feedbackTimer_ - std::max(0.0f, deltaTime));
		Stage2DeviceActor* nearest = FindNearestDevice(player);
		const bool interactHeld = nearest && input && input->PushKey(DIK_E);

		for (Stage2DeviceActor* device : devices_)
		{
			if (!device) continue;
			const bool activatedNow = device->UpdateInteraction(device == nearest, device == nearest && interactHeld, deltaTime);
			device->Update(0.0f); // CharacterWorld更新後に変化したFocusと起動色を同フレームへ反映する。
			if (!activatedNow) continue;

			++activationSequence_;
			feedbackTimer_ = 1.1f;
			SpawnActivationReinforcements(*device, characters);
			if (onActivated) onActivated(device->GetDeviceId());
		}

		if (feedbackTimer_ > 0.0f)
		{
			prompt_.visible = true;
			prompt_.text = "装置を起動しました";
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

	void SpawnActivationReinforcements(const Stage2DeviceActor& device, CharacterWorld& characters)
	{
		const K4E::Vector3 center = device.GetPosition();
		characters.SpawnEnemyAt(center + K4E::Vector3{ 5.5f, 2.0f, 1.0f }, EnemyType::Melee);
		if (activationSequence_ >= 2)
		{
			characters.SpawnEnemyAt(center + K4E::Vector3{ -5.0f, 2.0f, -2.0f }, EnemyType::MidRange);
		}
		if (activationSequence_ >= 3)
		{
			characters.SpawnEnemyAt(center + K4E::Vector3{ 1.5f, 2.0f, 6.0f }, EnemyType::Melee); // 最終装置では増援数を増やす。
		}
	}

	std::vector<Stage2DeviceActor*> devices_;
	std::vector<GamePlayStageContext::DevicePointInfo> pendingDevicePoints_;
	PromptSnapshot prompt_{};
	int requiredDeviceCount_ = 0;
	int activationSequence_ = 0;
	float feedbackTimer_ = 0.0f;
	bool active_ = false;
};
