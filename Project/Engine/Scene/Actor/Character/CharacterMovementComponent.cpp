#include "CharacterMovementComponent.h"

#include "Actor.h"
#include "SceneComponent.h"

#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	void CharacterMovementComponent::Update(float deltaTime)
	{
		if (!movementEnabled_ || !std::isfinite(deltaTime) || deltaTime <= 0.0f) return;
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
		return velocity_ * deltaTime; // Actor経路と旧Boss Adapterで同じ積分規則を共有する。
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
		root->RefreshWorldTransform(); // Movement適用後の子Component位置を同じフレームで確定する。
	}

	std::vector<ComponentProperty> CharacterMovementComponent::CreateProperties()
	{
		return {
			{ "Velocity", "速度", ComponentPropertyType::Vector3, [this]() -> ComponentPropertyValue { return velocity_; }, [this](const ComponentPropertyValue& value) { if (const Vector3* typedValue = std::get_if<Vector3>(&value)) SetVelocity(*typedValue); }, 0.0f, 0.0f, 0.05f, false, {}, ComponentPropertyDisplay::Default },
			{ "MovementEnabled", "移動有効", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return movementEnabled_; }, [this](const ComponentPropertyValue& value) { if (const bool* typedValue = std::get_if<bool>(&value)) SetMovementEnabled(*typedValue); }, 0.0f, 0.0f, 0.1f, false, {}, ComponentPropertyDisplay::Default }
		};
	}
} // namespace Ken4lowEngine
