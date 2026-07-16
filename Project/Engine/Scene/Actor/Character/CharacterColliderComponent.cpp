#include "CharacterColliderComponent.h"

#include "CharacterActor.h"

#include <cmath>

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
		SyncScaledShape();
	}

	void CharacterColliderComponent::Update(float deltaTime)
	{
		ColliderComponent::Update(deltaTime);
		SyncScaledShape();
	}

	void CharacterColliderComponent::UpdateEditor(float deltaTime)
	{
		ColliderComponent::UpdateEditor(deltaTime);
		SyncScaledShape();
	}

	void CharacterColliderComponent::PostPhysicsUpdate(float deltaTime)
	{
		ColliderComponent::PostPhysicsUpdate(deltaTime);
		SyncScaledShape();
	}

	void CharacterColliderComponent::SyncScaledShape()
	{
		Collider* collider = GetCollider();
		if (!collider) return;

		const Vector3 worldScale = GetWorldScale();
		const Vector3 authoredHalfSize = GetHalfSize();
		const Vector3 scaledHalfSize{
			authoredHalfSize.x * std::fabs(worldScale.x),
			authoredHalfSize.y * std::fabs(worldScale.y),
			authoredHalfSize.z * std::fabs(worldScale.z)
		};

		if (GetShapeType() == ECollisionShapeType::AABB)
		{
			const Vector3 center = GetWorldPosition();
			AABB aabb{};
			aabb.min = center - scaledHalfSize;
			aabb.max = center + scaledHalfSize;
			collider->SetAABB(aabb); // Characterの親Scale変更を実際のAABBサイズへ反映する。
		}
		else if (GetShapeType() == ECollisionShapeType::OBB)
		{
			collider->SetCenterPosition(GetWorldPosition());
			collider->SetOBBHalfSize(scaledHalfSize);
			collider->SetOrientation(GetWorldRotation()); // OBBも見た目と同じWorld Scale・回転へ同期する。
		}
	}
} // namespace Ken4lowEngine
