#pragma once

#include "PlayerInputComponent.h"
#include "WeaponComponent.h"

#include <Actor.h>
#include <ActorComponent.h>
#include <GaugeComponent.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>
#include <SpriteComponent.h>
#include <TextComponent.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <string_view>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// PlayerのHP・弾薬・Crosshair表示をPlayerActor本体から分離して同期するPresentation Component。
	class PlayerHudPresenterComponent final : public ActorComponent
	{
	public:
		void Update(float deltaTime) override
		{
			damageFlashRemaining_ = std::max(0.0f, damageFlashRemaining_ - std::max(0.0f, deltaTime));
			SyncHealth();
			SyncWeapon();
			ApplyCrosshairState(); // HUDの見た目更新をPlayerActorのゲームロジックから切り離す。
		}

		void DrawImGui() override
		{
#ifdef USE_IMGUI
			ImGui::SeparatorText("Player HUD Presentation");
			ImGui::Text("Visible: %s", hudVisible_ ? "Yes" : "No");
			ImGui::Text("Crosshair Targeted: %s", crosshairTargeted_ ? "Yes" : "No");
			ImGui::Text("Damage Flash: %.3f", damageFlashRemaining_);
#endif
		}

		std::string GetClassTypeName() const override { return "PlayerHudPresenterComponent"; }

		void SetHudVisible(bool visible)
		{
			hudVisible_ = visible;
			if (GaugeComponent* gauge = FindNamedComponent<GaugeComponent>("Player HP Gauge")) gauge->SetVisible(visible);
			if (TextComponent* label = FindNamedComponent<TextComponent>("Player HP Label")) label->SetVisible(visible);
			if (TextComponent* ammo = FindNamedComponent<TextComponent>("Player Ammo Text")) ammo->SetVisible(visible);
			ApplyCrosshairState();
		}

		void SetCrosshairTargeted(bool targeted)
		{
			crosshairTargeted_ = targeted;
			ApplyCrosshairState();
		}

		void NotifyDamageTaken()
		{
			damageFlashRemaining_ = damageFlashDuration_;
		}

		void ResetPresentation()
		{
			hudVisible_ = true;
			crosshairTargeted_ = false;
			damageFlashRemaining_ = 0.0f;
			SyncHealth();
			SyncWeapon();
			ApplyCrosshairState();
		}

	private:
		template<class T>
		T* FindNamedComponent(std::string_view name) const
		{
			Actor* owner = GetOwner();
			if (!owner) return nullptr;
			for (T* component : owner->GetComponents<T>())
			{
				if (component && component->GetName() == name) return component;
			}
			return nullptr;
		}

		void SyncHealth()
		{
			Actor* owner = GetOwner();
			CharacterHealthComponent* health = owner ? owner->GetComponent<CharacterHealthComponent>() : nullptr;
			if (!health) return;

			if (GaugeComponent* gauge = FindNamedComponent<GaugeComponent>("Player HP Gauge"))
			{
				gauge->SetMaxValue(health->GetMaxHealth());
				gauge->SetValue(health->GetCurrentHealth());
			}

			if (TextComponent* label = FindNamedComponent<TextComponent>("Player HP Label"))
			{
				char text[64]{};
				std::snprintf(text, sizeof(text), "PLAYER HP %.0f / %.0f", health->GetCurrentHealth(), health->GetMaxHealth());
				label->SetText(text);
				if (health->IsDead()) label->SetColor({ 1.0f, 0.2f, 0.2f, 1.0f });
				else if (damageFlashRemaining_ > 0.0f) label->SetColor({ 1.0f, 0.55f, 0.25f, 1.0f });
				else label->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
			}
		}

		void SyncWeapon()
		{
			Actor* owner = GetOwner();
			WeaponComponent* weapon = owner ? owner->GetComponent<WeaponComponent>() : nullptr;
			TextComponent* ammo = FindNamedComponent<TextComponent>("Player Ammo Text");
			if (!weapon || !ammo) return;

			char text[128]{};
			std::snprintf(text, sizeof(text), "AMMO %d / %d   RESERVE %d%s%s",
				weapon->GetMagazineAmmo(), weapon->GetMagazineCapacity(), weapon->GetReserveAmmo(),
				weapon->IsReloading() ? "   RELOADING" : "",
				weapon->IsAutomaticFireMode() ? "   AUTO" : "   SEMI");
			ammo->SetText(text);
		}

		void ApplyCrosshairState()
		{
			Actor* owner = GetOwner();
			if (!owner) return;
			const PlayerInputComponent* input = owner->GetComponent<PlayerInputComponent>();
			const bool aiming = input && input->IsAimHeld();
			const Vector4 color = crosshairTargeted_
				? Vector4{ 1.0f, 0.25f, 0.25f, 1.0f }
				: Vector4{ 1.0f, 1.0f, 1.0f, aiming ? 0.35f : 0.92f };

			for (SpriteComponent* sprite : owner->GetComponents<SpriteComponent>())
			{
				if (sprite && sprite->GetName().starts_with("Player Crosshair "))
				{
					sprite->SetVisible(hudVisible_);
					sprite->SetColor(color);
				}
			}
		}

	private:
		bool hudVisible_ = true;
		bool crosshairTargeted_ = false;
		float damageFlashRemaining_ = 0.0f;
		float damageFlashDuration_ = 0.18f;
	};
} // namespace Ken4lowEngine
