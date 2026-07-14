#pragma once

#include "InventoryComponent.h"
#include "PlayerCameraComponent.h"
#include "PlayerInputComponent.h"
#include "PlayerMovementComponent.h"
#include "WeaponComponent.h"

#include <ModelComponent.h>
#include <PhysicsCollisionLayer.h>
#include <RigidbodyComponent.h>
#include <Scene/Actor/Character/CharacterActor.h>
#include <Scene/Actor/Character/CharacterAnimationComponent.h>
#include <Scene/Actor/Character/CharacterColliderComponent.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>
#include <Scene/Actor/Character/HumanoidVisualComponent.h>
#include <SceneComponent.h>

namespace Ken4lowEngine
{
	/// 共通Character機能とPlayer専用の入力・移動・武器・Inventory・Camera・Rigidbodyを束ねるActor。
	class PlayerActor : public CharacterActor
	{
	public:
		/// 必要なPlayer専用Componentを不足分だけ生成し、共通Character ComponentとPhysicsへ接続する。
		void Initialize() override
		{
			SceneComponent* root = GetRootComponent();
			if (!root)
			{
				root = &CreateRootComponent<SceneComponent>();
				root->SetName("Player Root");
				root->SetUpdateOrder(-100);
			}

			if (!GetPlayerInputComponent())
			{
				auto& input = AddComponent<PlayerInputComponent>();
				input.SetName("Player Input");
				input.SetUpdateOrder(-95); // 入力要求の配送を移動・武器・Inventory・Cameraの更新より先に行う。
			}

			if (!GetPlayerMovementComponent())
			{
				auto& movement = AddComponent<PlayerMovementComponent>();
				movement.SetName("Player Movement");
				movement.SetUpdateOrder(-90);
			}

			if (!GetWeaponComponent())
			{
				auto& weapon = AddComponent<WeaponComponent>();
				weapon.SetName("Player Weapon");
				weapon.SetUpdateOrder(-80);
			}

			if (!GetInventoryComponent())
			{
				auto& inventory = AddComponent<InventoryComponent>();
				inventory.SetName("Player Inventory");
				inventory.SetUpdateOrder(-75);
			}

			auto* visual = GetHumanoidVisualComponent();
			if (!visual)
			{
				visual = &AddComponent<HumanoidVisualComponent>();
				visual->SetName("Player Humanoid Visual");
				visual->SetUpdateOrder(0);
				visual->SetDrawOrder(0);
				visual->SetCastShadowEnabled(true);
				visual->AttachTo(root);
			}

			auto* camera = GetPlayerCameraComponent();
			if (!camera)
			{
				camera = &AddComponent<PlayerCameraComponent>();
				camera->SetName("Player Camera");
				camera->SetUpdateOrder(10);
				camera->SetLocalPosition({ 0.0f, 1.6f, 0.0f });
				camera->SetAutoRegisterMainCamera(true); // DebugSceneでもPlayerActorのCameraを通常ゲーム視点として使用する。
				camera->AttachTo(root);
			}

			if (!GetWeaponViewComponent())
			{
				auto& weaponView = AddComponent<ModelComponent>();
				weaponView.SetName("Player Weapon View");
				weaponView.SetUpdateOrder(15);
				weaponView.SetDrawOrder(5);
				weaponView.SetModelPath("Sources/Weapons/primary_rifle.gltf");
				weaponView.SetLocalPosition({ 0.28f, -0.30f, 0.55f });
				weaponView.SetLocalRotation({ 1.5708f, 1.5708f, 0.0f });
				weaponView.SetLocalScale({ 0.55f, 0.55f, 0.55f });
				weaponView.SetCastShadowEnabled(false); // 一人称ViewModelの影をWorldへ落とさず、旧FPS表示に近い見え方へ寄せる。
				weaponView.AttachTo(camera);
			}

			const bool hadRigidbody = GetRigidbodyComponent() != nullptr;
			if (!hadRigidbody)
			{
				auto& rigidbody = AddComponent<RigidbodyComponent>();
				rigidbody.SetName("Player Rigidbody");
				rigidbody.SetUpdateOrder(-85);
				rigidbody.SetBodyType(BodyType::Dynamic);
				rigidbody.SetMass(1.0f);
				rigidbody.SetUseGravity(true);
				rigidbody.SetSleepEnabled(false);
				rigidbody.SetRestitution(0.0f);
				rigidbody.SetStaticFriction(0.0f);
				rigidbody.SetDynamicFriction(0.0f);
			}

			const bool hadHealth = GetHealthComponent() != nullptr;
			const bool hadCollider = GetColliderComponent() != nullptr;
			CharacterActor::Initialize();

			if (!hadHealth)
			{
				if (CharacterHealthComponent* health = GetHealthComponent()) health->ResetHealth(100.0f);
			}
			if (!hadCollider)
			{
				if (CharacterColliderComponent* collider = GetColliderComponent())
				{
					collider->SetShapeType(ECollisionShapeType::AABB);
					collider->SetHalfSize({ 0.45f, 0.9f, 0.45f });
					collider->SetCollisionLayer(PhysicsCollisionLayer::DynamicActor);
				}
			}
			if (visual && visual->GetSkinTexturePath().empty()) visual->ApplySkinToAllParts("Characters/steve.dds");
		}

		/// ViewModelがPlayer Cameraと同じ描画Cameraを使うよう、Component更新前に参照を同期する。
		void Update(float deltaTime) override
		{
			if (ModelComponent* weaponView = GetWeaponViewComponent())
			{
				if (PlayerCameraComponent* camera = GetPlayerCameraComponent()) weaponView->SetCamera(camera->GetCamera());
			}
			CharacterActor::Update(deltaTime);
		}

		/// JSON保存・復元で使用するActor識別名を返す。
		std::string GetClassTypeName() const override { return "PlayerActor"; }

		/// DebugSceneや移行確認から同じ個体を再利用できる初期状態へ戻す。
		void ResetForValidation(const Vector3& worldPosition)
		{
			SetActive(true);
			if (SceneComponent* root = GetRootComponent())
			{
				root->SetLocalPosition(worldPosition);
				root->SetLocalRotation({});
				root->RefreshWorldTransform();
			}
			if (CharacterHealthComponent* health = GetHealthComponent()) health->ResetHealth(100.0f);
			if (PlayerInputComponent* input = GetPlayerInputComponent()) input->SetInputEnabled(true);
			if (PlayerMovementComponent* movement = GetPlayerMovementComponent()) movement->ResetMovement();
			if (CharacterColliderComponent* collider = GetColliderComponent()) collider->SetActive(true);
			if (RigidbodyComponent* rigidbody = GetRigidbodyComponent()) rigidbody->SetVelocity({}); // 再配置時に以前の落下・移動速度を持ち越さない。
			if (WeaponComponent* weapon = GetWeaponComponent()) weapon->ResetWeapon();
			if (InventoryComponent* inventory = GetInventoryComponent()) inventory->ResetInventory();
			if (PlayerCameraComponent* camera = GetPlayerCameraComponent()) camera->ResetLook();
			if (CharacterAnimationComponent* animation = GetAnimationComponent()) animation->Play("Idle", 1.0f, true);
		}

		PlayerInputComponent* GetPlayerInputComponent() { return GetCharacterComponent<PlayerInputComponent>(); }
		const PlayerInputComponent* GetPlayerInputComponent() const { return GetCharacterComponent<PlayerInputComponent>(); }
		PlayerMovementComponent* GetPlayerMovementComponent() { return GetCharacterComponent<PlayerMovementComponent>(); }
		const PlayerMovementComponent* GetPlayerMovementComponent() const { return GetCharacterComponent<PlayerMovementComponent>(); }
		WeaponComponent* GetWeaponComponent() { return GetCharacterComponent<WeaponComponent>(); }
		const WeaponComponent* GetWeaponComponent() const { return GetCharacterComponent<WeaponComponent>(); }
		InventoryComponent* GetInventoryComponent() { return GetCharacterComponent<InventoryComponent>(); }
		const InventoryComponent* GetInventoryComponent() const { return GetCharacterComponent<InventoryComponent>(); }
		PlayerCameraComponent* GetPlayerCameraComponent() { return GetCharacterComponent<PlayerCameraComponent>(); }
		const PlayerCameraComponent* GetPlayerCameraComponent() const { return GetCharacterComponent<PlayerCameraComponent>(); }
		ModelComponent* GetWeaponViewComponent() { return GetCharacterComponent<ModelComponent>(); }
		const ModelComponent* GetWeaponViewComponent() const { return GetCharacterComponent<ModelComponent>(); }
		HumanoidVisualComponent* GetHumanoidVisualComponent() { return GetCharacterComponent<HumanoidVisualComponent>(); }
		const HumanoidVisualComponent* GetHumanoidVisualComponent() const { return GetCharacterComponent<HumanoidVisualComponent>(); }
		RigidbodyComponent* GetRigidbodyComponent() { return GetCharacterComponent<RigidbodyComponent>(); }
		const RigidbodyComponent* GetRigidbodyComponent() const { return GetCharacterComponent<RigidbodyComponent>(); }

	protected:
		/// 死亡時は入力・移動・武器・Collider・Rigidbodyを停止し、共通Healthの死亡状態を各機能へ反映する。
		void OnDeath(const CharacterDeathEvent& deathEvent) override
		{
			(void)deathEvent;
			if (PlayerInputComponent* input = GetPlayerInputComponent()) input->SetInputEnabled(false);
			if (PlayerMovementComponent* movement = GetPlayerMovementComponent())
			{
				movement->SetMoveInput(0.0f, 0.0f);
				movement->Stop();
				movement->SetMovementEnabled(false);
			}
			if (RigidbodyComponent* rigidbody = GetRigidbodyComponent()) rigidbody->SetVelocity({});
			if (WeaponComponent* weapon = GetWeaponComponent()) weapon->SetWeaponEnabled(false);
			if (CharacterColliderComponent* collider = GetColliderComponent()) collider->SetActive(false);
		}
	};
} // namespace Ken4lowEngine