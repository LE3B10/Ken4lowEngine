#pragma once

#include "../IPlayerRuntime.h"
#include "InventoryComponent.h"
#include "PlayerCameraComponent.h"
#include "PlayerHudPresenterComponent.h"
#include "PlayerInputComponent.h"
#include "PlayerMovementComponent.h"
#include "WeaponComponent.h"

#include <GameViewportConstants.h>
#include <GaugeComponent.h>
#include <ModelComponent.h>
#include <PhysicsCollisionLayer.h>
#include <RigidbodyComponent.h>
#include <Scene/Actor/Character/CharacterActor.h>
#include <Scene/Actor/Character/CharacterAnimationComponent.h>
#include <Scene/Actor/Character/CharacterColliderComponent.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>
#include <Scene/Actor/Character/HumanoidVisualComponent.h>
#include <SceneComponent.h>
#include <SpriteComponent.h>
#include <TextComponent.h>

#include <functional>
#include <string_view>

namespace Ken4lowEngine
{
	/// 共通Character機能とPlayer専用Componentを束ね、外部へ最小Runtime境界だけを公開するActor。
	class PlayerActor : public CharacterActor, public ::IPlayerRuntime
	{
	public:
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
				input.SetUpdateOrder(-95);
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
			if (!GetPlayerHudPresenterComponent())
			{
				auto& hud = AddComponent<PlayerHudPresenterComponent>();
				hud.SetName("Player HUD Presenter");
				hud.SetUpdateOrder(50); // 状態更新後にHP・弾薬・Crosshairを同期する。
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
				camera->SetLocalPosition({ 0.0f, 1.2f, 0.2f });
				camera->SetAutoRegisterMainCamera(true);
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
				weaponView.SetCastShadowEnabled(false);
				weaponView.AttachTo(camera);
			}

			CreateGameplayHudComponents();

			if (!GetRigidbodyComponent())
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
			CharacterActor::Initialize();
			if (!hadHealth)
			{
				if (CharacterHealthComponent* health = GetHealthComponent()) health->ResetHealth(100.0f);
			}
			if (visual && visual->GetSkinTexturePath().empty()) visual->ApplySkinToAllParts("Characters/steve.dds");
			deathTimer_ = 0.0f;
			gameOverReady_ = false;
			if (PlayerHudPresenterComponent* hud = GetPlayerHudPresenterComponent()) hud->ResetPresentation();
		}

		void Update(float deltaTime) override
		{
			if (ModelComponent* weaponView = GetWeaponViewComponent())
			{
				if (PlayerCameraComponent* camera = GetPlayerCameraComponent()) weaponView->SetCamera(camera->GetCamera());
			}
			CharacterActor::Update(deltaTime);
			UpdateDeathLifecycle(deltaTime);
		}

		std::string GetClassTypeName() const override { return "PlayerActor"; }

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
			if (RigidbodyComponent* rigidbody = GetRigidbodyComponent()) rigidbody->SetVelocity({});
			if (WeaponComponent* weapon = GetWeaponComponent()) weapon->ResetWeapon();
			if (InventoryComponent* inventory = GetInventoryComponent()) inventory->ResetInventory();
			if (PlayerCameraComponent* camera = GetPlayerCameraComponent()) camera->ResetLook();
			if (CharacterAnimationComponent* animation = GetAnimationComponent()) animation->Play("Idle", 1.0f, true);
			deathTimer_ = 0.0f;
			gameOverReady_ = false;
			if (PlayerHudPresenterComponent* hud = GetPlayerHudPresenterComponent()) hud->ResetPresentation();
		}

		CharacterDamageResult ApplyPlayerDamage(float amount)
		{
			const CharacterDamageResult result = CharacterActor::ApplyDamage(amount);
			if (result.accepted && result.appliedDamage > 0.0f)
			{
				if (PlayerHudPresenterComponent* hud = GetPlayerHudPresenterComponent()) hud->NotifyDamageTaken();
				if (onDamageTaken_) onDamageTaken_();
			}
			return result;
		}

		float HealPlayer(float amount)
		{
			CharacterHealthComponent* health = GetHealthComponent();
			return health ? health->Heal(amount) : 0.0f;
		}

		void SetOnDamageTakenCallback(std::function<void()> callback) { onDamageTaken_ = std::move(callback); }

		float GetHP() const override
		{
			const CharacterHealthComponent* health = GetHealthComponent();
			return health ? health->GetCurrentHealth() : 0.0f;
		}
		float GetMaxHP() const override
		{
			const CharacterHealthComponent* health = GetHealthComponent();
			return health ? health->GetMaxHealth() : 0.0f;
		}
		bool IsGameOverReady() const override { return gameOverReady_; }
		bool ConsumeGameOverReady() override
		{
			const bool ready = gameOverReady_;
			gameOverReady_ = false;
			return ready;
		}
		bool IsDeathActive() const override { return CharacterActor::IsDead(); }

		void SetCrosshairTargeted(bool targeted)
		{
			if (PlayerHudPresenterComponent* hud = GetPlayerHudPresenterComponent()) hud->SetCrosshairTargeted(targeted);
		}
		void SetGameplayHudVisible(bool visible)
		{
			if (PlayerHudPresenterComponent* hud = GetPlayerHudPresenterComponent()) hud->SetHudVisible(visible);
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
		PlayerHudPresenterComponent* GetPlayerHudPresenterComponent() { return GetCharacterComponent<PlayerHudPresenterComponent>(); }
		const PlayerHudPresenterComponent* GetPlayerHudPresenterComponent() const { return GetCharacterComponent<PlayerHudPresenterComponent>(); }
		ModelComponent* GetWeaponViewComponent() { return FindNamedComponent<ModelComponent>("Player Weapon View"); }
		const ModelComponent* GetWeaponViewComponent() const { return FindNamedComponent<ModelComponent>("Player Weapon View"); }
		HumanoidVisualComponent* GetHumanoidVisualComponent() { return GetCharacterComponent<HumanoidVisualComponent>(); }
		const HumanoidVisualComponent* GetHumanoidVisualComponent() const { return GetCharacterComponent<HumanoidVisualComponent>(); }
		RigidbodyComponent* GetRigidbodyComponent() { return GetCharacterComponent<RigidbodyComponent>(); }
		const RigidbodyComponent* GetRigidbodyComponent() const { return GetCharacterComponent<RigidbodyComponent>(); }

	protected:
		void OnDeath(const CharacterDeathEvent& deathEvent) override
		{
			(void)deathEvent;
			deathTimer_ = 0.0f;
			gameOverReady_ = false;
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
			if (PlayerHudPresenterComponent* hud = GetPlayerHudPresenterComponent()) hud->SetCrosshairTargeted(false);
		}

	private:
		template<class T>
		T* FindNamedComponent(std::string_view name)
		{
			for (T* component : GetComponents<T>()) if (component && component->GetName() == name) return component;
			return nullptr;
		}
		template<class T>
		const T* FindNamedComponent(std::string_view name) const
		{
			for (const T* component : GetComponents<T>()) if (component && component->GetName() == name) return component;
			return nullptr;
		}

		GaugeComponent* GetPlayerHealthGaugeComponent() { return FindNamedComponent<GaugeComponent>("Player HP Gauge"); }
		TextComponent* GetPlayerHealthLabelComponent() { return FindNamedComponent<TextComponent>("Player HP Label"); }
		TextComponent* GetAmmoTextComponent() { return FindNamedComponent<TextComponent>("Player Ammo Text"); }

		void CreateGameplayHudComponents()
		{
			constexpr float screenWidth = static_cast<float>(GameViewportConstants::Width);
			constexpr float screenHeight = static_cast<float>(GameViewportConstants::Height);
			if (!GetPlayerHealthGaugeComponent())
			{
				auto& gauge = AddComponent<GaugeComponent>();
				gauge.SetName("Player HP Gauge");
				gauge.SetDrawOrder(100);
				gauge.SetPosition({ 36.0f, screenHeight - 54.0f });
				gauge.SetSize({ 300.0f, 24.0f });
				gauge.SetBackgroundColor({ 0.05f, 0.05f, 0.05f, 0.82f });
				gauge.SetFillColor({ 0.20f, 0.82f, 0.32f, 1.0f });
				gauge.SetBorderColor({ 1.0f, 1.0f, 1.0f, 0.90f });
				gauge.SetBorderThickness(2.0f);
			}
			if (!GetPlayerHealthLabelComponent())
			{
				auto& label = AddComponent<TextComponent>();
				label.SetName("Player HP Label");
				label.SetDrawOrder(101);
				label.SetText("PLAYER HP");
				label.SetPosition({ 36.0f, screenHeight - 82.0f });
				label.SetFontSize(22.0f);
			}
			if (!GetAmmoTextComponent())
			{
				auto& ammo = AddComponent<TextComponent>();
				ammo.SetName("Player Ammo Text");
				ammo.SetDrawOrder(101);
				ammo.SetPosition({ screenWidth - 36.0f, screenHeight - 58.0f });
				ammo.SetAnchor({ 1.0f, 0.0f });
				ammo.SetFontSize(22.0f);
			}
			CreateCrosshairBar("Player Crosshair Left", { screenWidth * 0.5f - 10.0f, screenHeight * 0.5f }, { 8.0f, 2.0f });
			CreateCrosshairBar("Player Crosshair Right", { screenWidth * 0.5f + 10.0f, screenHeight * 0.5f }, { 8.0f, 2.0f });
			CreateCrosshairBar("Player Crosshair Top", { screenWidth * 0.5f, screenHeight * 0.5f - 10.0f }, { 2.0f, 8.0f });
			CreateCrosshairBar("Player Crosshair Bottom", { screenWidth * 0.5f, screenHeight * 0.5f + 10.0f }, { 2.0f, 8.0f });
		}

		void CreateCrosshairBar(std::string_view name, const Vector2& position, const Vector2& size)
		{
			if (FindNamedComponent<SpriteComponent>(name)) return;
			auto& sprite = AddComponent<SpriteComponent>();
			sprite.SetName(name);
			sprite.SetDrawOrder(110);
			sprite.SetTexturePath("Effects/white.dds");
			sprite.SetPosition(position);
			sprite.SetSize(size);
			sprite.SetAnchor({ 0.5f, 0.5f });
		}

		void UpdateDeathLifecycle(float deltaTime)
		{
			if (!CharacterActor::IsDead()) return;
			deathTimer_ += (std::max)(0.0f, deltaTime);
			if (deathTimer_ >= gameOverDelay_) gameOverReady_ = true; // 死亡演出用の最小待機後にGamePlayへ遷移可能状態を公開する。
		}

	private:
		std::function<void()> onDamageTaken_{};
		float deathTimer_ = 0.0f;
		float gameOverDelay_ = 1.0f;
		bool gameOverReady_ = false;
	};
} // namespace Ken4lowEngine
