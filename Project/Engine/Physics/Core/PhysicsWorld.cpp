#include "PhysicsWorld.h"

#include "Collider.h"
#include "CollisionUtility.h"
#include "Rigidbody.h"

#include <algorithm>

namespace Ken4lowEngine
{
	namespace
	{
		template<class T>
		bool ContainsPointer(const std::vector<T*>& values, T* value)
		{
			return std::find(values.begin(), values.end(), value) != values.end();
		}

		template<class T>
		void ErasePointer(std::vector<T*>& values, T* value)
		{
			values.erase(std::remove(values.begin(), values.end(), value), values.end());
		}
	}

	void PhysicsWorld::RegisterCollider(Collider* collider)
	{
		// nullptrと重複登録を避け、外部所有のCollider参照だけを保持する。
		if (!collider || ContainsPointer(colliders_, collider))
		{
			return;
		}

		colliders_.push_back(collider);
	}

	void PhysicsWorld::UnregisterCollider(Collider* collider)
	{
		// Collider登録解除時は、古いContactが次ステップへ残らないようにContactも掃除する。
		ErasePointer(colliders_, collider);
		contacts_.erase(
			std::remove_if(contacts_.begin(), contacts_.end(),
				[collider](const Contact& contact)
				{
					return contact.colliderA == collider || contact.colliderB == collider;
				}),
			contacts_.end());
	}

	void PhysicsWorld::RegisterRigidbody(Rigidbody* rigidbody)
	{
		// nullptrと重複登録を避け、外部所有のRigidbody参照だけを保持する。
		if (!rigidbody || ContainsPointer(rigidbodies_, rigidbody))
		{
			return;
		}

		rigidbodies_.push_back(rigidbody);
	}

	void PhysicsWorld::UnregisterRigidbody(Rigidbody* rigidbody)
	{
		// Rigidbodyは所有しないため、登録リストからのみ取り除く。
		ErasePointer(rigidbodies_, rigidbody);
	}

	void PhysicsWorld::Step(float deltaTime)
	{
		// 将来の本格接続に備え、積分、検出、解決の順序だけを固定する。
		IntegrateBodies(deltaTime);
		DetectCollisions();
		ResolveContacts();
	}

	void PhysicsWorld::IntegrateBodies(float deltaTime)
	{
		// 登録済みRigidbodyへ速度積分を委譲する。
		for (Rigidbody* rigidbody : rigidbodies_)
		{
			if (rigidbody)
			{
				rigidbody->Integrate(deltaTime);
			}
		}
	}

	void PhysicsWorld::DetectCollisions()
	{
		contacts_.clear();

		// 現段階では既存互換の総当たりでContactだけを作り、BroadPhase分離の差し替え口を残す。
		for (size_t i = 0; i < colliders_.size(); ++i)
		{
			for (size_t j = i + 1; j < colliders_.size(); ++j)
			{
				Collider* colliderA = colliders_[i];
				Collider* colliderB = colliders_[j];
				if (!colliderA || !colliderB)
				{
					continue;
				}
				if (!colliderA->IsCollisionEnabledForQuery() || !colliderB->IsCollisionEnabledForQuery())
				{
					continue;
				}
				if (!TestCollisionPair(colliderA, colliderB))
				{
					continue;
				}

				contacts_.push_back(BuildContact(colliderA, colliderB));
			}
		}
	}

	void PhysicsWorld::ResolveContacts()
	{
		// 今回は既存ゲーム挙動を変えないため、押し戻しやイベント通知はまだ行わない。
		for ([[maybe_unused]] const Contact& contact : contacts_)
		{
		}
	}

	bool PhysicsWorld::TestCollisionPair(Collider* colliderA, Collider* colliderB) const
	{
		// 既存CollisionUtilityを薄く利用し、NarrowPhase分離時の差し替え位置を明確にする。
		switch (colliderA->GetShapeType())
		{
		case ECollisionShapeType::Sphere:
			switch (colliderB->GetShapeType())
			{
			case ECollisionShapeType::Sphere:
				return CollisionUtility::IsCollision(colliderA->GetSphere(), colliderB->GetSphere());
			case ECollisionShapeType::AABB:
				return CollisionUtility::IsCollision(colliderB->GetAABB(), colliderA->GetSphere());
			case ECollisionShapeType::OBB:
				return CollisionUtility::IsCollision(colliderB->GetOBB(), colliderA->GetSphere());
			case ECollisionShapeType::Capsule:
				return CollisionUtility::IsCollision(colliderB->GetCapsule(), colliderA->GetSphere());
			default:
				return false;
			}

		case ECollisionShapeType::AABB:
			switch (colliderB->GetShapeType())
			{
			case ECollisionShapeType::Sphere:
				return CollisionUtility::IsCollision(colliderA->GetAABB(), colliderB->GetSphere());
			case ECollisionShapeType::AABB:
				return CollisionUtility::IsCollision(colliderA->GetAABB(), colliderB->GetAABB());
			case ECollisionShapeType::OBB:
				return CollisionUtility::IsCollision(colliderB->GetOBB(), colliderA->GetAABB());
			case ECollisionShapeType::Capsule:
				return CollisionUtility::IsCollision(colliderA->GetAABB(), colliderB->GetCapsule());
			case ECollisionShapeType::Segment:
				return CollisionUtility::IsCollision(colliderA->GetAABB(), colliderB->GetSegment());
			default:
				return false;
			}

		case ECollisionShapeType::OBB:
			switch (colliderB->GetShapeType())
			{
			case ECollisionShapeType::Sphere:
				return CollisionUtility::IsCollision(colliderA->GetOBB(), colliderB->GetSphere());
			case ECollisionShapeType::AABB:
				return CollisionUtility::IsCollision(colliderA->GetOBB(), colliderB->GetAABB());
			case ECollisionShapeType::OBB:
				return CollisionUtility::IsCollision(colliderA->GetOBB(), colliderB->GetOBB());
			case ECollisionShapeType::Segment:
				return CollisionUtility::IsCollision(colliderA->GetOBB(), colliderB->GetSegment());
			default:
				return false;
			}

		case ECollisionShapeType::Capsule:
			switch (colliderB->GetShapeType())
			{
			case ECollisionShapeType::Sphere:
				return CollisionUtility::IsCollision(colliderA->GetCapsule(), colliderB->GetSphere());
			case ECollisionShapeType::AABB:
				return CollisionUtility::IsCollision(colliderB->GetAABB(), colliderA->GetCapsule());
			case ECollisionShapeType::Capsule:
				return CollisionUtility::IsCollision(colliderA->GetCapsule(), colliderB->GetCapsule());
			case ECollisionShapeType::Segment:
				return CollisionUtility::IsCollision(colliderA->GetCapsule(), colliderB->GetSegment());
			default:
				return false;
			}

		case ECollisionShapeType::Segment:
			switch (colliderB->GetShapeType())
			{
			case ECollisionShapeType::AABB:
				return CollisionUtility::IsCollision(colliderB->GetAABB(), colliderA->GetSegment());
			case ECollisionShapeType::OBB:
				return CollisionUtility::IsCollision(colliderB->GetOBB(), colliderA->GetSegment());
			case ECollisionShapeType::Capsule:
				return CollisionUtility::IsCollision(colliderB->GetCapsule(), colliderA->GetSegment());
			default:
				return false;
			}

		case ECollisionShapeType::None:
		default:
			return false;
		}
	}

	Contact PhysicsWorld::BuildContact(Collider* colliderA, Collider* colliderB) const
	{
		Contact contact{};
		contact.colliderA = colliderA;
		contact.colliderB = colliderB;

		const Vector3 centerA = colliderA->GetCenterPosition();
		const Vector3 centerB = colliderB->GetCenterPosition();
		const Vector3 delta = centerB - centerA;

		// 現段階では簡易Contactとして中心間の方向と中点を保持し、詳細なpenetration計算はNarrowPhaseへ残す。
		contact.point = (centerA + centerB) * 0.5f;
		contact.normal = Vector3::NormalizeSafe(delta, { 0.0f, 1.0f, 0.0f });
		contact.penetration = 0.0f;
		contact.isTrigger = colliderA->IsTrigger() || colliderB->IsTrigger();
		return contact;
	}

} // namespace Ken4lowEngine
