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
			// Rigidbodyを持つCharacterはXZ移動だけをAI/Movementから渡し、Y速度は重力と衝突解決へ任せる。
			Vector3 physicalVelocity = rigidbody->GetVelocity();
			physicalVelocity.x = movementEnabled_ ? velocity_.x : 0.0f;
			physicalVelocity.z = movementEnabled_ ? velocity_.z : 0.0f;
			rigidbodyComponent->SetVelocity(physicalVelocity);
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
			{ "Velocity", "速度", ComponentPropertyType::Vector3, [this]() -> ComponentPropertyValue { return velocity_; }, [this](const ComponentPropertyValue& value) { if (const Vector3* typedValue = std::get_if<Vector3>(&value)) SetVelocity(*typedValue); }, 0.0f, 0.0f, 0.05f, false, {}, ComponentPropertyDisplay::Default },
			{ "MovementEnabled", "移動有効", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return movementEnabled_; }, [this](const ComponentPropertyValue& value) { if (const bool* typedValue = std::get_if<bool>(&value)) SetMovementEnabled(*typedValue); }, 0.0f, 0.0f, 0.1f, false, {}, ComponentPropertyDisplay::Default }
		};
	}
} // namespace Ken4lowEngine
