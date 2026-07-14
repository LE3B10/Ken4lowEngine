#include "CharacterMovementComponent.h"

#include "Actor.h"
#include "SceneComponent.h"
#include <RigidbodyComponent.h>

#include <algorithm>
#include <cmath>
#include <numbers>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	namespace
	{
		constexpr float kDirectionEpsilon = 0.0001f;

		/// Yaw差分を-πから+πへ正規化し、常に短い向きへ回転させる。
		float WrapAngle(float angle)
		{
			constexpr float pi = std::numbers::pi_v<float>;
			constexpr float twoPi = std::numbers::pi_v<float> * 2.0f;
			angle = std::fmod(angle + pi, twoPi);
			if (angle < 0.0f) angle += twoPi;
			return angle - pi;
		}
	}

	void CharacterMovementComponent::Update(float deltaTime)
	{
		if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) return;

		Actor* owner = GetOwner();
		RigidbodyComponent* rigidbodyComponent = owner ? owner->GetComponent<RigidbodyComponent>() : nullptr;
		Rigidbody* rigidbody = rigidbodyComponent ? rigidbodyComponent->GetRigidbody() : nullptr;
		if (rigidbody)
		{
			// Rigidbody Characterは速度を即時上書きせず、massに応じた加速度上限で目標XZ速度へ近づける。
			Vector3 physicalVelocity = rigidbody->GetVelocity();
			const Vector3 targetVelocity = movementEnabled_ ? velocity_ : Vector3{};
			const float deltaX = targetVelocity.x - physicalVelocity.x;
			const float deltaZ = targetVelocity.z - physicalVelocity.z;
			const float deltaSpeed = std::sqrt(deltaX * deltaX + deltaZ * deltaZ);

			if (deltaSpeed > 0.000001f)
			{
				const float targetHorizontalSpeedSq = targetVelocity.x * targetVelocity.x + targetVelocity.z * targetVelocity.z;
				const float forceLimit = targetHorizontalSpeedSq > 0.000001f ? maxDriveForce_ : maxBrakingForce_;
				const float maxDeltaSpeed = forceLimit * std::max(rigidbody->GetInvMass(), 0.0f) * deltaTime;
				const float appliedDeltaSpeed = std::min(deltaSpeed, maxDeltaSpeed);
				const float ratio = deltaSpeed > 0.0f ? appliedDeltaSpeed / deltaSpeed : 0.0f;
				physicalVelocity.x += deltaX * ratio;
				physicalVelocity.z += deltaZ * ratio;
			}

			rigidbodyComponent->SetVelocity(physicalVelocity); // Y速度は重力・ジャンプ・衝突Impulseの結果を保持する。
			return;
		}

		if (!movementEnabled_) return;
		ApplyMovement(deltaTime);
	}

	void CharacterMovementComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("キャラクター移動");
		ComponentPropertyUtility::DrawImGui(CreateProperties());
#endif
	}

	void CharacterMovementComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson);
		ComponentPropertyUtility::ToJson(const_cast<CharacterMovementComponent*>(this)->CreateProperties(), outJson);
	}

	void CharacterMovementComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson);
		ComponentPropertyUtility::FromJson(CreateProperties(), inJson);
	}

	void CharacterMovementComponent::SetVelocity(const Vector3& velocity)
	{
		velocity_.x = std::isfinite(velocity.x) ? velocity.x : 0.0f;
		velocity_.y = std::isfinite(velocity.y) ? velocity.y : 0.0f;
		velocity_.z = std::isfinite(velocity.z) ? velocity.z : 0.0f;
	}

	void CharacterMovementComponent::SetMaxDriveForce(float force)
	{
		maxDriveForce_ = std::isfinite(force) ? std::max(0.0f, force) : 0.0f;
	}

	void CharacterMovementComponent::SetMaxBrakingForce(float force)
	{
		maxBrakingForce_ = std::isfinite(force) ? std::max(0.0f, force) : 0.0f;
	}

	Vector3 CharacterMovementComponent::CalculateDisplacement(float deltaTime) const
	{
		if (!movementEnabled_ || !std::isfinite(deltaTime) || deltaTime <= 0.0f) return {};
		return velocity_ * deltaTime;
	}

	bool CharacterMovementComponent::FaceDirectionXZ(const Vector3& direction, float rotateSpeed, float deltaTime)
	{
		Actor* owner = GetOwner();
		SceneComponent* root = owner ? owner->GetRootComponent() : nullptr;
		const float length = Vector3::LengthXZ(direction);
		if (!root || length < kDirectionEpsilon || !std::isfinite(rotateSpeed) || rotateSpeed < 0.0f || !std::isfinite(deltaTime) || deltaTime <= 0.0f) return false;

		const Vector3 normalized{ direction.x / length, 0.0f, direction.z / length };
		const float targetYaw = std::atan2(-normalized.x, normalized.z);
		Vector3 rotation = root->GetLocalRotation();
		const float maxStep = rotateSpeed * deltaTime;
		const float deltaYaw = std::clamp(WrapAngle(targetYaw - rotation.y), -maxStep, maxStep);
		rotation.y = WrapAngle(rotation.y + deltaYaw);
		root->SetLocalRotation(rotation);
		root->RefreshWorldTransform();
		return true;
	}

	void CharacterMovementComponent::ApplyMovement(float deltaTime)
	{
		Actor* owner = GetOwner();
		SceneComponent* root = owner ? owner->GetRootComponent() : nullptr;
		if (!root) return;

		const Vector3& position = root->GetLocalPosition();
		const Vector3 displacement = CalculateDisplacement(deltaTime);
		root->SetLocalPosition({
			position.x + displacement.x,
			position.y + displacement.y,
			position.z + displacement.z
			});
		root->RefreshWorldTransform();
	}

	std::vector<ComponentProperty> CharacterMovementComponent::CreateProperties()
	{
		return {
			{ "Velocity", "目標速度", ComponentPropertyType::Vector3, [this]() -> ComponentPropertyValue { return velocity_; }, [this](const ComponentPropertyValue& value) { if (const Vector3* typedValue = std::get_if<Vector3>(&value)) SetVelocity(*typedValue); }, 0.0f, 0.0f, 0.05f, false, {}, ComponentPropertyDisplay::Default },
			{ "MaxDriveForce", "最大駆動力", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return maxDriveForce_; }, [this](const ComponentPropertyValue& value) { if (const float* typedValue = std::get_if<float>(&value)) SetMaxDriveForce(*typedValue); }, 0.0f, 5000.0f, 1.0f, true },
			{ "MaxBrakingForce", "最大制動力", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return maxBrakingForce_; }, [this](const ComponentPropertyValue& value) { if (const float* typedValue = std::get_if<float>(&value)) SetMaxBrakingForce(*typedValue); }, 0.0f, 5000.0f, 1.0f, true },
			{ "MovementEnabled", "移動有効", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return movementEnabled_; }, [this](const ComponentPropertyValue& value) { if (const bool* typedValue = std::get_if<bool>(&value)) SetMovementEnabled(*typedValue); }, 0.0f, 0.0f, 0.1f, false, {}, ComponentPropertyDisplay::Default }
		};
	}
} // namespace Ken4lowEngine
