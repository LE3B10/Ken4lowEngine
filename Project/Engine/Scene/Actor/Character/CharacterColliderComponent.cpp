#include "CharacterColliderComponent.h"

namespace Ken4lowEngine
{
	CharacterColliderComponent::CharacterColliderComponent()
	{
		SetShapeType(ECollisionShapeType::OBB);
		SetHalfSize({ 0.5f, 1.0f, 0.5f });
	}
} // namespace Ken4lowEngine
