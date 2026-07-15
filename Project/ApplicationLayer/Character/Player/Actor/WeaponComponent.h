#pragma once

#include <ActorComponent.h>

#include <algorithm>
#include <cmath>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// Playerの射撃・リロード・発射モードと弾薬を所有し、入力要求を具体的な武器状態へ変換するComponent。
	class WeaponComponent final : public ActorComponent
	{
	public:
		/// 射撃・リロード要求とCooldown・リロード時間を1フレーム進める。
		void Update(float deltaTime) override
		{
			const float safeDeltaTime = (std::max)(0.0f, deltaTime);
			fireCooldownRemaining_ = (std::max)(0.0f, fireCooldownRemaining_ - safeDeltaTime);

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

			if (automaticFireMode_ && triggerHeld_)
			{
				TryFire(); // AUTOはTrigger保持中だけCooldownに従って継続発射する。
			}
		}

		/// 武器状態と弾数をDebug表示する。
		void DrawImGui() override
		{
#ifdef USE_IMGUI
			ImGui::SeparatorText("武器");
			ImGui::Text("Weapon ID: %d", weaponId_);
			ImGui::Text("Ammo: %d / %d  Reserve: %d", magazineAmmo_, magazineCapacity_, reserveAmmo_);
			ImGui::Text("State: %s", isReloading_ ? "Reloading" : "Ready");
			ImGui::Text("Fire Mode: %s / Trigger: %s", automaticFireMode_ ? "AUTO" : "SEMI", triggerHeld_ ? "Held" : "Released");
			ImGui::Text("Damage: %.1f / Range: %.1f / Fire Interval: %.3f", damage_, range_, fireInterval_);
			ImGui::Text("Shot Revision: %u", shotRevision_);
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
			weaponId_ = inJson.value("WeaponId", weaponId_);
			magazineCapacity_ = (std::max)(1, inJson.value("MagazineCapacity", magazineCapacity_));
			magazineAmmo_ = (std::clamp)(inJson.value("MagazineAmmo", magazineAmmo_), 0, magazineCapacity_);
			reserveAmmo_ = (std::max)(0, inJson.value("ReserveAmmo", reserveAmmo_));
			reloadDuration_ = inJson.value("ReloadDuration", reloadDuration_);
			damage_ = inJson.value("Damage", damage_);
			range_ = inJson.value("Range", range_);
			fireInterval_ = inJson.value("FireInterval", fireInterval_);
			if (!std::isfinite(reloadDuration_) || reloadDuration_ < 0.0f) reloadDuration_ = 1.5f;
			if (!std::isfinite(damage_) || damage_ < 0.0f) damage_ = 25.0f;
			if (!std::isfinite(range_) || range_ <= 0.0f) range_ = 200.0f;
			if (!std::isfinite(fireInterval_) || fireInterval_ < 0.01f) fireInterval_ = 0.12f;
			automaticFireMode_ = inJson.value("AutomaticFireMode", automaticFireMode_);
			weaponEnabled_ = inJson.value("WeaponEnabled", weaponEnabled_);
			ResetTransientState();
		}

		void RequestFire() { fireRequested_ = true; }
		void RequestReload() { reloadRequested_ = true; }
		void RequestToggleFireMode() { toggleFireModeRequested_ = true; }
		void SetTriggerHeld(bool held) { triggerHeld_ = held; }
		void SetWeaponId(int weaponId) { weaponId_ = weaponId; }

		void SetWeaponEnabled(bool enabled)
		{
			weaponEnabled_ = enabled;
			if (!enabled)
			{
				ClearTransientRequests();
				triggerHeld_ = false;
			}
		}

		void ResetWeapon()
		{
			magazineAmmo_ = magazineCapacity_;
			reserveAmmo_ = defaultReserveAmmo_;
			ResetTransientState();
			weaponEnabled_ = true;
		}

		int GetWeaponId() const { return weaponId_; }
		int GetMagazineAmmo() const { return magazineAmmo_; }
		int GetMagazineCapacity() const { return magazineCapacity_; }
		int GetReserveAmmo() const { return reserveAmmo_; }
		float GetDamage() const { return damage_; }
		float GetRange() const { return range_; }
		float GetFireInterval() const { return fireInterval_; }
		bool IsReloading() const { return isReloading_; }
		bool IsWeaponEnabled() const { return weaponEnabled_; }
		bool IsAutomaticFireMode() const { return automaticFireMode_; }
		unsigned int GetShotRevision() const { return shotRevision_; }

	private:
		bool TryFire()
		{
			if (isReloading_ || magazineAmmo_ <= 0 || fireCooldownRemaining_ > 0.0f) return false;
			--magazineAmmo_; // Hit判定はCamera中心Rayを扱うゲームプレイ層へ通知し、このComponentは発射成立状態を確定する。
			fireCooldownRemaining_ = fireInterval_;
			++shotRevision_;
			return true;
		}

		bool StartReload()
		{
			if (isReloading_ || magazineAmmo_ >= magazineCapacity_ || reserveAmmo_ <= 0) return false;
			isReloading_ = true;
			reloadTimer_ = 0.0f;
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
			triggerHeld_ = false;
			ClearTransientRequests();
		}

	private:
		int weaponId_ = 1;
		int magazineCapacity_ = 30;
		int magazineAmmo_ = 30;
		int reserveAmmo_ = 90;
		int defaultReserveAmmo_ = 90;
		float reloadDuration_ = 1.5f;
		float reloadTimer_ = 0.0f;
		float damage_ = 25.0f;
		float range_ = 200.0f;
		float fireInterval_ = 0.12f;
		float fireCooldownRemaining_ = 0.0f;
		bool weaponEnabled_ = true;
		bool isReloading_ = false;
		bool automaticFireMode_ = true;
		bool triggerHeld_ = false;
		bool fireRequested_ = false;
		bool reloadRequested_ = false;
		bool toggleFireModeRequested_ = false;
		unsigned int shotRevision_ = 0;
	};
} // namespace Ken4lowEngine
