#include "EnemyActor.h"

#include "EnemyAIComponent.h"
#include "EnemyAttackComponent.h"
#include "EnemyEffectComponent.h"

#include <PhysicsCollisionLayer.h>
#include <RigidbodyComponent.h>
#include <Scene/Actor/Character/CharacterColliderComponent.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>
#include <Scene/Actor/Character/CharacterMovementComponent.h>
#include <Scene/Actor/Character/HumanoidVisualComponent.h>
#include <SceneComponent.h>
#include <WorldGaugeComponent.h>

namespace Ken4lowEngine
{
	void EnemyActor::Initialize()
	{
		SceneComponent* root = GetRootComponent();
		if (!root)
		{
			root = &CreateRootComponent<SceneComponent>();
			root->SetName("Enemy Root");
			root->SetUpdateOrder(-100);
		}

		auto* ai = GetEnemyAIComponent();
		if (!ai)
		{
			ai = &AddComponent<EnemyAIComponent>();
			ai->SetName("Enemy AI");
			ai->SetUpdateOrder(-95); // AIは移動要求を決め、CharacterMovementが同フレームでPhysics速度へ変換する。
		}

		if (!GetComponent<RigidbodyComponent>())
		{
			auto& rigidbody = AddComponent<RigidbodyComponent>();
			rigidbody.SetName("Enemy Rigidbody");
			rigidbody.SetUpdateOrder(-85);
			rigidbody.SetBodyType(BodyType::Dynamic);
			rigidbody.SetMass(1.0f);
			rigidbody.SetUseGravity(true);
			rigidbody.SetSleepEnabled(false);
			rigidbody.SetRestitution(0.0f);
			rigidbody.SetStaticFriction(0.0f);
			rigidbody.SetDynamicFriction(0.0f);
		}

		auto* visual = GetHumanoidVisualComponent();
		if (!visual)
		{
			visual = &AddComponent<HumanoidVisualComponent>();
			visual->SetName("Enemy Humanoid Visual");
			visual->SetUpdateOrder(0);
			visual->SetDrawOrder(0);
			visual->SetCastShadowEnabled(true);
			visual->AttachTo(root);
		}

		if (!GetEnemyAttackComponent())
		{
			auto& attack = AddComponent<EnemyAttackComponent>();
			attack.SetName("Enemy Attack");
			attack.SetUpdateOrder(-80);
		}

		if (!GetEnemyEffectComponent())
		{
			auto& effect = AddComponent<EnemyEffectComponent>();
			effect.SetName("Enemy Effect");
			effect.SetUpdateOrder(20);
		}

		if (!GetHealthGaugeComponent())
		{
			auto& healthGauge = AddComponent<WorldGaugeComponent>();
			healthGauge.SetName("Enemy HP Gauge");
			healthGauge.SetDrawOrder(120);
			healthGauge.SetLocalPosition({ 0.0f, 2.65f, 0.0f });
			healthGauge.SetSize({ 190.0f, 16.0f });
			healthGauge.SetScreenOffset({ 0.0f, -8.0f });
			healthGauge.SetBackgroundColor({ 0.05f, 0.05f, 0.05f, 0.86f });
			healthGauge.SetFillColor({ 0.90f, 0.58f, 0.16f, 1.0f });
			healthGauge.SetBorderColor({ 1.0f, 1.0f, 1.0f, 0.90f });
			healthGauge.SetBorderThickness(2.0f);
			healthGauge.SetVisible(false); // 通常EnemyはPlayerの照準が合った時だけ表示する。
			healthGauge.AttachTo(root);
		}

		const bool hadHealth = GetHealthComponent() != nullptr;
		const bool hadCollider = GetColliderComponent() != nullptr;
		CharacterActor::Initialize();

		if (!hadHealth)
		{
			if (CharacterHealthComponent* health = GetHealthComponent()) health->ResetHealth(240.0f);
		}
		if (!hadCollider)
		{
			if (CharacterColliderComponent* collider = GetColliderComponent())
			{
				collider->SetHalfSize({ 1.0f, 2.0f, 1.0f });
				collider->SetCollisionLayer(PhysicsCollisionLayer::DynamicActor);
			}
		}
		if (visual && visual->GetSkinTexturePath().empty()) visual->ApplySkinToAllParts("Characters/enemy.dds");
		SyncHealthGauge();
	}

	void EnemyActor::Update(float deltaTime)
	{
		CharacterActor::Update(deltaTime);
		SyncHealthGauge(); // ゲーム描画用WorldGaugeへHealth Componentの値を同期する。
	}

	void EnemyActor::SetTargetActor(CharacterActor* targetActor)
	{
		if (EnemyAIComponent* ai = GetEnemyAIComponent()) ai->SetTargetActor(targetActor);
		if (EnemyAttackComponent* attack = GetEnemyAttackComponent()) attack->SetTargetActor(targetActor);
	}

	void EnemyActor::SetNavigationObstacles(const std::vector<AABB>* obstacles)
	{
		if (EnemyAIComponent* ai = GetEnemyAIComponent()) ai->SetNavigationObstacles(obstacles);
	}

	CharacterDamageResult EnemyActor::ApplyComparisonDamage(float amount)
	{
		const CharacterDamageResult result = ApplyDamage(amount);
		if (result.accepted)
		{
			const SceneComponent* root = GetRootComponent();
			if (EnemyEffectComponent* effect = GetEnemyEffectComponent()) effect->TriggerHitEffect(root ? root->GetWorldPosition() : Vector3{});
			SyncHealthGauge();
		}
		return result;
	}

	void EnemyActor::ResetForComparison(const Vector3& worldPosition)
	{
		SetActive(true);
		if (SceneComponent* root = GetRootComponent())
		{
			root->SetLocalPosition(worldPosition);
			root->RefreshWorldTransform();
		}
		if (CharacterHealthComponent* health = GetHealthComponent()) health->ResetHealth(240.0f);
		if (CharacterMovementComponent* movement = GetMovementComponent()) movement->Stop();
		if (RigidbodyComponent* rigidbody = GetComponent<RigidbodyComponent>()) rigidbody->SetVelocity({}); // 再配置時に以前の落下・追跡速度を持ち越さない。
		if (CharacterColliderComponent* collider = GetColliderComponent()) collider->SetActive(true);
		if (EnemyAIComponent* ai = GetEnemyAIComponent()) ai->ResetBehavior();
		if (EnemyAttackComponent* attack = GetEnemyAttackComponent()) attack->ResetAttackState();
		if (EnemyEffectComponent* effect = GetEnemyEffectComponent()) effect->ResetEffectState();
		SetHealthBarVisible(false);
		SyncHealthGauge();
	}

	void EnemyActor::SetHealthBarVisible(bool visible)
	{
		if (WorldGaugeComponent* gauge = GetHealthGaugeComponent()) gauge->SetVisible(visible && !IsDead());
	}

	EnemyAIComponent* EnemyActor::GetEnemyAIComponent()
	{
		return GetCharacterComponent<EnemyAIComponent>();
	}

	EnemyAttackComponent* EnemyActor::GetEnemyAttackComponent()
	{
		return GetCharacterComponent<EnemyAttackComponent>();
	}

	EnemyEffectComponent* EnemyActor::GetEnemyEffectComponent()
	{
		return GetCharacterComponent<EnemyEffectComponent>();
	}

	HumanoidVisualComponent* EnemyActor::GetHumanoidVisualComponent()
	{
		return GetCharacterComponent<HumanoidVisualComponent>();
	}

	WorldGaugeComponent* EnemyActor::GetHealthGaugeComponent()
	{
		for (WorldGaugeComponent* gauge : GetComponents<WorldGaugeComponent>())
		{
			if (gauge && gauge->GetName() == "Enemy HP Gauge") return gauge;
		}
		return nullptr;
	}

	void EnemyActor::SyncHealthGauge()
	{
		CharacterHealthComponent* health = GetHealthComponent();
		WorldGaugeComponent* gauge = GetHealthGaugeComponent();
		if (!health || !gauge) return;
		gauge->SetMaxValue(health->GetMaxHealth());
		gauge->SetValue(health->GetCurrentHealth());
	}

	void EnemyActor::OnDeath(const CharacterDeathEvent& deathEvent)
	{
		(void)deathEvent;
		if (EnemyAIComponent* ai = GetEnemyAIComponent()) ai->StopBehavior();
		if (EnemyAttackComponent* attack = GetEnemyAttackComponent()) attack->StopAttacking();
		if (CharacterMovementComponent* movement = GetMovementComponent()) movement->Stop();
		if (RigidbodyComponent* rigidbody = GetComponent<RigidbodyComponent>()) rigidbody->SetVelocity({});
		if (CharacterColliderComponent* collider = GetColliderComponent()) collider->SetActive(false);
		SetHealthBarVisible(false);
		SyncHealthGauge();

		const SceneComponent* root = GetRootComponent();
		if (EnemyEffectComponent* effect = GetEnemyEffectComponent()) effect->TriggerDeathEffect(root ? root->GetWorldPosition() : Vector3{});
	}
} // namespace Ken4lowEngine
