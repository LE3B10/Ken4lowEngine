#include "CharacterColliderComponent.h"

#include "CharacterActor.h"

namespace Ken4lowEngine
{
	CharacterColliderComponent::CharacterColliderComponent()
	{
		SetShapeType(ECollisionShapeType::OBB);
		SetHalfSize({ 0.5f, 1.0f, 0.5f });
	}

	void CharacterColliderComponent::Initialize()
	{
		ColliderComponent::Initialize();

		auto* character = dynamic_cast<CharacterActor*>(GetOwner());
		if (!character) return;

		// JSON復元でComponentが再生成されても、毎回現在のCharacterActorへ通知先を張り直す。
		SetOnCollisionCallback([character](Collider* other) { character->OnCollision(other); });
		SetOnCollisionEnterCallback([character](Collider* other) { character->OnCollisionEnter(other); });
		SetOnCollisionStayCallback([character](Collider* other) { character->OnCollisionStay(other); });
		SetOnCollisionExitCallback([character](Collider* other) { character->OnCollisionExit(other); });
		SetOnCollisionEnterHitCallback([character](const CollisionHit& hit) { character->OnCollisionEnter(hit); });
		SetOnCollisionStayHitCallback([character](const CollisionHit& hit) { character->OnCollisionStay(hit); });
		SetOnCollisionExitHitCallback([character](const CollisionHit& hit) { character->OnCollisionExit(hit); });
		SetOnOverlapBeginCallback([character](const CollisionHit& hit) { character->OnOverlapBegin(hit); });
		SetOnOverlapStayCallback([character](const CollisionHit& hit) { character->OnOverlapStay(hit); });
		SetOnOverlapEndCallback([character](const CollisionHit& hit) { character->OnOverlapEnd(hit); });
	}
} // namespace Ken4lowEngine
