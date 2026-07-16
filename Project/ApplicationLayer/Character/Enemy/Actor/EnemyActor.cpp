#include "EnemyActor.h"

#include "EnemyAIComponent.h"
#include "EnemyAttackComponent.h"
#include "EnemyEffectComponent.h"

#include <Collider.h>
#include <CollisionPreset.h>
#include <RigidbodyComponent.h>
#include <Scene/Actor/Character/CharacterColliderComponent.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>
#include <Scene/Actor/Character/CharacterMovementComponent.h>
#include <Scene/Actor/Character/HumanoidVisualComponent.h>
#include <SceneComponent.h>
#include <WorldGaugeComponent.h>

#include <algorithm>
#include <cmath>

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

		if (!GetEnemyAIComponent())
		{
			auto& ai = AddComponent<EnemyAIComponent>();
			ai.SetName("Enemy AI");
			ai.SetUpdateOrder(-95); // AIは移動要求を決め、EnemyBaseの地形解決へ同じ速度を渡す。
		}

		HumanoidVisualComponent* visual = GetHumanoidVisualComponent();
		if (visual)
		{
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
			healthGauge.SetVisible(false);
			healthGauge.AttachTo(root);
		}

		SetMaxHp(160);
		EnemyBase::Initialize(); // Collision、地形補正、被弾、死亡演出は既存本番経路を維持する。
		if (CharacterMovementComponent* movement = GetMovementComponent()) movement->SetMovementEnabled(false);
		runtimeStateInitialized_ = true;
		if (visual && visual->GetSkinTexturePath().empty()) visual->ApplySkinToAllParts("Characters/enemy.dds");
		ApplyPendingRuntimeBindings();
		SetHealthBarVisible(false);
		SyncHealthGauge();
	}

	void EnemyActor::Update(float deltaTime)
	{
		EnsureRuntimeStateInitialized();
		if (!GetComponent<RigidbodyComponent>())
		{
			if (const CharacterMovementComponent* movement = GetMovementComponent())
			{
				const Vector3 desiredVelocity = movement->GetVelocity();
				velocity_.x = desiredVelocity.x;
				velocity_.z = desiredVelocity.z; // Componentの要求速度を旧地形解決へ渡し、Root直接移動との二重適用を避ける。
			}
		}
		EnemyBase::Update(deltaTime);
		if (!IsDead())
		{
			if (const SceneComponent* root = GetRootComponent()) orientation_ = root->GetWorldRotation();
		}
		SyncHealthGauge();
	}

	void EnemyActor::ApplyDirectorDifficulty(float moveSpeedMultiplier, float attackCooldownMultiplier, float damageMultiplier)
	{
		EnsureRuntimeStateInitialized();
		if (EnemyAIComponent* ai = GetEnemyAIComponent()) ai->ApplyMoveSpeedMultiplier(moveSpeedMultiplier);
		if (EnemyAttackComponent* attack = GetEnemyAttackComponent()) attack->ApplyDifficultyMultipliers(attackCooldownMultiplier, damageMultiplier);
	}

	void EnemyActor::SetTargetActor(CharacterActor* targetActor)
	{
		targetActor_ = targetActor;
		if (EnemyAIComponent* ai = GetEnemyAIComponent()) ai->SetTargetActor(targetActor_);
		if (EnemyAttackComponent* attack = GetEnemyAttackComponent()) attack->SetTargetActor(targetActor_);
	}

	void EnemyActor::SetNavigationObstacles(const std::vector<AABB>* obstacles)
	{
		navigationObstacles_ = obstacles;
		if (EnemyAIComponent* ai = GetEnemyAIComponent()) ai->SetNavigationObstacles(navigationObstacles_);
	}

	CharacterDamageResult EnemyActor::ApplyComparisonDamage(float amount)
	{
		EnsureRuntimeStateInitialized();
		CharacterDamageResult result{};
		result.requestedDamage = amount;
		result.healthBefore = static_cast<float>(GetHp());
		const bool wasDead = IsDead();
		if (std::isfinite(amount) && amount > 0.0f)
		{
			TakeDamage(std::max(1, static_cast<int>(std::round(amount)))); // Debug比較も本番のEnemyBase Damage経路だけを使用する。
		}
		result.healthAfter = static_cast<float>(GetHp());
		result.appliedDamage = std::max(0.0f, result.healthBefore - result.healthAfter);
		result.accepted = result.appliedDamage > 0.0f;
		result.killed = !wasDead && IsDead();
		if (result.accepted)
		{
			if (EnemyEffectComponent* effect = GetEnemyEffectComponent()) effect->TriggerHitEffect(GetCenterPosition());
		}
		SyncHealthGauge();
		return result;
	}

	void EnemyActor::ResetForComparison(const Vector3& worldPosition)
	{
		EnsureRuntimeStateInitialized();
		SetActive(true);
		isDead_ = false;
		removable_ = false;
		deathBreakActive_ = false;
		deathBreakInitialized_ = false;
		hasDeathEffectOrigin_ = false;
		hasDeathPartWorldTransforms_ = false;
		deathTimer_ = 0.0f;
		deathPieces_.clear();
		velocity_ = {};
		orientation_ = {};
		hitFlashTimer_ = 0.0f;
		SetPosition(worldPosition);
		SetCurrentHp(160);
		SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		SetAllPartsActive(true);
		if (CharacterMovementComponent* movement = GetMovementComponent())
		{
			movement->Stop();
			movement->SetMovementEnabled(GetComponent<RigidbodyComponent>() != nullptr);
		}
		if (CharacterColliderComponent* collider = GetColliderComponent()) collider->SetActive(true);
		if (Collider* primitive = GetCollisionPrimitive()) primitive->SetEnabled(true);
		if (EnemyAIComponent* ai = GetEnemyAIComponent()) ai->ResetBehavior();
		if (EnemyAttackComponent* attack = GetEnemyAttackComponent())
		{
			attack->SetAttackEnabled(true);
			attack->ResetAttackState();
		}
		if (EnemyEffectComponent* effect = GetEnemyEffectComponent()) effect->ResetEffectState();
		ApplyPendingRuntimeBindings();
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

	const EnemyAIComponent* EnemyActor::GetEnemyAIComponent() const
	{
		return GetCharacterComponent<EnemyAIComponent>();
	}

	EnemyAttackComponent* EnemyActor::GetEnemyAttackComponent()
	{
		return GetCharacterComponent<EnemyAttackComponent>();
	}

	const EnemyAttackComponent* EnemyActor::GetEnemyAttackComponent() const
	{
		return GetCharacterComponent<EnemyAttackComponent>();
	}

	EnemyEffectComponent* EnemyActor::GetEnemyEffectComponent()
	{
		return GetCharacterComponent<EnemyEffectComponent>();
	}

	const EnemyEffectComponent* EnemyActor::GetEnemyEffectComponent() const
	{
		return GetCharacterComponent<EnemyEffectComponent>();
	}

	HumanoidVisualComponent* EnemyActor::GetHumanoidVisualComponent()
	{
		return GetCharacterComponent<HumanoidVisualComponent>();
	}

	const HumanoidVisualComponent* EnemyActor::GetHumanoidVisualComponent() const
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

	const WorldGaugeComponent* EnemyActor::GetHealthGaugeComponent() const
	{
		const auto gauges = GetComponents<WorldGaugeComponent>();
		for (const WorldGaugeComponent* gauge : gauges)
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

	void EnemyActor::ApplyPendingRuntimeBindings()
	{
		SetTargetActor(targetActor_);
		SetNavigationObstacles(navigationObstacles_);
	}

	void EnemyActor::EnsureRuntimeStateInitialized()
	{
		if (runtimeStateInitialized_) return;
		if (CharacterHealthComponent* health = GetHealthComponent())
		{
			configuredMaxHp_ = std::max(1, static_cast<int>(std::round(health->GetMaxHealth())));
			isDead_ = !health->IsAlive();
		}
		else
		{
			configuredMaxHp_ = 160;
			isDead_ = false;
		}

		removable_ = false;
		deathBreakActive_ = false;
		deathBreakInitialized_ = false;
		hasDeathEffectOrigin_ = false;
		hasDeathPartWorldTransforms_ = false;
		deathTimer_ = 0.0f;
		deathPieces_.clear();
		hitFlashTimer_ = 0.0f;
		velocity_ = {};
		const bool usesPhysicsBody = GetComponent<RigidbodyComponent>() != nullptr;
		useGravity_ = !usesPhysicsBody;
		useWorldResolve_ = !usesPhysicsBody;
		if (CharacterMovementComponent* movement = GetMovementComponent()) movement->SetMovementEnabled(usesPhysicsBody);
		if (CharacterColliderComponent* collider = GetColliderComponent())
		{
			obbHalf_ = collider->GetHalfSize();
			worldCol_.half = obbHalf_;
			worldCol_.centerOffset = collider->GetLocalPosition();
			worldColOverride_ = true;
		}
		ApplyCollisionPreset(*this, ECollisionPresetId::Enemy);
		if (Collider* primitive = GetCollisionPrimitive()) primitive->SetOwner(this);
		spawnPosition_ = GetCenterPosition();
		lastSafePosition_ = spawnPosition_;
		orientation_ = GetOrientation();
		SetColor(baseColor_);
		ApplyPendingRuntimeBindings();
		runtimeStateInitialized_ = true; // Prefab生成ではActor固有Initializeが呼ばれないため、最初の利用時に一度だけ補完する。
		SyncHealthGauge();
	}

	void EnemyActor::OnDeath(const CharacterDeathEvent& deathEvent)
	{
		(void)deathEvent;
		if (EnemyAIComponent* ai = GetEnemyAIComponent()) ai->StopBehavior();
		if (EnemyAttackComponent* attack = GetEnemyAttackComponent()) attack->StopAttacking();
		if (CharacterMovementComponent* movement = GetMovementComponent()) movement->Stop();
		SetHealthBarVisible(false);
		SyncHealthGauge(); // Collider停止と部位死亡演出は直後のEnemyBase::TakeDamageへ一元化する。
	}
} // namespace Ken4lowEngine
