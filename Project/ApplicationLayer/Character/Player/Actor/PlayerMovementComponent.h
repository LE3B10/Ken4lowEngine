#pragma once

#include "PlayerCameraComponent.h"

#include <Actor.h>
#include <RigidbodyComponent.h>
#include <Scene/Actor/Character/CharacterMovementComponent.h>

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// Playerの移動入力を速度へ変換し、Rigidbodyがある場合は物理移動へ、無い場合は共通Character移動へ委譲するComponent。
	class PlayerMovementComponent final : public CharacterMovementComponent
	{
	public:
		/// 現在の移動入力をCamera Yaw基準のXZ速度へ変換し、物理Bodyまたは共通移動処理へ渡す。
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
			const PlayerCameraComponent* camera = owner ? owner->GetComponent<PlayerCameraComponent>() : nullptr;
			const float yaw = camera ? camera->GetYaw() : 0.0f;
			const float sinYaw = std::sin(yaw);
			const float cosYaw = std::cos(yaw);

			// +Zを前方とし、CameraのYawだけを使ってFPS操作の移動方向をWorld空間へ変換する。
			const float worldX = x * cosYaw - z * sinYaw;
			const float worldZ = x * sinYaw + z * cosYaw;

			RigidbodyComponent* rigidbodyComponent = owner ? owner->GetComponent<RigidbodyComponent>() : nullptr;
			Rigidbody* rigidbody = rigidbodyComponent ? rigidbodyComponent->GetRigidbody() : nullptr;
			if (rigidbody)
			{
				Vector3 velocity = rigidbody->GetVelocity();
				velocity.x = worldX * moveSpeed_;
				velocity.z = worldZ * moveSpeed_;

				if (jumpRequested_ && rigidbody->IsGrounded())
				{
					velocity.y = jumpSpeed_; // Jump要求はMovement側で物理速度へ変換し、Input側では具体処理を行わない。
				}

				rigidbodyComponent->SetVelocity(velocity);
				CharacterMovementComponent::SetVelocity(velocity); // Debug表示用の共通速度も同じ値へ同期する。
				jumpRequested_ = false;
				return; // Rigidbody使用時はRoot Transformを直接積分せず、PhysicsWorldの結果だけを採用する。
			}

			Vector3 velocity = GetVelocity();
			velocity.x = worldX * moveSpeed_;
			velocity.z = worldZ * moveSpeed_;
			if (jumpRequested_) velocity.y = jumpSpeed_;
			SetVelocity(velocity);
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
			moveSpeed_ = std::max(0.0f, moveSpeed_);
			jumpSpeed_ = std::max(0.0f, jumpSpeed_);
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
					rigidbodyComponent->SetVelocity(velocity); // Reset時も重力由来のY速度は保持する。
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
