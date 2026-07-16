#pragma once

#include <ActorComponent.h>
#include <Vector3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// Playerの射撃・リロード・複数武器状態・ViewModel演出を所有するComponent。
	class WeaponComponent final : public ActorComponent
	{
	public:
		void Initialize() override
		{
			ActorComponent::Initialize();
			ResetWeapon();
		}

		void Update(float deltaTime) override
		{
			const float safeDeltaTime = (std::max)(0.0f, deltaTime);
			fireCooldownRemaining_ = (std::max)(0.0f, fireCooldownRemaining_ - safeDeltaTime);
			dryFireFeedbackRemaining_ = (std::max)(0.0f, dryFireFeedbackRemaining_ - safeDeltaTime);
			recoilTimer_ = (std::max)(0.0f, recoilTimer_ - safeDeltaTime);
			if (equipTimer_ < equipDuration_) equipTimer_ = (std::min)(equipDuration_, equipTimer_ + safeDeltaTime);
			reticleKick_ = (std::max)(0.0f, reticleKick_ - reticleRecoverSpeed_ * safeDeltaTime);

			if (!weaponEnabled_)
			{
				ClearTransientRequests();
				triggerHeld_ = false;
				return;
			}

			if (toggleFireModeRequested_)
			{
				toggleFireModeRequested_ = false;
				automaticFireMode_ = !automaticFireMode_;
			}

			if (isReloading_)
			{
				reloadTimer_ += safeDeltaTime;
				if (reloadTimer_ >= reloadDuration_) FinishReload();
			}

			if (reloadRequested_)
			{
				reloadRequested_ = false;
				StartReload();
			}

			if (fireRequested_)
			{
				fireRequested_ = false;
				TryFire();
			}

			if (automaticFireMode_ && triggerHeld_) TryFire();
		}

		void DrawImGui() override
		{
#ifdef USE_IMGUI
			ImGui::SeparatorText("武器");
			ImGui::Text("Weapon: %s  ID:%d", GetWeaponDisplayName(), weaponId_);
			ImGui::Text("Ammo: %d / %d  Reserve: %d / %d", magazineAmmo_, magazineCapacity_, reserveAmmo_, maxReserveAmmo_);
			ImGui::Text("State: %s / Equip: %.2f", isReloading_ ? "Reloading" : "Ready", GetEquipNormalizedTime());
			ImGui::Text("Fire Mode: %s / Trigger: %s", automaticFireMode_ ? "AUTO" : "SEMI", triggerHeld_ ? "Held" : "Released");
			ImGui::Text("Damage: %.1f / Range: %.1f / Fire Interval: %.3f", damage_, range_, fireInterval_);
			ImGui::Text("Reticle Spread: %.2f / Shot Revision: %u / Equip Revision: %u", GetReticleSpread(), shotRevision_, equipRevision_);
#endif
		}

		std::string GetClassTypeName() const override { return "WeaponComponent"; }

		void ToJson(nlohmann::json& outJson) const override
		{
			ActorComponent::ToJson(outJson);
			outJson["WeaponId"] = weaponId_;
			outJson["MagazineCapacity"] = magazineCapacity_;
			outJson["MagazineAmmo"] = magazineAmmo_;
			outJson["ReserveAmmo"] = reserveAmmo_;
			outJson["MaxReserveAmmo"] = maxReserveAmmo_;
			outJson["ReloadDuration"] = reloadDuration_;
			outJson["Damage"] = damage_;
			outJson["Range"] = range_;
			outJson["FireInterval"] = fireInterval_;
			outJson["AutomaticFireMode"] = automaticFireMode_;
			outJson["WeaponEnabled"] = weaponEnabled_;
		}

		void FromJson(const nlohmann::json& inJson) override
		{
			ActorComponent::FromJson(inJson);
			ResetAllStoredStates();
			weaponId_ = NormalizeWeaponId(inJson.value("WeaponId", weaponId_));
			LoadWeaponDefinition(weaponId_, false);
			magazineCapacity_ = (std::max)(1, inJson.value("MagazineCapacity", magazineCapacity_));
			magazineAmmo_ = (std::clamp)(inJson.value("MagazineAmmo", magazineAmmo_), 0, magazineCapacity_);
			maxReserveAmmo_ = (std::max)(0, inJson.value("MaxReserveAmmo", maxReserveAmmo_));
			reserveAmmo_ = (std::clamp)(inJson.value("ReserveAmmo", reserveAmmo_), 0, maxReserveAmmo_);
			reloadDuration_ = SanitizePositive(inJson.value("ReloadDuration", reloadDuration_), reloadDuration_, 0.01f);
			damage_ = SanitizePositive(inJson.value("Damage", damage_), damage_, 0.0f);
			range_ = SanitizePositive(inJson.value("Range", range_), range_, 0.01f);
			fireInterval_ = SanitizePositive(inJson.value("FireInterval", fireInterval_), fireInterval_, 0.01f);
			automaticFireMode_ = inJson.value("AutomaticFireMode", automaticFireMode_);
			weaponEnabled_ = inJson.value("WeaponEnabled", weaponEnabled_);
			SaveCurrentWeaponState();
			ResetTransientState();
			RestartEquipAnimation();
		}

		void RequestFire() { fireRequested_ = true; }
		void RequestReload() { reloadRequested_ = true; }
		void RequestToggleFireMode() { toggleFireModeRequested_ = true; }
		void SetTriggerHeld(bool held) { triggerHeld_ = held; }

		void SetWeaponId(int weaponId)
		{
			const int normalized = NormalizeWeaponId(weaponId);
			if (normalized == weaponId_) return;
			SaveCurrentWeaponState();
			weaponId_ = normalized;
			LoadWeaponDefinition(weaponId_, true);
			RestartEquipAnimation();
			++equipRevision_; // 武器切替時だけViewModelとHUDへ装備変更を通知する。
		}

		void SetWeaponEnabled(bool enabled)
		{
			weaponEnabled_ = enabled;
			if (!enabled)
			{
				ClearTransientRequests();
				triggerHeld_ = false;
			}
		}

		void ConfigureAmmoState(int magazineCapacity, int magazineAmmo, int reserveAmmo, int maxReserveAmmo = 120)
		{
			magazineCapacity_ = (std::max)(1, magazineCapacity);
			magazineAmmo_ = (std::clamp)(magazineAmmo, 0, magazineCapacity_);
			maxReserveAmmo_ = (std::max)(reserveAmmo, maxReserveAmmo);
			reserveAmmo_ = (std::clamp)(reserveAmmo, 0, maxReserveAmmo_);
			defaultReserveAmmo_ = reserveAmmo_;
			SaveCurrentWeaponState();
			ResetTransientState();
		}

		int AddReserveAmmo(int amount)
		{
			const int before = reserveAmmo_;
			reserveAmmo_ = (std::clamp)(reserveAmmo_ + amount, 0, maxReserveAmmo_);
			SaveCurrentWeaponState();
			return reserveAmmo_ - before;
		}

		void ResetWeapon()
		{
			ResetAllStoredStates();
			weaponId_ = 0;
			LoadWeaponDefinition(weaponId_, false);
			ResetTransientState();
			weaponEnabled_ = true;
			RestartEquipAnimation();
			++equipRevision_;
		}

		void RestartEquipAnimation()
		{
			equipTimer_ = 0.0f;
			isReloading_ = false;
			reloadTimer_ = 0.0f;
			fireCooldownRemaining_ = 0.0f;
		}

		int GetWeaponId() const { return weaponId_; }
		int GetMagazineAmmo() const { return magazineAmmo_; }
		int GetMagazineCapacity() const { return magazineCapacity_; }
		int GetReserveAmmo() const { return reserveAmmo_; }
		int GetMaxReserveAmmo() const { return maxReserveAmmo_; }
		float GetDamage() const { return damage_; }
		float GetRange() const { return range_; }
		float GetFireInterval() const { return fireInterval_; }
		float GetReloadTimer() const { return reloadTimer_; }
		float GetReloadDuration() const { return reloadDuration_; }
		bool IsReloading() const { return isReloading_; }
		bool IsWeaponEnabled() const { return weaponEnabled_; }
		bool IsAutomaticFireMode() const { return automaticFireMode_; }
		bool IsEquipAnimating() const { return equipTimer_ < equipDuration_; }
		bool ShouldShowNoAmmoFeedback() const { return dryFireFeedbackRemaining_ > 0.0f || (magazineAmmo_ <= 0 && reserveAmmo_ <= 0); }
		unsigned int GetShotRevision() const { return shotRevision_; }
		unsigned int GetEquipRevision() const { return equipRevision_; }
		const char* GetWeaponDisplayName() const { return GetDefinition(weaponId_).displayName; }
		const char* GetViewModelPath() const { return GetDefinition(weaponId_).modelPath; }
		float GetReticleSpread() const { return reticleBaseSpread_ + reticleKick_; }
		float GetReticleMaxSpread() const { return reticleMaxSpread_; }
		float GetEquipNormalizedTime() const { return equipDuration_ > 0.000001f ? std::clamp(equipTimer_ / equipDuration_, 0.0f, 1.0f) : 1.0f; }

		Vector3 GetViewModelPositionOffset() const
		{
			const float equipT = SmoothStep(GetEquipNormalizedTime());
			Vector3 offset{ 0.0f, -(1.0f - equipT) * 0.42f, -(1.0f - equipT) * 0.12f };
			if (isReloading_ && reloadDuration_ > 0.000001f)
			{
				const float progress = std::clamp(reloadTimer_ / reloadDuration_, 0.0f, 1.0f);
				const float arc = std::sin(progress * kPi);
				offset.x += arc * 0.08f;
				offset.y -= arc * 0.18f;
			}
			if (recoilTimer_ > 0.0f && recoilDuration_ > 0.000001f) offset.z -= 0.10f * (recoilTimer_ / recoilDuration_);
			return offset;
		}

		Vector3 GetViewModelRotationOffset() const
		{
			const float equipT = SmoothStep(GetEquipNormalizedTime());
			Vector3 rotation{ (1.0f - equipT) * 0.45f, 0.0f, 0.0f };
			if (isReloading_ && reloadDuration_ > 0.000001f)
			{
				const float progress = std::clamp(reloadTimer_ / reloadDuration_, 0.0f, 1.0f);
				const float arc = std::sin(progress * kPi);
				rotation.x += arc * 0.34f;
				rotation.z += arc * 0.20f;
			}
			if (recoilTimer_ > 0.0f && recoilDuration_ > 0.000001f) rotation.x -= 0.14f * (recoilTimer_ / recoilDuration_);
			return rotation;
		}

	private:
		struct WeaponDefinition
		{
			int id = 0;
			const char* displayName = "Primary Rifle";
			const char* modelPath = "Sources/Weapons/primary_rifle.gltf";
			int magazineCapacity = 30;
			int reserveAmmo = 90;
			int maxReserveAmmo = 120;
			float reloadDuration = 1.95f;
			float damage = 31.0f;
			float range = 200.0f;
			float fireInterval = 60.0f / 780.0f;
			bool automatic = true;
			float baseSpread = 1.2f;
			float maxSpread = 5.0f;
			float spreadPerShot = 2.0f;
			float spreadRecovery = 18.0f;
			float equipDuration = 0.35f;
		};

		struct StoredWeaponState
		{
			bool valid = false;
			int magazineAmmo = 0;
			int reserveAmmo = 0;
			bool automatic = false;
		};

		static const WeaponDefinition& GetDefinition(int weaponId)
		{
			static const std::array<WeaponDefinition, 4> definitions = {
				WeaponDefinition{ 0, "M4A1", "Sources/Weapons/primary_rifle.gltf", 30, 90, 120, 1.95f, 31.0f, 200.0f, 60.0f / 780.0f, true, 1.2f, 5.0f, 2.0f, 18.0f, 0.35f },
				WeaponDefinition{ 1, "Backup", "Sources/Weapons/primary_rifle.gltf", 12, 48, 72, 1.35f, 42.0f, 160.0f, 0.25f, false, 0.8f, 4.0f, 1.4f, 20.0f, 0.24f },
				WeaponDefinition{ 2, "Special", "Sources/Weapons/primary_rifle.gltf", 8, 32, 48, 2.20f, 70.0f, 240.0f, 0.52f, false, 0.55f, 3.5f, 1.0f, 14.0f, 0.42f },
				WeaponDefinition{ 3, "Heavy", "Sources/Weapons/primary_rifle.gltf", 60, 180, 240, 2.60f, 18.0f, 180.0f, 0.09f, true, 1.8f, 7.0f, 2.5f, 12.0f, 0.55f }
			};
			return definitions[static_cast<std::size_t>(NormalizeWeaponId(weaponId))];
		}

		static int NormalizeWeaponId(int weaponId) { return std::clamp(weaponId, 0, 3); }
		static float SmoothStep(float value) { return value * value * (3.0f - 2.0f * value); }
		static float SanitizePositive(float value, float fallback, float minimum)
		{
			return std::isfinite(value) ? (std::max)(minimum, value) : fallback;
		}

		void ResetAllStoredStates()
		{
			for (StoredWeaponState& state : storedStates_) state = {};
		}

		void SaveCurrentWeaponState()
		{
			StoredWeaponState& state = storedStates_[static_cast<std::size_t>(NormalizeWeaponId(weaponId_))];
			state.valid = true;
			state.magazineAmmo = magazineAmmo_;
			state.reserveAmmo = reserveAmmo_;
			state.automatic = automaticFireMode_;
		}

		void LoadWeaponDefinition(int weaponId, bool restoreStoredState)
		{
			const WeaponDefinition& definition = GetDefinition(weaponId);
			magazineCapacity_ = definition.magazineCapacity;
			maxReserveAmmo_ = definition.maxReserveAmmo;
			defaultReserveAmmo_ = definition.reserveAmmo;
			reloadDuration_ = definition.reloadDuration;
			damage_ = definition.damage;
			range_ = definition.range;
			fireInterval_ = definition.fireInterval;
			reticleBaseSpread_ = definition.baseSpread;
			reticleMaxSpread_ = definition.maxSpread;
			reticleSpreadPerShot_ = definition.spreadPerShot;
			reticleRecoverSpeed_ = definition.spreadRecovery;
			equipDuration_ = definition.equipDuration;

			const StoredWeaponState& stored = storedStates_[static_cast<std::size_t>(NormalizeWeaponId(weaponId))];
			if (restoreStoredState && stored.valid)
			{
				magazineAmmo_ = std::clamp(stored.magazineAmmo, 0, magazineCapacity_);
				reserveAmmo_ = std::clamp(stored.reserveAmmo, 0, maxReserveAmmo_);
				automaticFireMode_ = stored.automatic;
			}
			else
			{
				magazineAmmo_ = magazineCapacity_;
				reserveAmmo_ = defaultReserveAmmo_;
				automaticFireMode_ = definition.automatic;
				SaveCurrentWeaponState();
			}
			ResetTransientState();
		}

		bool TryFire()
		{
			if (isReloading_ || IsEquipAnimating() || fireCooldownRemaining_ > 0.0f) return false;
			if (magazineAmmo_ <= 0)
			{
				dryFireFeedbackRemaining_ = 0.35f;
				return false;
			}
			--magazineAmmo_;
			fireCooldownRemaining_ = fireInterval_;
			recoilTimer_ = recoilDuration_;
			reticleKick_ = (std::min)(reticleMaxSpread_ - reticleBaseSpread_, reticleKick_ + reticleSpreadPerShot_);
			++shotRevision_;
			SaveCurrentWeaponState();
			return true;
		}

		bool StartReload()
		{
			if (isReloading_ || IsEquipAnimating() || magazineAmmo_ >= magazineCapacity_ || reserveAmmo_ <= 0) return false;
			isReloading_ = true;
			reloadTimer_ = 0.0f;
			dryFireFeedbackRemaining_ = 0.0f;
			return true;
		}

		void FinishReload()
		{
			const int required = magazineCapacity_ - magazineAmmo_;
			const int loaded = (std::min)(required, reserveAmmo_);
			magazineAmmo_ += loaded;
			reserveAmmo_ -= loaded;
			isReloading_ = false;
			reloadTimer_ = 0.0f;
			SaveCurrentWeaponState();
		}

		void ClearTransientRequests()
		{
			fireRequested_ = false;
			reloadRequested_ = false;
			toggleFireModeRequested_ = false;
		}

		void ResetTransientState()
		{
			isReloading_ = false;
			reloadTimer_ = 0.0f;
			fireCooldownRemaining_ = 0.0f;
			recoilTimer_ = 0.0f;
			reticleKick_ = 0.0f;
			dryFireFeedbackRemaining_ = 0.0f;
			triggerHeld_ = false;
			ClearTransientRequests();
		}

	private:
		static constexpr float kPi = 3.14159265358979323846f;
		std::array<StoredWeaponState, 4> storedStates_{};
		int weaponId_ = 0;
		int magazineCapacity_ = 30;
		int magazineAmmo_ = 30;
		int reserveAmmo_ = 90;
		int maxReserveAmmo_ = 120;
		int defaultReserveAmmo_ = 90;
		float reloadDuration_ = 1.95f;
		float reloadTimer_ = 0.0f;
		float damage_ = 31.0f;
		float range_ = 200.0f;
		float fireInterval_ = 60.0f / 780.0f;
		float fireCooldownRemaining_ = 0.0f;
		float equipDuration_ = 0.35f;
		float equipTimer_ = 0.35f;
		float recoilDuration_ = 0.10f;
		float recoilTimer_ = 0.0f;
		float reticleBaseSpread_ = 1.2f;
		float reticleMaxSpread_ = 5.0f;
		float reticleSpreadPerShot_ = 2.0f;
		float reticleRecoverSpeed_ = 18.0f;
		float reticleKick_ = 0.0f;
		float dryFireFeedbackRemaining_ = 0.0f;
		bool weaponEnabled_ = true;
		bool isReloading_ = false;
		bool automaticFireMode_ = true;
		bool triggerHeld_ = false;
		bool fireRequested_ = false;
		bool reloadRequested_ = false;
		bool toggleFireModeRequested_ = false;
		unsigned int shotRevision_ = 0u;
		unsigned int equipRevision_ = 0u;
	};
} // namespace Ken4lowEngine
