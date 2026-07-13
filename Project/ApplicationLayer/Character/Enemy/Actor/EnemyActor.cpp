#include "EnemyActor.h"

#include "EnemyAIComponent.h"
#include "EnemyAttackComponent.h"
#include "EnemyEffectComponent.h"

#include <PhysicsCollisionLayer.h>
#include <Scene/Actor/Character/CharacterColliderComponent.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>
#include <Scene/Actor/Character/CharacterMovementComponent.h>
#include <Scene/Actor/Character/HumanoidVisualComponent.h>
#include <SceneComponent.h>

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
			ai->SetUpdateOrder(-95); // AIは速度を決めるだけとし、位置積分より先に一度だけ実行する。
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

		CharacterActor::Initialize();

		if (CharacterHealthComponent* health = GetHealthComponent()) health->ResetHealth(240.0f);
		if (CharacterColliderComponent* collider = GetColliderComponent())
		{
			collider->SetHalfSize({ 1.0f, 2.0f, 1.0f });
			collider->SetCollisionLayer(PhysicsCollisionLayer::DynamicActor);
		}
		if (visual) visual->ApplySkinToAllParts("Characters/enemy.dds");
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
		if (CharacterColliderComponent* collider = GetColliderComponent()) collider->SetActive(true);
		if (EnemyAIComponent* ai = GetEnemyAIComponent()) ai->ResetBehavior();
		if (EnemyAttackComponent* attack = GetEnemyAttackComponent()) attack->ResetAttackState();
		if (EnemyEffectComponent* effect = GetEnemyEffectComponent()) effect->ResetEffectState();
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

	void EnemyActor::OnDeath(const CharacterDeathEvent& deathEvent)
	{
		(void)deathEvent;
		if (EnemyAIComponent* ai = GetEnemyAIComponent()) ai->StopBehavior();
		if (EnemyAttackComponent* attack = GetEnemyAttackComponent()) attack->StopAttacking();
		if (CharacterMovementComponent* movement = GetMovementComponent()) movement->Stop();
		if (CharacterColliderComponent* collider = GetColliderComponent()) collider->SetActive(false);

		const SceneComponent* root = GetRootComponent();
		if (EnemyEffectComponent* effect = GetEnemyEffectComponent()) effect->TriggerDeathEffect(root ? root->GetWorldPosition() : Vector3{});
	}
} // namespace Ken4lowEngine
