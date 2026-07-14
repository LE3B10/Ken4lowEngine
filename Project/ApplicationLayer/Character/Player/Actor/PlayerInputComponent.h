#pragma once

#include "InventoryComponent.h"
#include "PlayerCameraComponent.h"
#include "PlayerMovementComponent.h"
#include "WeaponComponent.h"

#include <Actor.h>
#include <ActorComponent.h>

#include <algorithm>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// Player入力を具体的なゲーム処理へ変換せず、対応する専用Componentへ要求だけを配送するComponent。
	class PlayerInputComponent final : public ActorComponent
	{
	public:
		/// 保持中の移動入力と1フレーム要求を各専用Componentへ配送する。
		void Update(float deltaTime) override
		{
			(void)deltaTime;
			Actor* owner = GetOwner();
			if (!owner)
			{
				ClearTransientRequests();
				return;
			}

			PlayerMovementComponent* movement = owner->GetComponent<PlayerMovementComponent>();
			if (movement) movement->SetMoveInput(inputEnabled_ ? moveX_ : 0.0f, inputEnabled_ ? moveZ_ : 0.0f);

			if (inputEnabled_)
			{
				if (fireRequested_)
				{
					if (WeaponComponent* weapon = owner->GetComponent<WeaponComponent>()) weapon->RequestFire();
				}
				if (reloadRequested_)
				{
					if (WeaponComponent* weapon = owner->GetComponent<WeaponComponent>()) weapon->RequestReload();
				}
				if (inventorySlotRequested_ >= 0)
				{
					if (InventoryComponent* inventory = owner->GetComponent<InventoryComponent>()) inventory->RequestSelectSlot(inventorySlotRequested_);
				}
				if (lookYawDelta_ != 0.0f || lookPitchDelta_ != 0.0f)
				{
					if (PlayerCameraComponent* camera = owner->GetComponent<PlayerCameraComponent>()) camera->RequestLook(lookYawDelta_, lookPitchDelta_);
				}
			}

			ClearTransientRequests(); // 入力Componentは要求を一度配送したら具体処理の完了状態を保持しない。
		}

		/// 入力受付状態と未配送要求をDebug表示する。
		void DrawImGui() override
		{
#ifdef USE_IMGUI
			ImGui::SeparatorText("プレイヤー入力要求");
			ImGui::Text("Enabled: %s", inputEnabled_ ? "ON" : "OFF");
			ImGui::Text("Move: %.2f, %.2f", moveX_, moveZ_);
			ImGui::Text("Fire: %s / Reload: %s / Slot: %d", fireRequested_ ? "Yes" : "No", reloadRequested_ ? "Yes" : "No", inventorySlotRequested_);
#endif
		}

		/// JSON保存・復元で使用するComponent識別名を返す。
		std::string GetClassTypeName() const override { return "PlayerInputComponent"; }

		/// 入力受付の有効状態だけをActor JSONへ保存し、フレーム要求は保存しない。
		void ToJson(nlohmann::json& outJson) const override
		{
			ActorComponent::ToJson(outJson);
			outJson["InputEnabled"] = inputEnabled_;
		}

		/// Actor JSONから入力受付状態を復元し、未配送要求を破棄する。
		void FromJson(const nlohmann::json& inJson) override
		{
			ActorComponent::FromJson(inJson);
			inputEnabled_ = inJson.value("InputEnabled", inputEnabled_);
			moveX_ = 0.0f;
			moveZ_ = 0.0f;
			ClearTransientRequests();
		}

		/// 移動要求を更新する。値は配送されるまで保持する。
		void RequestMove(float x, float z)
		{
			moveX_ = std::clamp(x, -1.0f, 1.0f);
			moveZ_ = std::clamp(z, -1.0f, 1.0f);
		}

		/// 1フレーム分の視点回転要求を追加する。
		void RequestLook(float yawDelta, float pitchDelta)
		{
			lookYawDelta_ += yawDelta;
			lookPitchDelta_ += pitchDelta;
		}

		/// 1回分の射撃要求を予約する。
		void RequestFire() { fireRequested_ = true; }

		/// 1回分のリロード要求を予約する。
		void RequestReload() { reloadRequested_ = true; }

		/// Inventoryのスロット選択要求を予約する。
		void RequestInventorySlot(int slotIndex) { inventorySlotRequested_ = slotIndex; }

		/// 死亡や操作ロック時の入力受付を切り替える。
		void SetInputEnabled(bool enabled)
		{
			inputEnabled_ = enabled;
			if (!enabled)
			{
				moveX_ = 0.0f;
				moveZ_ = 0.0f;
				ClearTransientRequests();
			}
		}

		bool IsInputEnabled() const { return inputEnabled_; }
		float GetMoveX() const { return moveX_; }
		float GetMoveZ() const { return moveZ_; }

	private:
		void ClearTransientRequests()
		{
			lookYawDelta_ = 0.0f;
			lookPitchDelta_ = 0.0f;
			fireRequested_ = false;
			reloadRequested_ = false;
			inventorySlotRequested_ = -1;
		}

	private:
		float moveX_ = 0.0f;
		float moveZ_ = 0.0f;
		float lookYawDelta_ = 0.0f;
		float lookPitchDelta_ = 0.0f;
		bool fireRequested_ = false;
		bool reloadRequested_ = false;
		int inventorySlotRequested_ = -1;
		bool inputEnabled_ = true;
	};
} // namespace Ken4lowEngine
