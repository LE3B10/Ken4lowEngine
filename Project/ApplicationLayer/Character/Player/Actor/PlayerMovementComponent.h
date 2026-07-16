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
	/// Playerの通常移動・Ladder・Jump・Blink・Fall Damage・Damage KnockbackをRigidbodyへ反映するComponent。
	class PlayerMovementComponent final : public CharacterMovementComponent
	{
	public:
		void Initialize() override
		{
			CharacterMovementComponent::Initialize();
			NormalizePlayerColliderLayout();
			ResetTransientMovementState();
		}

		void Update(float deltaTime) override
		{
			const float safeDeltaTime = (std::max)(0.0f, deltaTime);
			Actor* owner = GetOwner();
			RigidbodyComponent* rigidbodyComponent = owner ? owner->GetComponent<RigidbodyComponent>() : nullptr;
			Rigidbody* rigidbody = rigidbodyComponent ? rigidbodyComponent->GetRigidbody() : nullptr;
			UpdateFallState(owner, rigidbody);

			const WeaponComponent* weapon = owner ? owner->GetComponent<WeaponComponent>() : nullptr;
			const bool actionLocked = weapon && (weapon->IsReloading() || weapon->IsEquipAnimating());

			if (HandleLadderMovement(safeDeltaTime, rigidbodyComponent, rigidbody)) return;
			if (rigidbodyComponent) rigidbodyComponent->SetUseGravity(true);

			float x = moveInputX_;
			float z = moveInputZ_;
			const float lengthSq = x * x + z * z;
			if (lengthSq > 1.0f)
			{
				const float invLength = 1.0f / std::sqrt(lengthSq);
				x *= invLength;
				z *= invLength;
			}

			const PlayerCameraComponent* playerCamera = owner ? owner->GetComponent<PlayerCameraComponent>() : nullptr;
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
			if (blinkRequested_ && blinkCooldownRemaining_ <= 0.0f && blinkRemaining_ <= 0.0f && !actionLocked)
			{
				blinkDirection_ = NormalizeXZOrDefault({ worldX, 0.0f, worldZ }, forward);
				blinkRemaining_ = blinkDuration_;
				blinkCooldownRemaining_ = blinkCooldown_;
			}
			blinkRequested_ = false;

			if (blinkRemaining_ > 0.0f)
			{
				worldX = blinkDirection_.x * blinkSpeed_;
				worldZ = blinkDirection_.z * blinkSpeed_;
				blinkRemaining_ = (std::max)(0.0f, blinkRemaining_ - safeDeltaTime);
			}
			else
			{
				const float sprintMultiplier = (sprintHeld_ && !actionLocked) ? sprintSpeedMultiplier_ : 1.0f;
				const float actionMultiplier = actionLocked ? reloadSpeedMultiplier_ : 1.0f;
				worldX *= moveSpeed_ * sprintMultiplier * actionMultiplier;
				worldZ *= moveSpeed_ * sprintMultiplier * actionMultiplier;
			}

			if (knockbackRemaining_ > 0.0f)
			{
				worldX = knockbackVelocity_.x;
				worldZ = knockbackVelocity_.z;
				const float decay = std::exp(-knockbackDecay_ * safeDeltaTime);
				knockbackVelocity_.x *= decay;
				knockbackVelocity_.z *= decay;
				knockbackRemaining_ = (std::max)(0.0f, knockbackRemaining_ - safeDeltaTime);
			}

			Vector3 targetVelocity = GetVelocity();
			targetVelocity.x = worldX;
			targetVelocity.z = worldZ;
			targetVelocity.y = 0.0f;
			CharacterMovementComponent::SetVelocity(targetVelocity);

			if (rigidbody)
			{
				CharacterMovementComponent::Update(safeDeltaTime);
				if (jumpRequested_ && rigidbody->IsGrounded() && !actionLocked)
				{
					Vector3 physicalVelocity = rigidbody->GetVelocity();
					physicalVelocity.y = jumpSpeed_;
					rigidbodyComponent->SetVelocity(physicalVelocity); // JumpだけY速度を書き換え、落下中の重力速度はPhysicsへ残す。
				}
				jumpRequested_ = false;
				return;
			}

			if (jumpRequested_ && !actionLocked) targetVelocity.y = jumpSpeed_;
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
			ImGui::SliderFloat("Action移動倍率", &reloadSpeedMultiplier_, 0.1f, 1.0f, "%.2f");
			ImGui::SliderFloat("ジャンプ速度", &jumpSpeed_, 0.0f, 30.0f, "%.2f");
			ImGui::SliderFloat("Ladder昇降速度", &ladderClimbSpeed_, 0.1f, 12.0f, "%.2f");
			ImGui::SliderFloat("Blink速度", &blinkSpeed_, 1.0f, 60.0f, "%.2f");
			ImGui::SliderFloat("Blink時間", &blinkDuration_, 0.01f, 1.0f, "%.3f");
			ImGui::SliderFloat("Blinkクールダウン", &blinkCooldown_, 0.0f, 5.0f, "%.2f");
			ImGui::SeparatorText("Fall Damage / Knockback");
			ImGui::SliderFloat("安全落下距離", &safeFallDistance_, 0.0f, 20.0f, "%.2f");
			ImGui::SliderFloat("落下ダメージ倍率", &fallDamagePerUnit_, 0.0f, 30.0f, "%.2f");
			ImGui::SliderFloat("Knockback時間", &knockbackDuration_, 0.0f, 1.0f, "%.2f");
			ImGui::Text("Ladder Area:%s Climbing:%s / Fall:%.2f Impact:%.2f / Pending Damage:%.1f",
				isInLadderArea_ ? "Yes" : "No", isClimbingLadder_ ? "Yes" : "No", lastFallDistance_, maxDownwardSpeed_, pendingFallDamage_);
			ImGui::Text("Grounded: %s / Knockback: %.2f", IsGrounded() ? "Yes" : "No", knockbackRemaining_);
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
			outJson["LadderClimbSpeed"] = ladderClimbSpeed_;
			outJson["BlinkSpeed"] = blinkSpeed_;
			outJson["BlinkDuration"] = blinkDuration_;
			outJson["BlinkCooldown"] = blinkCooldown_;
			outJson["SafeFallDistance"] = safeFallDistance_;
			outJson["FallDamagePerUnit"] = fallDamagePerUnit_;
			outJson["FallKillY"] = fallKillY_;
			outJson["KnockbackDuration"] = knockbackDuration_;
			outJson["KnockbackDecay"] = knockbackDecay_;
		}

		void FromJson(const nlohmann::json& inJson) override
		{
			CharacterMovementComponent::FromJson(inJson);
			moveSpeed_ = Sanitize(inJson.value("MoveSpeed", moveSpeed_), 6.0f, 0.0f);
			sprintSpeedMultiplier_ = Sanitize(inJson.value("SprintSpeedMultiplier", sprintSpeedMultiplier_), 1.55f, 1.0f);
			reloadSpeedMultiplier_ = std::clamp(Sanitize(inJson.value("ReloadSpeedMultiplier", reloadSpeedMultiplier_), 0.65f, 0.1f), 0.1f, 1.0f);
			jumpSpeed_ = Sanitize(inJson.value("JumpSpeed", jumpSpeed_), 7.0f, 0.0f);
			ladderClimbSpeed_ = Sanitize(inJson.value("LadderClimbSpeed", ladderClimbSpeed_), 3.0f, 0.1f);
			blinkSpeed_ = Sanitize(inJson.value("BlinkSpeed", blinkSpeed_), 18.0f, 0.0f);
			blinkDuration_ = Sanitize(inJson.value("BlinkDuration", blinkDuration_), 0.15f, 0.01f);
			blinkCooldown_ = Sanitize(inJson.value("BlinkCooldown", blinkCooldown_), 0.75f, 0.0f);
			safeFallDistance_ = Sanitize(inJson.value("SafeFallDistance", safeFallDistance_), 5.0f, 0.0f);
			fallDamagePerUnit_ = Sanitize(inJson.value("FallDamagePerUnit", fallDamagePerUnit_), 12.0f, 0.0f);
			fallKillY_ = std::isfinite(inJson.value("FallKillY", fallKillY_)) ? inJson.value("FallKillY", fallKillY_) : -30.0f;
			knockbackDuration_ = Sanitize(inJson.value("KnockbackDuration", knockbackDuration_), 0.28f, 0.0f);
			knockbackDecay_ = Sanitize(inJson.value("KnockbackDecay", knockbackDecay_), 8.0f, 0.0f);
			ResetTransientMovementState();
		}

		void SetMoveInput(float x, float z)
		{
			moveInputX_ = std::clamp(x, -1.0f, 1.0f);
			moveInputZ_ = std::clamp(z, -1.0f, 1.0f);
		}
		void SetSprintHeld(bool held) { sprintHeld_ = held; }
		void RequestJump() { jumpRequested_ = true; }
		void RequestBlink() { blinkRequested_ = true; }

		void SetLadderState(bool inLadderArea)
		{
			if (!inLadderArea)
			{
				isInLadderArea_ = false;
				isClimbingLadder_ = false;
				ladderDetachLocked_ = false;
				return;
			}
			if (!ladderDetachLocked_) isInLadderArea_ = true;
		}

		void ApplyDamageKnockback(const Vector3& direction, float horizontalPower = 6.0f, float verticalPower = 2.0f)
		{
			const Vector3 normalized = NormalizeXZOrDefault(direction, { 0.0f, 0.0f, 1.0f });
			knockbackVelocity_ = { normalized.x * (std::max)(0.0f, horizontalPower), 0.0f, normalized.z * (std::max)(0.0f, horizontalPower) };
			knockbackRemaining_ = knockbackDuration_;
			Actor* owner = GetOwner();
			RigidbodyComponent* rigidbodyComponent = owner ? owner->GetComponent<RigidbodyComponent>() : nullptr;
			Rigidbody* rigidbody = rigidbodyComponent ? rigidbodyComponent->GetRigidbody() : nullptr;
			if (rigidbody)
			{
				Vector3 physicalVelocity = rigidbody->GetVelocity();
				physicalVelocity.x = knockbackVelocity_.x;
				physicalVelocity.z = knockbackVelocity_.z;
				physicalVelocity.y = (std::max)(physicalVelocity.y, verticalPower);
				rigidbodyComponent->SetVelocity(physicalVelocity); // 被弾フレームだけ即時反映し、その後はComponent内で減衰させる。
			}
		}

		float ConsumePendingFallDamage()
		{
			const float damage = pendingFallDamage_;
			pendingFallDamage_ = 0.0f;
			return damage;
		}

		bool IsGrounded() const
		{
			Actor* owner = GetOwner();
			RigidbodyComponent* rigidbodyComponent = owner ? owner->GetComponent<RigidbodyComponent>() : nullptr;
			Rigidbody* rigidbody = rigidbodyComponent ? rigidbodyComponent->GetRigidbody() : nullptr;
			return rigidbody && rigidbody->IsGrounded();
		}
		bool IsSprinting() const { return sprintHeld_ && !IsBlinking() && !isInLadderArea_; }
		bool IsBlinking() const { return blinkRemaining_ > 0.0f; }
		bool IsInLadderArea() const { return isInLadderArea_; }
		bool IsClimbingLadder() const { return isClimbingLadder_; }

		void ResetMovement()
		{
			ResetTransientMovementState();
			Actor* owner = GetOwner();
			if (RigidbodyComponent* rigidbodyComponent = owner ? owner->GetComponent<RigidbodyComponent>() : nullptr)
			{
				rigidbodyComponent->SetUseGravity(true);
				rigidbodyComponent->SetVelocity({});
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
		bool HandleLadderMovement(float deltaTime, RigidbodyComponent* rigidbodyComponent, Rigidbody* rigidbody)
		{
			if (!isInLadderArea_ || !rigidbodyComponent || !rigidbody) return false;
			blinkRemaining_ = 0.0f;
			const bool descendHeld = sprintHeld_ || blinkRequested_;
			blinkRequested_ = false;

			if (jumpRequested_)
			{
				isInLadderArea_ = false;
				isClimbingLadder_ = false;
				ladderDetachLocked_ = true;
				rigidbodyComponent->SetUseGravity(true);
				Vector3 velocity = rigidbody->GetVelocity();
				velocity.y = jumpSpeed_;
				rigidbodyComponent->SetVelocity(velocity);
				jumpRequested_ = false;
				fallTracking_ = true;
				wasGrounded_ = false;
				return false;
			}

			rigidbodyComponent->SetUseGravity(false);
			const float climbInput = descendHeld ? -1.0f : std::clamp(moveInputZ_, -1.0f, 1.0f);
			isClimbingLadder_ = std::fabs(climbInput) > 0.01f;
			CharacterMovementComponent::SetVelocity({});
			CharacterMovementComponent::Update(deltaTime);
			Vector3 velocity = rigidbody->GetVelocity();
			velocity.x = 0.0f;
			velocity.z = 0.0f;
			velocity.y = climbInput * ladderClimbSpeed_;
			rigidbodyComponent->SetVelocity(velocity);
			jumpRequested_ = false;
			fallTracking_ = false;
			maxDownwardSpeed_ = 0.0f;
			wasGrounded_ = false;
			return true;
		}

		void UpdateFallState(Actor* owner, Rigidbody* rigidbody)
		{
			const SceneComponent* root = owner ? owner->GetRootComponent() : nullptr;
			if (!root || !rigidbody) return;
			const float currentY = root->GetWorldPosition().y;
			const bool grounded = rigidbody->IsGrounded();
			const float downwardSpeed = (std::max)(0.0f, -rigidbody->GetVelocity().y);

			if (isInLadderArea_)
			{
				fallTracking_ = false;
				maxDownwardSpeed_ = 0.0f;
				wasGrounded_ = false;
				return;
			}

			if (!grounded)
			{
				if (!fallTracking_)
				{
					fallTracking_ = true;
					fallStartY_ = currentY;
					maxDownwardSpeed_ = 0.0f;
				}
				maxDownwardSpeed_ = (std::max)(maxDownwardSpeed_, downwardSpeed);
				if (currentY <= fallKillY_) pendingFallDamage_ = (std::max)(pendingFallDamage_, 10000.0f);
			}
			else if (!wasGrounded_ && fallTracking_)
			{
				lastFallDistance_ = (std::max)(0.0f, fallStartY_ - currentY);
				const float distanceDamage = (std::max)(0.0f, lastFallDistance_ - safeFallDistance_) * fallDamagePerUnit_;
				const float speedDamage = (std::max)(0.0f, maxDownwardSpeed_ - safeImpactSpeed_) * impactDamagePerSpeed_;
				pendingFallDamage_ = (std::max)(pendingFallDamage_, (std::max)(distanceDamage, speedDamage));
				fallTracking_ = false;
				maxDownwardSpeed_ = 0.0f;
			}
			wasGrounded_ = grounded;
		}

		static float Sanitize(float value, float fallback, float minimum)
		{
			return std::isfinite(value) ? (std::max)(minimum, value) : fallback;
		}

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
			isInLadderArea_ = false;
			isClimbingLadder_ = false;
			ladderDetachLocked_ = false;
			fallTracking_ = false;
			wasGrounded_ = false;
			fallStartY_ = 0.0f;
			maxDownwardSpeed_ = 0.0f;
			lastFallDistance_ = 0.0f;
			pendingFallDamage_ = 0.0f;
			knockbackVelocity_ = {};
			knockbackRemaining_ = 0.0f;
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
		float ladderClimbSpeed_ = 3.0f;
		float blinkSpeed_ = 18.0f;
		float blinkDuration_ = 0.15f;
		float blinkCooldown_ = 0.75f;
		float blinkRemaining_ = 0.0f;
		float blinkCooldownRemaining_ = 0.0f;
		Vector3 blinkDirection_{ 0.0f, 0.0f, 1.0f };
		bool sprintHeld_ = false;
		bool jumpRequested_ = false;
		bool blinkRequested_ = false;
		bool isInLadderArea_ = false;
		bool isClimbingLadder_ = false;
		bool ladderDetachLocked_ = false;
		bool fallTracking_ = false;
		bool wasGrounded_ = false;
		float fallStartY_ = 0.0f;
		float maxDownwardSpeed_ = 0.0f;
		float lastFallDistance_ = 0.0f;
		float pendingFallDamage_ = 0.0f;
		float safeFallDistance_ = 5.0f;
		float safeImpactSpeed_ = 13.0f;
		float fallDamagePerUnit_ = 12.0f;
		float impactDamagePerSpeed_ = 4.0f;
		float fallKillY_ = -30.0f;
		Vector3 knockbackVelocity_{};
		float knockbackRemaining_ = 0.0f;
		float knockbackDuration_ = 0.28f;
		float knockbackDecay_ = 8.0f;
	};
} // namespace Ken4lowEngine
