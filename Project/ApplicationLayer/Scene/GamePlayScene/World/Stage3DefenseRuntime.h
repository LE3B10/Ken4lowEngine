#pragma once

#include "ApplicationLayer/Character/CharacterWorld/CharacterWorld.h"
#include "ApplicationLayer/Character/Enemy/Actor/EnemyActor.h"
#include "ApplicationLayer/Character/Player/IPlayerRuntime.h"
#include "ApplicationLayer/Scene/GamePlayScene/Core/GamePlayStageContext.h"

#include <LightManager.h>
#include <Object3D.h>
#include <Scene/Actor/Character/CharacterActor.h>
#include <Scene/Actor/Character/CharacterColliderComponent.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>
#include <Scene/Actor/Character/CharacterMovementComponent.h>
#include <Scene/Actor/Character/CharacterTargetComponent.h>
#include <SceneComponent.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>

namespace K4E = ::Ken4lowEngine;

/// Stage 3の防衛コアを台座・装甲・発光中枢の3層で描画するComponent。
class Stage3DefenseTargetVisualComponent final : public K4E::SceneComponent
{
public:
	std::string GetClassTypeName() const override { return "Stage3DefenseTargetVisualComponent"; }

	void Initialize() override
	{
		K4E::SceneComponent::Initialize();
		baseObject_ = CreateCube();
		armorObject_ = CreateCube();
		coreObject_ = CreateCube();
		SyncVisuals();
	}

	void Update(float deltaTime) override
	{
		K4E::SceneComponent::Update(deltaTime);
		visualTime_ += std::max(0.0f, deltaTime);
		damageFlashTimer_ = std::max(0.0f, damageFlashTimer_ - std::max(0.0f, deltaTime));
		SyncVisuals();
	}

	void UpdateEditor(float deltaTime) override
	{
		K4E::SceneComponent::UpdateEditor(deltaTime);
		visualTime_ += std::max(0.0f, deltaTime);
		SyncVisuals();
	}

	void Draw() override
	{
		if (baseObject_) baseObject_->Draw();
		if (armorObject_) armorObject_->Draw();
		if (coreObject_) coreObject_->Draw();
	}

	void DrawShadow() override
	{
		if (baseObject_) baseObject_->DrawShadow();
		if (armorObject_) armorObject_->DrawShadow();
	}

	void Finalize() override
	{
		coreObject_.reset();
		armorObject_.reset();
		baseObject_.reset();
		K4E::SceneComponent::Finalize();
	}

	void SetHealthRatio(float ratio)
	{
		const float nextRatio = std::clamp(ratio, 0.0f, 1.0f);
		if (nextRatio + 0.001f < healthRatio_) damageFlashTimer_ = 0.18f;
		healthRatio_ = nextRatio;
	}

	void SetDestroyed(bool destroyed) { destroyed_ = destroyed; }

	void UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection)
	{
		if (baseObject_) baseObject_->UpdateShadowMatrix(lightViewProjection);
		if (armorObject_) armorObject_->UpdateShadowMatrix(lightViewProjection);
	}

private:
	static std::unique_ptr<K4E::Object3D> CreateCube()
	{
		try
		{
			auto object = std::make_unique<K4E::Object3D>();
			object->Initialize("Sample/cube.gltf");
			object->SetPbrEnabled(true);
			object->SetMetallic(0.48f);
			object->SetRoughness(0.26f);
			object->SetReflectivity(0.62f);
			object->SetFrustumCullingEnabled(false);
			return object;
		}
		catch (...)
		{
			return nullptr; // Visual生成失敗時も防衛ObjectiveとDamage受付は継続する。
		}
	}

	void SyncVisuals()
	{
		const K4E::Vector3 position = GetWorldPosition();
		const float pulse = 0.5f + 0.5f * std::sin(visualTime_ * (2.8f + (1.0f - healthRatio_) * 4.0f));
		const float warning = std::clamp((0.45f - healthRatio_) / 0.45f, 0.0f, 1.0f);
		const float flash = damageFlashTimer_ > 0.0f ? 1.0f : 0.0f;

		if (baseObject_)
		{
			baseObject_->SetTranslate(position + K4E::Vector3{ 0.0f, 0.55f, 0.0f });
			baseObject_->SetScale({ 3.8f, 0.55f, 3.8f });
			baseObject_->SetColor(destroyed_
				? K4E::Vector4{ 0.12f, 0.10f, 0.10f, 1.0f }
				: K4E::Vector4{ 0.16f + flash * 0.25f, 0.20f, 0.25f, 1.0f });
			baseObject_->SetEmissiveFactor({ warning * 0.55f, 0.03f, 0.03f, 1.0f });
			baseObject_->Update();
		}

		if (armorObject_)
		{
			armorObject_->SetTranslate(position + K4E::Vector3{ 0.0f, destroyed_ ? 0.9f : 2.25f, 0.0f });
			armorObject_->SetRotate({ destroyed_ ? 0.34f : 0.0f, visualTime_ * (destroyed_ ? 0.0f : 0.18f), destroyed_ ? -0.22f : 0.0f });
			armorObject_->SetScale({ 2.15f, 1.65f, 2.15f });
			armorObject_->SetColor(destroyed_
				? K4E::Vector4{ 0.18f, 0.12f, 0.10f, 1.0f }
				: K4E::Vector4{ 0.22f + flash * 0.35f, 0.32f * healthRatio_, 0.42f * healthRatio_, 1.0f });
			armorObject_->SetEmissiveFactor({ warning * (1.2f + pulse), 0.08f, 0.05f, 1.0f });
			armorObject_->Update();
		}

		if (coreObject_)
		{
			const float coreScale = destroyed_ ? 0.20f : 0.72f + pulse * 0.12f;
			coreObject_->SetTranslate(position + K4E::Vector3{ 0.0f, destroyed_ ? 0.65f : 4.25f, 0.0f });
			coreObject_->SetRotate({ visualTime_ * 0.38f, visualTime_ * 0.82f, visualTime_ * 0.24f });
			coreObject_->SetScale({ coreScale, coreScale, coreScale });
			coreObject_->SetColor(destroyed_
				? K4E::Vector4{ 0.25f, 0.08f, 0.04f, 0.35f }
				: K4E::Vector4{ 0.20f + warning * 0.80f, 0.72f * healthRatio_, 1.0f * healthRatio_, 0.95f });
			coreObject_->SetEmissiveFactor(destroyed_
				? K4E::Vector4{ 0.45f, 0.05f, 0.01f, 1.0f }
				: K4E::Vector4{ 0.25f + warning * 2.4f, 1.6f * healthRatio_, 3.4f * healthRatio_ + pulse, 1.0f });
			coreObject_->Update();
		}
	}

	std::unique_ptr<K4E::Object3D> baseObject_;
	std::unique_ptr<K4E::Object3D> armorObject_;
	std::unique_ptr<K4E::Object3D> coreObject_;
	float visualTime_ = 0.0f;
	float healthRatio_ = 1.0f;
	float damageFlashTimer_ = 0.0f;
	bool destroyed_ = false;
};

/// 通常Enemyが共通Character攻撃経路でDamageを与えられるStage 3専用防衛対象。
class Stage3DefenseTargetActor final : public K4E::CharacterActor
{
public:
	std::string GetClassTypeName() const override { return "Stage3DefenseTargetActor"; }

	void Initialize() override
	{
		K4E::SceneComponent* root = GetRootComponent();
		if (!root)
		{
			root = &CreateRootComponent<K4E::SceneComponent>();
			root->SetName("Stage 3 Defense Root");
			root->SetUpdateOrder(-100);
		}
		if (!GetComponent<Stage3DefenseTargetVisualComponent>())
		{
			auto& visual = AddComponent<Stage3DefenseTargetVisualComponent>();
			visual.SetName("Stage 3 Defense Visual");
			visual.SetUpdateOrder(-30);
			visual.SetDrawOrder(0);
			visual.AttachTo(root);
		}

		K4E::CharacterActor::Initialize();
		visual_ = GetComponent<Stage3DefenseTargetVisualComponent>();
		if (K4E::CharacterHealthComponent* health = GetHealthComponent()) health->ResetHealth(maxHealth_);
		if (K4E::CharacterMovementComponent* movement = GetMovementComponent()) movement->SetMovementEnabled(false);
		if (K4E::CharacterTargetComponent* target = GetTargetComponent()) target->SetLocalPosition({ 0.0f, 2.6f, 0.0f });
		if (K4E::CharacterColliderComponent* collider = GetColliderComponent()) collider->SetHalfSize({ 2.4f, 2.6f, 2.4f });
		SetName("Stage3DefenseCore");
		SetLayer("StageObjective");
		AddTag("DefenseTarget");
		SyncVisualState();
	}

	void Update(float deltaTime) override
	{
		SyncVisualState();
		K4E::Actor::Update(deltaTime);
	}

	void Configure(const K4E::Vector3& position, float maxHealth)
	{
		maxHealth_ = std::max(1.0f, maxHealth);
		if (K4E::CharacterHealthComponent* health = GetHealthComponent()) health->ResetHealth(maxHealth_);
		if (K4E::SceneComponent* root = GetRootComponent())
		{
			root->SetLocalPosition(position);
			root->RefreshWorldTransform();
		}
		SyncVisualState();
	}

	void UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection)
	{
		if (visual_) visual_->UpdateShadowMatrix(lightViewProjection);
	}

	float GetHP() const
	{
		const K4E::CharacterHealthComponent* health = GetHealthComponent();
		return health ? health->GetCurrentHealth() : 0.0f;
	}

	float GetMaxHP() const
	{
		const K4E::CharacterHealthComponent* health = GetHealthComponent();
		return health ? health->GetMaxHealth() : maxHealth_;
	}

	float GetHealthRatio() const
	{
		const K4E::CharacterHealthComponent* health = GetHealthComponent();
		return health ? health->GetHealthRatio() : 0.0f;
	}

protected:
	void OnDeath(const K4E::CharacterDeathEvent& deathEvent) override
	{
		(void)deathEvent;
		if (K4E::CharacterMovementComponent* movement = GetMovementComponent()) movement->Stop();
		if (visual_) visual_->SetDestroyed(true); // HP0成立時にコア崩壊色へ切り替え、Objective失敗通知はRuntimeが一度だけ送る。
	}

private:
	void SyncVisualState()
	{
		if (!visual_) return;
		visual_->SetHealthRatio(GetHealthRatio());
		visual_->SetDestroyed(IsDead());
	}

	Stage3DefenseTargetVisualComponent* visual_ = nullptr;
	float maxHealth_ = 2200.0f;
};

/// Stage 3の防衛コア生成、Enemy再Target、時間経過による三方向増援を管理する。
class Stage3DefenseRuntime final
{
public:
	struct PromptSnapshot
	{
		bool visible = false;
		std::string text;
		float normalizedProgress = 0.0f;
	};

	static constexpr const char* GetDestroyedEventId() { return "__Stage3DefenseTargetDestroyed"; }

	void Initialize(const GamePlayStageContext& stageContext)
	{
		Finalize();
		const auto rule = stageContext.GetCurrentStageRule();
		active_ = rule.objectiveType == GamePlayStageContext::StageObjectiveType::DefendTarget;
		if (!active_) return;
		if (!stageContext.GetDefenseTargetPoints().empty()) targetPosition_ = stageContext.GetDefenseTargetPoints().front().position;
		else targetPosition_ = { 0.0f, 0.0f, 0.0f };
		targetPosition_.y = std::max(0.0f, targetPosition_.y);
		ConfigureDefenseLighting();
	}

	void Finalize()
	{
		if (target_) target_->Destroy();
		target_ = nullptr;
		prompt_ = {};
		defenseElapsedSec_ = 0.0f;
		spawnTimer_ = 3.5f;
		retargetTimer_ = 0.0f;
		spawnSequence_ = 0u;
		destroyedEventSent_ = false;
		active_ = false;
	}

	void Update(
		IPlayerRuntime* player,
		CharacterWorld& characters,
		float deltaTime,
		const std::function<void(const std::string&)>& onEvent)
	{
		(void)player;
		prompt_ = {};
		if (!active_) return;
		SpawnTargetIfNeeded(characters.GetActorWorld());
		if (!target_) return;

		const float safeDeltaTime = std::max(0.0f, deltaTime);
		defenseElapsedSec_ += safeDeltaTime;
		retargetTimer_ -= safeDeltaTime;
		if (retargetTimer_ <= 0.0f)
		{
			RetargetEnemies(characters);
			retargetTimer_ = 0.35f;
		}

		if (target_->IsDead())
		{
			BuildPrompt();
			if (!destroyedEventSent_ && onEvent)
			{
				destroyedEventSent_ = true;
				onEvent(GetDestroyedEventId());
			}
			return;
		}

		spawnTimer_ -= safeDeltaTime;
		if (spawnTimer_ <= 0.0f)
		{
			SpawnReinforcementBatch(characters);
			spawnTimer_ = ResolveSpawnInterval();
		}
		BuildPrompt();
	}

	void UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection)
	{
		if (target_) target_->UpdateShadowMatrix(lightViewProjection);
	}

	bool IsActive() const { return active_; }
	Stage3DefenseTargetActor* GetTarget() const { return target_; }
	const PromptSnapshot& GetPromptSnapshot() const { return prompt_; }

private:
	static void ConfigureDefenseLighting()
	{
		auto* lightManager = K4E::LightManager::GetInstance();
		if (!lightManager) return;
		lightManager->ResetToDefaultLighting();

		auto& settings = lightManager->GetMutableLightingSettingsForEditor();
		settings.ambientColor = { 0.055f, 0.070f, 0.095f, 0.18f };
		settings.fogColor = { 0.070f, 0.090f, 0.125f, 1.0f };
		settings.exposure = 1.02f;
		settings.contrast = 1.08f;
		settings.fogStart = 55.0f;
		settings.fogEnd = 210.0f;
		settings.enableFog = 1u;

		auto& lights = lightManager->GetMutablePunctualLightsForEditor();
		lights.clear();
		K4E::LightManager::PunctualLightGPU directional{};
		directional.lightType = 1u;
		directional.color = { 0.52f, 0.62f, 0.78f, 1.0f };
		directional.intensity = 0.55f;
		directional.direction = { 0.32f, -0.90f, 0.25f };
		directional.enabled = 1u;
		lights.push_back(directional);

		auto addPoint = [&lights](const K4E::Vector3& position, const K4E::Vector4& color, float intensity, float radius)
		{
			K4E::LightManager::PunctualLightGPU light{};
			light.lightType = 2u;
			light.position = position;
			light.color = color;
			light.intensity = intensity;
			light.radius = radius;
			light.decay = 1.55f;
			light.enabled = 1u;
			lights.push_back(light);
		};
		addPoint({ 0.0f, 7.5f, 0.0f }, { 0.20f, 0.68f, 1.0f, 1.0f }, 1.85f, 22.0f);
		addPoint({ -22.0f, 6.0f, -10.0f }, { 1.0f, 0.52f, 0.20f, 1.0f }, 1.05f, 18.0f);
		addPoint({ 22.0f, 6.0f, -10.0f }, { 1.0f, 0.52f, 0.20f, 1.0f }, 1.05f, 18.0f);
		addPoint({ 0.0f, 6.0f, 25.0f }, { 1.0f, 0.34f, 0.18f, 1.0f }, 0.90f, 19.0f);
		lightManager->SetShadowCasterLightIndex(0);
		lightManager->SetManualShadowFocusPosition({ 0.0f, 2.0f, 0.0f });
		lightManager->SetDirectionalShadowFrustum(105.0f, 105.0f, 0.1f, 240.0f); // 中央コアを青く強調し、三方向の侵入口は暖色灯で識別できるようにする。
	}

	void SpawnTargetIfNeeded(K4E::ActorWorld& actorWorld)
	{
		if (target_) return;
		auto& target = actorWorld.SpawnActor<Stage3DefenseTargetActor>();
		target.Configure(targetPosition_, targetMaxHealth_);
		target.Update(0.0f);
		target_ = &target;
	}

	void RetargetEnemies(CharacterWorld& characters)
	{
		if (!target_ || target_->IsDead()) return;
		for (EnemyBase* enemyBase : characters.GetEnemyRawList())
		{
			auto* enemy = dynamic_cast<K4E::EnemyActor*>(enemyBase);
			if (!enemy || enemy->IsDead()) continue;
			enemy->SetTargetActor(target_); // WaveManager生成分と追加増援を同じ防衛コアへ収束させる。
		}
	}

	void SpawnReinforcementBatch(CharacterWorld& characters)
	{
		if (!target_ || target_->IsDead()) return;
		const int phase = defenseElapsedSec_ >= 60.0f ? 2 : (defenseElapsedSec_ >= 30.0f ? 1 : 0);
		const int aliveCap = phase == 0 ? 12 : (phase == 1 ? 16 : 20);
		const int aliveCount = characters.GetAliveNormalEnemyCount();
		if (aliveCount >= aliveCap) return;

		const std::array<K4E::Vector3, 3> laneOffsets = {{
			{ 0.0f, 2.0f, 44.0f },
			{ -42.0f, 2.0f, -23.0f },
			{ 42.0f, 2.0f, -23.0f }
		}};
		const K4E::Vector3 laneCenter = targetPosition_ + laneOffsets[spawnSequence_ % laneOffsets.size()];
		const int requestedCount = phase == 0 ? 2 : (phase == 1 ? 3 : 4);
		const int spawnCount = std::min(requestedCount, std::max(0, aliveCap - aliveCount));

		for (int index = 0; index < spawnCount; ++index)
		{
			const float side = static_cast<float>(index - (spawnCount - 1) / 2) * 2.2f;
			const K4E::Vector3 spread = spawnSequence_ % 3u == 0u
				? K4E::Vector3{ side, 0.0f, static_cast<float>(index % 2) * 1.6f }
				: K4E::Vector3{ static_cast<float>(index % 2) * 1.6f, 0.0f, side };
			const bool useMidRange = phase > 0 && index == spawnCount - 1;
			EnemyBase& spawned = characters.SpawnEnemyAt(laneCenter + spread, useMidRange ? EnemyType::MidRange : EnemyType::Melee);
			if (auto* enemy = dynamic_cast<K4E::EnemyActor*>(&spawned)) enemy->SetTargetActor(target_);
		}
		++spawnSequence_;
	}

	float ResolveSpawnInterval() const
	{
		if (defenseElapsedSec_ >= 60.0f) return 4.2f;
		if (defenseElapsedSec_ >= 30.0f) return 5.0f;
		return 6.0f;
	}

	void BuildPrompt()
	{
		if (!target_) return;
		char text[96]{};
		if (target_->IsDead())
		{
			prompt_.visible = true;
			prompt_.text = "防衛コアが破壊された";
			prompt_.normalizedProgress = 0.0f;
			return;
		}
		std::snprintf(text, sizeof(text), "防衛コア HP %.0f / %.0f", target_->GetHP(), target_->GetMaxHP());
		prompt_.visible = true;
		prompt_.text = text;
		prompt_.normalizedProgress = target_->GetHealthRatio();
	}

	Stage3DefenseTargetActor* target_ = nullptr;
	PromptSnapshot prompt_{};
	K4E::Vector3 targetPosition_{};
	float targetMaxHealth_ = 2200.0f;
	float defenseElapsedSec_ = 0.0f;
	float spawnTimer_ = 3.5f;
	float retargetTimer_ = 0.0f;
	std::size_t spawnSequence_ = 0u;
	bool destroyedEventSent_ = false;
	bool active_ = false;
};
