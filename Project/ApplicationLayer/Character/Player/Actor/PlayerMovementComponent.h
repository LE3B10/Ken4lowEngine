#pragma once

#include "PlayerCameraComponent.h"
#include "WeaponComponent.h"

#include <Actor.h>
#include <Camera.h>
#include <PhysicsCollisionLayer.h>
#include <RigidbodyComponent.h>
#include <Scene/Actor/Character/CharacterColliderComponent.h>
#include <Scene/Actor/Character/CharacterMovementComponent.h>

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// Playerの移動入力を目標速度へ変換し、Sprint・Blink・Jumpを共通Character MotorとRigidbodyへ委譲するComponent。
	class PlayerMovementComponent final : public CharacterMovementComponent
	{
	public:
		void Initialize() override
		{
			CharacterMovementComponent::Initialize();
			NormalizePlayerColliderLayout(); // 旧Prefabの巨大Colliderや中心オフセットを実行時へ持ち越さない。
		}

		void Update(float deltaTime) override
		{
			const float safeDeltaTime = (std::max)(0.0f, deltaTime);
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
			const WeaponComponent* weapon = owner ? owner->GetComponent<WeaponComponent>() : nullptr;
			const bool isReloading = weapon && weapon->IsReloading();
			Vector3 forward{ 0.0f, 0.0f, 1.0f };
			if (playerCamera)
			{
				if (const Camera* camera = playerCamera->GetCamera()) forward = camera->GetForward();
				else
				{
					const float yaw = playerCamera->GetYaw();
					forward = { -std::sin(yaw), 0.0f, std::cos(yaw) };
				}
			}
			forward = NormalizeXZOrDefault(forward, { 0.0f, 0.0f, 1.0f });

			const Vector3 right{ forward.z, 0.0f, -forward.x };
			float worldX = right.x * x + forward.x * z;
			float worldZ = right.z * x + forward.z * z;

			blinkCooldownRemaining_ = (std::max)(0.0f, blinkCooldownRemaining_ - safeDeltaTime);
			if (blinkRequested_ && blinkCooldownRemaining_ <= 0.0f && blinkRemaining_ <= 0.0f && !isReloading)
			{
				Vector3 requestedDirection{ worldX, 0.0f, worldZ };
				blinkDirection_ = NormalizeXZOrDefault(requestedDirection, forward);
				blinkRemaining_ = blinkDuration_;
				blinkCooldownRemaining_ = blinkCooldown_;
			}
			blinkRequested_ = false;

			const bool blinking = blinkRemaining_ > 0.0f;
			if (blinking)
			{
				worldX = blinkDirection_.x * blinkSpeed_;
				worldZ = blinkDirection_.z * blinkSpeed_;
				blinkRemaining_ = (std::max)(0.0f, blinkRemaining_ - safeDeltaTime);
			}
			else
			{
				const float sprintMultiplier = (sprintHeld_ && !isReloading) ? sprintSpeedMultiplier_ : 1.0f;
				const float actionMultiplier = isReloading ? reloadSpeedMultiplier_ : 1.0f;
				const float speedMultiplier = sprintMultiplier * actionMultiplier;
				worldX *= moveSpeed_ * speedMultiplier;
				worldZ *= moveSpeed_ * speedMultiplier; // 旧Playerと同じくReload中は移動速度を0.65倍へ落とす。
			}

			Vector3 targetVelocity = GetVelocity();
			targetVelocity.x = worldX;
			targetVelocity.z = worldZ;
			targetVelocity.y = 0.0f;
			CharacterMovementComponent::SetVelocity(targetVelocity);

			RigidbodyComponent* rigidbodyComponent = owner ? owner->GetComponent<RigidbodyComponent>() : nullptr;
			Rigidbody* rigidbody = rigidbodyComponent ? rigidbodyComponent->GetRigidbody() : nullptr;
			if (rigidbody)
			{
				CharacterMovementComponent::Update(safeDeltaTime);
				if (jumpRequested_ && rigidbody->IsGrounded() && !isReloading)
				{
					Vector3 physicalVelocity = rigidbody->GetVelocity();
					physicalVelocity.y = jumpSpeed_; // JumpだけY速度を書き換え、重力・落下速度はPhysicsの正本を維持する。
					rigidbodyComponent->SetVelocity(physicalVelocity);
				}
				jumpRequested_ = false;
				return;
			}

			if (jumpRequested_ && !isReloading) targetVelocity.y = jumpSpeed_;
			CharacterMovementComponent::SetVelocity(targetVelocity);
			jumpRequested_ = false;
			CharacterMovementComponent::Update(safeDeltaTime);
		}

		void DrawImGui() override
		{
			CharacterMovementComponent::DrawImGui();
#ifdef USE_IMGUI
			ImGui::SeparatorText("プレイヤー移動");
			ImGui::SliderFloat("移動速度", &moveSpeed_, 0.0f, 30.0f, "%.2f");
			ImGui::SliderFloat("Sprint倍率", &sprintSpeedMultiplier_, 1.0f, 3.0f, "%.2f");
			ImGui::SliderFloat("Reload移動倍率", &reloadSpeedMultiplier_, 0.1f, 1.0f, "%.2f");
			ImGui::SliderFloat("ジャンプ速度", &jumpSpeed_, 0.0f, 30.0f, "%.2f");
			ImGui::SliderFloat("Blink速度", &blinkSpeed_, 1.0f, 60.0f, "%.2f");
			ImGui::SliderFloat("Blink時間", &blinkDuration_, 0.01f, 1.0f, "%.3f");
			ImGui::SliderFloat("Blinkクールダウン", &blinkCooldown_, 0.0f, 5.0f, "%.2f");
			Actor* owner = GetOwner();
			const WeaponComponent* weapon = owner ? owner->GetComponent<WeaponComponent>() : nullptr;
			ImGui::Text("入力: %.2f, %.2f / Sprint: %s / Reload: %s / Blink: %s", moveInputX_, moveInputZ_, sprintHeld_ ? "Yes" : "No", weapon && weapon->IsReloading() ? "Yes" : "No", IsBlinking() ? "Yes" : "No");
			ImGui::Text("Grounded: %s", IsGrounded() ? "Yes" : "No");

			const SceneComponent* root = owner ? owner->GetRootComponent() : nullptr;
			const CharacterColliderComponent* collider = owner ? owner->GetComponent<CharacterColliderComponent>() : nullptr;
			const Collider* physicsCollider = collider ? collider->GetCollider() : nullptr;
			const float rootY = root ? root->GetWorldPosition().y : 0.0f;
			const float colliderCenterY = physicsCollider ? physicsCollider->GetCenterPosition().y : 0.0f;
			const float colliderHalfHeight = collider ? collider->GetHalfSize().y : 0.0f;
			ImGui::SeparatorText("移動 / Collider基準");
			ImGui::Text("Root Y: %.3f", rootY);
			ImGui::Text("Collider Center Y: %.3f", colliderCenterY);
			ImGui::Text("Collider Bottom Y: %.3f", colliderCenterY - colliderHalfHeight);
			ImGui::Text("Center Offset Y: %.3f", colliderCenterY - rootY);
#endif
		}

		std::string GetClassTypeName() const override { return "PlayerMovementComponent"; }

		void ToJson(nlohmann::json& outJson) const override
		{
			CharacterMovementComponent::ToJson(outJson);
			outJson["MoveSpeed"] = moveSpeed_;
			outJson["SprintSpeedMultiplier"] = sprintSpeedMultiplier_;
			outJson["ReloadSpeedMultiplier"] = reloadSpeedMultiplier_;
			outJson["JumpSpeed"] = jumpSpeed_;
			outJson["BlinkSpeed"] = blinkSpeed_;
			outJson["BlinkDuration"] = blinkDuration_;
			outJson["BlinkCooldown"] = blinkCooldown_;
		}

		void FromJson(const nlohmann::json& inJson) override
		{
			CharacterMovementComponent::FromJson(inJson);
			moveSpeed_ = inJson.value("MoveSpeed", moveSpeed_);
			sprintSpeedMultiplier_ = inJson.value("SprintSpeedMultiplier", sprintSpeedMultiplier_);
			reloadSpeedMultiplier_ = inJson.value("ReloadSpeedMultiplier", reloadSpeedMultiplier_);
			jumpSpeed_ = inJson.value("JumpSpeed", jumpSpeed_);
			blinkSpeed_ = inJson.value("BlinkSpeed", blinkSpeed_);
			blinkDuration_ = inJson.value("BlinkDuration", blinkDuration_);
			blinkCooldown_ = inJson.value("BlinkCooldown", blinkCooldown_);
			if (!std::isfinite(moveSpeed_)) moveSpeed_ = 6.0f;
			if (!std::isfinite(sprintSpeedMultiplier_)) sprintSpeedMultiplier_ = 1.55f;
			if (!std::isfinite(reloadSpeedMultiplier_)) reloadSpeedMultiplier_ = 0.65f;
			if (!std::isfinite(jumpSpeed_)) jumpSpeed_ = 7.0f;
			if (!std::isfinite(blinkSpeed_)) blinkSpeed_ = 18.0f;
			if (!std::isfinite(blinkDuration_)) blinkDuration_ = 0.15f;
			if (!std::isfinite(blinkCooldown_)) blinkCooldown_ = 0.75f;
			moveSpeed_ = (std::max)(0.0f, moveSpeed_);
			sprintSpeedMultiplier_ = (std::max)(1.0f, sprintSpeedMultiplier_);
			reloadSpeedMultiplier_ = std::clamp(reloadSpeedMultiplier_, 0.1f, 1.0f);
			jumpSpeed_ = (std::max)(0.0f, jumpSpeed_);
			blinkSpeed_ = (std::max)(0.0f, blinkSpeed_);
			blinkDuration_ = (std::max)(0.01f, blinkDuration_);
			blinkCooldown_ = (std::max)(0.0f, blinkCooldown_);
			ResetTransientMovementState();
		}

		void SetMoveInput(float x, float z)
		{
			moveInputX_ = (std::clamp)(x, -1.0f, 1.0f);
			moveInputZ_ = (std::clamp)(z, -1.0f, 1.0f);
		}
		void SetSprintHeld(bool held) { sprintHeld_ = held; }
		void RequestJump() { jumpRequested_ = true; }
		void RequestBlink() { blinkRequested_ = true; }

		bool IsGrounded() const
		{
			Actor* owner = GetOwner();
			RigidbodyComponent* rigidbodyComponent = owner ? owner->GetComponent<RigidbodyComponent>() : nullptr;
			Rigidbody* rigidbody = rigidbodyComponent ? rigidbodyComponent->GetRigidbody() : nullptr;
			return rigidbody && rigidbody->IsGrounded();
		}
		bool IsSprinting() const { return sprintHeld_ && !IsBlinking(); }
		bool IsBlinking() const { return blinkRemaining_ > 0.0f; }

		void ResetMovement()
		{
			ResetTransientMovementState();
			Actor* owner = GetOwner();
			if (RigidbodyComponent* rigidbodyComponent = owner ? owner->GetComponent<RigidbodyComponent>() : nullptr)
			{
				if (Rigidbody* rigidbody = rigidbodyComponent->GetRigidbody())
				{
					Vector3 velocity = rigidbody->GetVelocity();
					velocity.x = 0.0f;
					velocity.z = 0.0f;
					rigidbodyComponent->SetVelocity(velocity);
				}
			}
			Stop();
			SetMovementEnabled(true);
		}

		float GetMoveInputX() const { return moveInputX_; }
		float GetMoveInputZ() const { return moveInputZ_; }
		float GetMoveSpeed() const { return moveSpeed_; }
		float GetJumpSpeed() const { return jumpSpeed_; }
		float GetReloadSpeedMultiplier() const { return reloadSpeedMultiplier_; }

	private:
		static Vector3 NormalizeXZOrDefault(const Vector3& value, const Vector3& fallback)
		{
			const float lengthSq = value.x * value.x + value.z * value.z;
			if (lengthSq <= 0.000001f) return fallback;
			const float invLength = 1.0f / std::sqrt(lengthSq);
			return { value.x * invLength, 0.0f, value.z * invLength };
		}

		void NormalizePlayerColliderLayout()
		{
			Actor* owner = GetOwner();
			CharacterColliderComponent* collider = owner ? owner->GetComponent<CharacterColliderComponent>() : nullptr;
			if (!collider) return;
			collider->SetShapeType(ECollisionShapeType::AABB);
			collider->SetLocalPosition({ 0.0f, 0.0f, 0.0f });
			collider->SetLocalRotation({ 0.0f, 0.0f, 0.0f });
			collider->SetHalfSize({ kColliderHalfWidth, kColliderHalfHeight, kColliderHalfDepth });
			collider->SetCollisionLayer(PhysicsCollisionLayer::DynamicActor);
			collider->RefreshWorldTransform();
		}

		void ResetTransientMovementState()
		{
			moveInputX_ = 0.0f;
			moveInputZ_ = 0.0f;
			sprintHeld_ = false;
			jumpRequested_ = false;
			blinkRequested_ = false;
			blinkRemaining_ = 0.0f;
			blinkCooldownRemaining_ = 0.0f;
			blinkDirection_ = { 0.0f, 0.0f, 1.0f };
		}

	private:
		static constexpr float kColliderHalfWidth = 0.45f;
		static constexpr float kColliderHalfHeight = 0.90f;
		static constexpr float kColliderHalfDepth = 0.45f;
		float moveInputX_ = 0.0f;
		float moveInputZ_ = 0.0f;
		float moveSpeed_ = 6.0f;
		float sprintSpeedMultiplier_ = 1.55f;
		float reloadSpeedMultiplier_ = 0.65f;
		float jumpSpeed_ = 7.0f;
		float blinkSpeed_ = 18.0f;
		float blinkDuration_ = 0.15f;
		float blinkCooldown_ = 0.75f;
		float blinkRemaining_ = 0.0f;
		float blinkCooldownRemaining_ = 0.0f;
		Vector3 blinkDirection_{ 0.0f, 0.0f, 1.0f };
		bool sprintHeld_ = false;
		bool jumpRequested_ = false;
		bool blinkRequested_ = false;
	};
} // namespace Ken4lowEngine
