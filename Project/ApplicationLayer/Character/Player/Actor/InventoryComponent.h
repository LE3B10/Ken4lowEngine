#pragma once

#include "WeaponComponent.h"

#include <Actor.h>
#include <ActorComponent.h>

#include <algorithm>
#include <array>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// Playerの武器スロットと現在選択中のスロットを所有するInventory Component。
	class InventoryComponent final : public ActorComponent
	{
	public:
		/// 予約された直接選択または循環選択を確定し、WeaponComponentへ装備武器IDを通知する。
		void Update(float deltaTime) override
		{
			(void)deltaTime;
			if (pendingSlot_ >= 0)
			{
				const int requestedSlot = pendingSlot_;
				pendingSlot_ = -1;
				pendingCycleDirection_ = 0;
				SelectSlot(requestedSlot);
				return;
			}
			if (pendingCycleDirection_ != 0)
			{
				const int direction = pendingCycleDirection_;
				pendingCycleDirection_ = 0;
				CycleSlot(direction);
			}
		}

		void DrawImGui() override
		{
#ifdef USE_IMGUI
			ImGui::SeparatorText("Inventory");
			ImGui::Text("Selected Slot: %d", selectedSlot_);
			for (int i = 0; i < static_cast<int>(slots_.size()); ++i) ImGui::Text("Slot %d: Weapon %d", i, slots_[i]);
#endif
		}

		std::string GetClassTypeName() const override { return "InventoryComponent"; }

		void ToJson(nlohmann::json& outJson) const override
		{
			ActorComponent::ToJson(outJson);
			outJson["Slots"] = slots_;
			outJson["SelectedSlot"] = selectedSlot_;
		}

		void FromJson(const nlohmann::json& inJson) override
		{
			ActorComponent::FromJson(inJson);
			if (inJson.contains("Slots") && inJson["Slots"].is_array())
			{
				for (std::size_t i = 0; i < slots_.size() && i < inJson["Slots"].size(); ++i) slots_[i] = inJson["Slots"][i].get<int>();
			}
			selectedSlot_ = std::clamp(inJson.value("SelectedSlot", selectedSlot_), 0, static_cast<int>(slots_.size()) - 1);
			pendingSlot_ = -1;
			pendingCycleDirection_ = 0;
		}

		void RequestSelectSlot(int slotIndex) { pendingSlot_ = slotIndex; }
		void RequestCycle(int direction) { pendingCycleDirection_ = direction < 0 ? -1 : (direction > 0 ? 1 : 0); }

		void SetSlot(int slotIndex, int weaponId)
		{
			if (slotIndex < 0 || slotIndex >= static_cast<int>(slots_.size())) return;
			slots_[slotIndex] = weaponId;
			if (slotIndex == selectedSlot_) SyncSelectedWeapon();
		}

		void ResetInventory()
		{
			slots_ = { 0, 1, -1, -1 };
			selectedSlot_ = 0;
			pendingSlot_ = -1;
			pendingCycleDirection_ = 0;
			SyncSelectedWeapon(); // Reset時もWeaponComponentとの選択状態を即時一致させる。
		}

		int GetSelectedSlot() const { return selectedSlot_; }
		int GetSelectedWeaponId() const { return slots_[selectedSlot_]; }
		const std::array<int, 4>& GetSlots() const { return slots_; }

	private:
		bool SelectSlot(int slotIndex)
		{
			if (slotIndex < 0 || slotIndex >= static_cast<int>(slots_.size())) return false;
			if (slots_[slotIndex] < 0) return false;
			selectedSlot_ = slotIndex;
			SyncSelectedWeapon();
			return true;
		}

		bool CycleSlot(int direction)
		{
			if (direction == 0) return false;
			const int count = static_cast<int>(slots_.size());
			for (int step = 1; step <= count; ++step)
			{
				const int candidate = (selectedSlot_ + direction * step + count * 2) % count;
				if (slots_[candidate] >= 0) return SelectSlot(candidate);
			}
			return false;
		}

		void SyncSelectedWeapon()
		{
			Actor* owner = GetOwner();
			WeaponComponent* weapon = owner ? owner->GetComponent<WeaponComponent>() : nullptr;
			if (weapon) weapon->SetWeaponId(slots_[selectedSlot_]);
		}

	private:
		std::array<int, 4> slots_{ 0, 1, -1, -1 };
		int selectedSlot_ = 0;
		int pendingSlot_ = -1;
		int pendingCycleDirection_ = 0;
	};
} // namespace Ken4lowEngine
