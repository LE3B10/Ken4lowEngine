#pragma once

#include "PlayerCameraComponent.h"

#include <Actor.h>
#include <Camera.h>
#include <RigidbodyComponent.h>
#include <Scene/Actor/Character/CharacterMovementComponent.h>

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// Playerの移動入力を目標速度へ変換し、共通Character MotorとRigidbodyへ委譲するComponent。
	class PlayerMovementComponent final : public CharacterMovementComponent
	{
	public:
		/// 現在の移動入力を実際のPlayer Camera基準のXZ目標速度へ変換し、共通Motorへ渡す。
		void Update(float deltaTime) override
		{
			float x = moveInputX_;
			float z = moveInputZ_;
			const float lengthSq = x * x + z * z;
			if (lengthSq > 1.0f)
			{
				const float invLength = 1.0f / std::sqrt(lengthSq);
				x *= invLength;
				z *= invLength;
			}

			Actor* owner = GetOwner();
			const PlayerCameraComponent* playerCamera = owner ? owner->GetComponent<PlayerCameraComponent>() : nullptr;

			// 見えているPlayer Cameraの前方向をXZ平面へ落とし、WASDの基準方向を画面と一致させる。
			Vector3 forward{ 0.0f, 0.0f, 1.0f };
			if (playerCamera)
			{
				if (const Camera* camera = playerCamera->GetCamera())
				{
					forward = camera->GetForward();
				}
				else
				{
					const float yaw = playerCamera->GetYaw();
					forward = { -std::sin(yaw), 0.0f, std::cos(yaw) };
				}
			}

			forward.y = 0.0f;
			const float forwardLengthSq = forward.x * forward.x + forward.z * forward.z;
			if (forwardLengthSq <= 0.000001f)
			{
				forward = { 0.0f, 0.0f, 1.0f };
			}
			else
			{
				const float invForwardLength = 1.0f / std::sqrt(forwardLengthSq);
				forward.x *= invForwardLength;
				forward.z *= invForwardLength;
			}

			// Z+前方の左手座標系に合わせ、右方向はForwardのXZ直交ベクトルから求める。
			const Vector3 right{ forward.z, 0.0f, -forward.x };
			const float worldX = right.x * x + forward.x * z;
			const float worldZ = right.z * x + forward.z * z;

			Vector3 targetVelocity = GetVelocity();
			targetVelocity.x = worldX * moveSpeed_;
			targetVelocity.z = worldZ * moveSpeed_;
			targetVelocity.y = 0.0f; // Rigidbody使用時のY速度は重力・Jump・衝突解決が所有する。
			CharacterMovementComponent::SetVelocity(targetVelocity);

			RigidbodyComponent* rigidbodyComponent = owner ? owner->GetComponent<RigidbodyComponent>() : nullptr;
			Rigidbody* rigidbody = rigidbodyComponent ? rigidbodyComponent->GetRigidbody() : nullptr;
			if (rigidbody)
			{
				CharacterMovementComponent::Update(deltaTime); // XZは質量とDrive Forceを考慮した共通Motorで加減速する。

				if (jumpRequested_ && rigidbody->IsGrounded())
				{
					Vector3 physicalVelocity = rigidbody->GetVelocity();
					physicalVelocity.y = jumpSpeed_; // Jumpだけは瞬間的な跳躍Impulse相当としてY速度へ反映する。
					rigidbodyComponent->SetVelocity(physicalVelocity);
				}

				jumpRequested_ = false;
				return;
			}

			if (jumpRequested_) targetVelocity.y = jumpSpeed_;
			CharacterMovementComponent::SetVelocity(targetVelocity);
			jumpRequested_ = false;
			CharacterMovementComponent::Update(deltaTime);
		}

		/// 共通移動情報にPlayer用移動速度と物理接地状態を追加表示する。
		void DrawImGui() override
		{
			CharacterMovementComponent::DrawImGui();
#ifdef USE_IMGUI
			ImGui::SeparatorText("プレイヤー移動");
			ImGui::SliderFloat("移動速度", &moveSpeed_, 0.0f, 30.0f, "%.2f");
			ImGui::SliderFloat("ジャンプ速度", &jumpSpeed_, 0.0f, 30.0f, "%.2f");
			ImGui::Text("入力: %.2f, %.2f", moveInputX_, moveInputZ_);
			ImGui::Text("Grounded: %s", IsGrounded() ? "Yes" : "No");
#endif
		}

		/// JSON保存・復元で使用するComponent識別名を返す。
		std::string GetClassTypeName() const override { return "PlayerMovementComponent"; }

		/// Player固有の移動速度とJump速度をActor JSONへ保存する。
		void ToJson(nlohmann::json& outJson) const override
		{
			CharacterMovementComponent::ToJson(outJson);
			outJson["MoveSpeed"] = moveSpeed_;
			outJson["JumpSpeed"] = jumpSpeed_;
		}

		/// Actor JSONからPlayer固有の移動設定を復元する。
		void FromJson(const nlohmann::json& inJson) override
		{
			CharacterMovementComponent::FromJson(inJson);
			moveSpeed_ = inJson.value("MoveSpeed", moveSpeed_);
			jumpSpeed_ = inJson.value("JumpSpeed", jumpSpeed_);
			if (!std::isfinite(moveSpeed_)) moveSpeed_ = 6.0f;
			if (!std::isfinite(jumpSpeed_)) jumpSpeed_ = 7.0f;
			moveSpeed_ = (std::max)(0.0f, moveSpeed_);
			jumpSpeed_ = (std::max)(0.0f, jumpSpeed_);
			jumpRequested_ = false;
		}

		/// 入力Componentから受け取った移動要求を保持する。
		void SetMoveInput(float x, float z)
		{
			moveInputX_ = std::clamp(x, -1.0f, 1.0f);
			moveInputZ_ = std::clamp(z, -1.0f, 1.0f);
		}

		/// Input Componentから配送されたJump要求を次のMovement更新で処理する。
		void RequestJump() { jumpRequested_ = true; }

		/// Rigidbodyが存在する場合はPhysicsWorldの接地状態を返す。
		bool IsGrounded() const
		{
			Actor* owner = GetOwner();
			RigidbodyComponent* rigidbodyComponent = owner ? owner->GetComponent<RigidbodyComponent>() : nullptr;
			Rigidbody* rigidbody = rigidbodyComponent ? rigidbodyComponent->GetRigidbody() : nullptr;
			return rigidbody && rigidbody->IsGrounded();
		}

		/// 移動要求と現在の水平速度を停止状態へ戻す。
		void ResetMovement()
		{
			moveInputX_ = 0.0f;
			moveInputZ_ = 0.0f;
			jumpRequested_ = false;

			Actor* owner = GetOwner();
			if (RigidbodyComponent* rigidbodyComponent = owner ? owner->GetComponent<RigidbodyComponent>() : nullptr)
			{
				if (Rigidbody* rigidbody = rigidbodyComponent->GetRigidbody())
				{
					Vector3 velocity = rigidbody->GetVelocity();
					velocity.x = 0.0f;
					velocity.z = 0.0f;
					rigidbodyComponent->SetVelocity(velocity); // Resetは検証初期化なので水平速度を即時停止する。
				}
			}

			Stop();
			SetMovementEnabled(true);
		}

		float GetMoveInputX() const { return moveInputX_; }
		float GetMoveInputZ() const { return moveInputZ_; }
		float GetMoveSpeed() const { return moveSpeed_; }
		float GetJumpSpeed() const { return jumpSpeed_; }

	private:
		float moveInputX_ = 0.0f;
		float moveInputZ_ = 0.0f;
		float moveSpeed_ = 6.0f;
		float jumpSpeed_ = 7.0f;
		bool jumpRequested_ = false;
	};
} // namespace Ken4lowEngine
