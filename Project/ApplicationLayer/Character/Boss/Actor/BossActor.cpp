#include "BossActor.h"

#include "BossActorAttackComponent.h"
#include "BossBrainComponent.h"
#include "BossPresentationComponent.h"
#include "ApplicationLayer/Character/Boss/Components/BossPhaseComponent.h"
#include "ApplicationLayer/Character/Boss/Components/BossWeakPointComponent.h"

#include <PhysicsCollisionLayer.h>
#include <RigidbodyComponent.h>
#include <Scene/Actor/Character/CharacterAnimationComponent.h>
#include <Scene/Actor/Character/CharacterColliderComponent.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>
#include <Scene/Actor/Character/CharacterMovementComponent.h>
#include <Scene/Actor/Character/HumanoidVisualComponent.h>
#include <SceneComponent.h>

namespace Ken4lowEngine
{
	void BossActor::Initialize()
	{
		SceneComponent* root = GetRootComponent();
		if (!root)
		{
			root = &CreateRootComponent<SceneComponent>();
			root->SetName("Boss Root");
			root->SetUpdateOrder(-100);
		}

		auto* phase = GetBossPhaseComponent();
		if (!phase)
		{
			phase = &AddComponent<BossPhaseComponent>();
			phase->SetName("Boss Phase");
			phase->SetUpdateOrder(-98);
		}

		auto* presentation = GetBossPresentationComponent();
		if (!presentation)
		{
			presentation = &AddComponent<BossPresentationComponent>();
			presentation->SetName("Boss Presentation");
			presentation->SetUpdateOrder(-97);
		}

		if (!GetBossBrainComponent())
		{
			auto& brain = AddComponent<BossBrainComponent>();
			brain.SetName("Boss Brain");
			brain.SetUpdateOrder(-95);
		}

		if (!GetBossAttackComponent())
		{
			auto& attack = AddComponent<BossAttackComponent>();
			attack.SetName("Boss Attack");
			attack.SetUpdateOrder(-92); // Chargeの速度出力を同フレームの共通Movementより先に確定する。
		}

		if (!GetComponent<RigidbodyComponent>())
		{
			auto& rigidbody = AddComponent<RigidbodyComponent>();
			rigidbody.SetName("Boss Rigidbody");
			rigidbody.SetUpdateOrder(-85);
			rigidbody.SetBodyType(BodyType::Dynamic);
			rigidbody.SetMass(4.0f);
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
			visual->SetName("Boss Humanoid Visual");
			visual->SetUpdateOrder(0);
			visual->SetDrawOrder(0);
			visual->SetCastShadowEnabled(true);
			visual->SetLocalScale({ 1.5f, 1.5f, 1.5f });
			visual->AttachTo(root);
		}

		if (!GetBossWeakPointComponent())
		{
			auto& weakPoint = AddComponent<BossWeakPointComponent>();
			weakPoint.SetName("Boss Weak Point");
			weakPoint.SetUpdateOrder(-40);
		}

		const bool hadHealth = GetHealthComponent() != nullptr;
		const bool hadCollider = GetColliderComponent() != nullptr;
		CharacterActor::Initialize();

		if (CharacterMovementComponent* movement = GetMovementComponent()) movement->SetUpdateOrder(-90);
		if (CharacterAnimationComponent* animation = GetAnimationComponent()) animation->SetUpdateOrder(-70);
		if (!hadHealth)
		{
			if (CharacterHealthComponent* health = GetHealthComponent()) health->ResetHealth(1200.0f);
		}
		if (!hadCollider)
		{
			if (CharacterColliderComponent* collider = GetColliderComponent())
			{
				collider->SetHalfSize({ 1.5f, 3.0f, 1.5f });
				collider->SetCollisionLayer(PhysicsCollisionLayer::DynamicActor);
			}
		}
		if (visual && visual->GetSkinTexturePath().empty()) visual->ApplySkinToAllParts("Characters/enemy.dds");
	}

	void BossActor::SetTargetActor(CharacterActor* targetActor)
	{
		if (BossBrainComponent* brain = GetBossBrainComponent()) brain->SetTargetActor(targetActor);
		if (BossAttackComponent* attack = GetBossAttackComponent()) attack->SetTargetActor(targetActor);
	}

	CharacterDamageResult BossActor::ApplyWeakPointDamage(std::string_view partId, float baseDamage)
	{
		if (BossWeakPointComponent* weakPoint = GetBossWeakPointComponent()) return weakPoint->ApplyDamageToPart(partId, baseDamage);
		return {};
	}

	void BossActor::ResetForValidation(const Vector3& worldPosition)
	{
		SetActive(true);
		if (SceneComponent* root = GetRootComponent())
		{
			root->SetLocalPosition(worldPosition);
			root->SetLocalRotation({});
			root->RefreshWorldTransform();
		}
		if (CharacterHealthComponent* health = GetHealthComponent()) health->ResetHealth(1200.0f);
		if (CharacterMovementComponent* movement = GetMovementComponent()) movement->Stop();
		if (RigidbodyComponent* rigidbody = GetComponent<RigidbodyComponent>()) rigidbody->SetVelocity({}); // 再配置時に以前のCharge・落下速度を持ち越さない。
		if (CharacterColliderComponent* collider = GetColliderComponent()) collider->SetActive(true);
		if (BossPhaseComponent* phase = GetBossPhaseComponent()) phase->ResetPhase();
		if (BossPresentationComponent* presentation = GetBossPresentationComponent()) presentation->ResetPresentation();
		if (BossBrainComponent* brain = GetBossBrainComponent()) brain->ResetBehavior();
		if (BossAttackComponent* attack = GetBossAttackComponent()) attack->ResetAttackState();
		if (CharacterAnimationComponent* animation = GetAnimationComponent()) animation->Play("Idle", 1.5f, true);
	}

	BossBrainComponent* BossActor::GetBossBrainComponent() { return GetCharacterComponent<BossBrainComponent>(); }
	BossAttackComponent* BossActor::GetBossAttackComponent() { return GetCharacterComponent<BossAttackComponent>(); }
	BossPhaseComponent* BossActor::GetBossPhaseComponent() { return GetCharacterComponent<BossPhaseComponent>(); }
	BossWeakPointComponent* BossActor::GetBossWeakPointComponent() { return GetCharacterComponent<BossWeakPointComponent>(); }
	BossPresentationComponent* BossActor::GetBossPresentationComponent() { return GetCharacterComponent<BossPresentationComponent>(); }
	HumanoidVisualComponent* BossActor::GetHumanoidVisualComponent() { return GetCharacterComponent<HumanoidVisualComponent>(); }

	void BossActor::OnDeath(const CharacterDeathEvent& deathEvent)
	{
		(void)deathEvent;
		if (BossBrainComponent* brain = GetBossBrainComponent()) brain->StopBehavior();
		if (BossAttackComponent* attack = GetBossAttackComponent()) attack->SetAttackEnabled(false);
		if (CharacterMovementComponent* movement = GetMovementComponent()) movement->Stop();
		if (RigidbodyComponent* rigidbody = GetComponent<RigidbodyComponent>()) rigidbody->SetVelocity({});
		if (CharacterColliderComponent* collider = GetColliderComponent()) collider->SetActive(false);
		if (BossPresentationComponent* presentation = GetBossPresentationComponent()) presentation->StartDeathPresentation();
	}
} // namespace Ken4lowEngine
