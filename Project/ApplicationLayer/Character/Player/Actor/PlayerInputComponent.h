#pragma once

#include "InventoryComponent.h"
#include "PlayerCameraComponent.h"
#include "PlayerInputSnapshot.h"
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
				if (jumpRequested_ && movement) movement->RequestJump(); // InputはJump実装を持たず、Movementへ要求だけ配送する。
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
			ImGui::Text("Sprint: %s / Aim: %s / Fire Held: %s",
				frameInput_.sprintHeld ? "Yes" : "No",
				frameInput_.aimHeld ? "Yes" : "No",
				frameInput_.fireHeld ? "Yes" : "No");
			ImGui::Text("Jump: %s / Fire: %s / Reload: %s / Slot: %d",
				frameInput_.jumpPressed ? "Yes" : "No",
				frameInput_.fireHeld ? "Yes" : "No",
				frameInput_.reloadPressed ? "Yes" : "No",
				frameInput_.weaponSlotPressed);
			ImGui::Text("Restrictions: %s  Move:%s Shoot:%s Reload:%s",
				restrictionsEnabled_ ? "ON" : "OFF",
				allowMove_ ? "ON" : "OFF",
				allowShoot_ ? "ON" : "OFF",
				allowReload_ ? "ON" : "OFF");
#endif
		}

		/// JSON保存・復元で使用するComponent識別名を返す。
		std::string GetClassTypeName() const override { return "PlayerInputComponent"; }

		/// 入力受付状態だけをActor JSONへ保存し、フレーム要求と一時的な操作制限は保存しない。
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
			ResetInputState();
		}

		/// 旧Playerと同じInputSnapshotを受け取り、新Actor用の要求へ変換する段階移行入口。
		void ApplyInputSnapshot(const ::InputSnapshot& snapshot, float mouseLookSensitivity, bool allowLookInput = true)
		{
			if (!inputEnabled_)
			{
				ResetInputState();
				return;
			}

			::InputSnapshot filtered = snapshot;
			ApplyRestrictions(filtered);
			frameInput_ = filtered; // 後続Componentが同じフレーム入力を参照できるよう、制限適用後の値を保持する。

			RequestMove(filtered.moveX, filtered.moveZ);

			if (allowLookInput)
			{
				const float yawDelta = -filtered.lookMouseX * mouseLookSensitivity;
				const float pitchDelta = filtered.lookMouseY * mouseLookSensitivity;
				if (yawDelta != 0.0f || pitchDelta != 0.0f) RequestLook(yawDelta, pitchDelta);
			}

			if (filtered.jumpPressed) RequestJump();
			if (filtered.fireHeld) RequestFire();
			if (filtered.reloadPressed) RequestReload();
			if (filtered.weaponSlotPressed > 0) RequestInventorySlot(filtered.weaponSlotPressed - 1);

			blinkRequested_ = filtered.blinkPressed;
			meleeRequested_ = filtered.meleePressed;
			weaponSwitchRequested_ = filtered.weaponSwitch;
			toggleFireModeRequested_ = filtered.toggleFireModePressed;
		}

		/// 旧Tutorial制限と同じ意味で、移動・射撃・リロードの受付可否を設定する。
		void SetInputRestrictions(bool enabled, bool allowMove, bool allowShoot, bool allowReload)
		{
			restrictionsEnabled_ = enabled;
			allowMove_ = allowMove;
			allowShoot_ = allowShoot;
			allowReload_ = allowReload;
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

		/// 1回分のJump要求を予約する。
		void RequestJump() { jumpRequested_ = true; }

		/// 1回分の射撃要求を予約する。
		void RequestFire() { fireRequested_ = true; }

		/// 1回分のリロード要求を予約する。
		void RequestReload() { reloadRequested_ = true; }

		/// Inventoryのスロット選択要求を予約する。
		void RequestInventorySlot(int slotIndex) { inventorySlotRequested_ = slotIndex; }

		/// Editor操作や再配置時に、保持入力と未配送要求をすべて初期化する。
		void ResetInputState()
		{
			moveX_ = 0.0f;
			moveZ_ = 0.0f;
			frameInput_ = {};
			ClearTransientRequests();
		}

		/// 死亡や操作ロック時の入力受付を切り替える。
		void SetInputEnabled(bool enabled)
		{
			inputEnabled_ = enabled;
			if (!enabled) ResetInputState();
		}

		bool IsInputEnabled() const { return inputEnabled_; }
		float GetMoveX() const { return moveX_; }
		float GetMoveZ() const { return moveZ_; }
		const ::InputSnapshot& GetFrameInput() const { return frameInput_; }
		bool IsSprintHeld() const { return frameInput_.sprintHeld; }
		bool IsAimHeld() const { return frameInput_.aimHeld; }
		bool IsFireHeld() const { return frameInput_.fireHeld; }
		bool IsBlinkRequested() const { return frameInput_.blinkPressed; }
		bool IsMeleeRequested() const { return frameInput_.meleePressed; }
		int GetWeaponSwitchRequested() const { return frameInput_.weaponSwitch; }
		bool IsToggleFireModeRequested() const { return frameInput_.toggleFireModePressed; }

	private:
		void ApplyRestrictions(::InputSnapshot& input) const
		{
			if (!restrictionsEnabled_) return;

			input.weaponSwitch = 0;
			input.weaponSlotPressed = 0;
			input.toggleFireModePressed = false;
			input.meleePressed = false;

			if (!allowMove_)
			{
				input.moveX = 0.0f;
				input.moveZ = 0.0f;
				input.sprintHeld = false;
				input.jumpHeld = false;
				input.jumpPressed = false;
				input.blinkPressed = false;
			}
			if (!allowShoot_)
			{
				input.aimHeld = false;
				input.aimPressed = false;
				input.fireHeld = false;
				input.firePressed = false;
			}
			if (!allowReload_) input.reloadPressed = false;
		}

		void ClearTransientRequests()
		{
			lookYawDelta_ = 0.0f;
			lookPitchDelta_ = 0.0f;
			jumpRequested_ = false;
			fireRequested_ = false;
			reloadRequested_ = false;
			inventorySlotRequested_ = -1;
			blinkRequested_ = false;
			meleeRequested_ = false;
			weaponSwitchRequested_ = 0;
			toggleFireModeRequested_ = false;
		}

	private:
		::InputSnapshot frameInput_{};
		float moveX_ = 0.0f;
		float moveZ_ = 0.0f;
		float lookYawDelta_ = 0.0f;
		float lookPitchDelta_ = 0.0f;
		bool jumpRequested_ = false;
		bool fireRequested_ = false;
		bool reloadRequested_ = false;
		bool blinkRequested_ = false;
		bool meleeRequested_ = false;
		int inventorySlotRequested_ = -1;
		int weaponSwitchRequested_ = 0;
		bool toggleFireModeRequested_ = false;
		bool inputEnabled_ = true;
		bool restrictionsEnabled_ = false;
		bool allowMove_ = true;
		bool allowShoot_ = true;
		bool allowReload_ = true;
	};
} // namespace Ken4lowEngine
