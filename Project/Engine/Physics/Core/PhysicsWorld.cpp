#include "PhysicsWorld.h"

#include "Collider.h"
#include "CollisionUtility.h"
#include "Engine/Physics/Solver/PositionSolver.h"
#include "Rigidbody.h"

#include <algorithm>
#include <cmath>

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
		// Contactは毎フレーム作り直し、前フレームの接触情報を残さない。
		contacts_.clear();

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
		if (!positionSolveEnabled_)
		{
			return;
		}

		PositionSolver positionSolver{};

		// Trigger以外のContactだけを位置補正し、速度反射やイベント通知はまだ行わない。
		for (Contact& contact : contacts_)
		{
			positionSolver.Resolve(contact);
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
		const AABB aabbA = colliderA->GetAABB();
		const AABB aabbB = colliderB->GetAABB();

		const float overlapX = std::min(aabbA.max.x, aabbB.max.x) - std::max(aabbA.min.x, aabbB.min.x);
		const float overlapY = std::min(aabbA.max.y, aabbB.max.y) - std::max(aabbA.min.y, aabbB.min.y);
		const float overlapZ = std::min(aabbA.max.z, aabbB.max.z) - std::max(aabbA.min.z, aabbB.min.z);

		const Vector3 overlapMin{
			std::max(aabbA.min.x, aabbB.min.x),
			std::max(aabbA.min.y, aabbB.min.y),
			std::max(aabbA.min.z, aabbB.min.z),
		};
		const Vector3 overlapMax{
			std::min(aabbA.max.x, aabbB.max.x),
			std::min(aabbA.max.y, aabbB.max.y),
			std::min(aabbA.max.z, aabbB.max.z),
		};

		// まずはAABBの重なり量から最小侵入軸を選び、Contact確認に必要なnormal/penetrationを作る。
		contact.point = (overlapMin + overlapMax) * 0.5f;
		contact.penetration = std::max(0.0f, overlapX);
		contact.normal = { delta.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f };

		if (overlapY < contact.penetration)
		{
			contact.penetration = std::max(0.0f, overlapY);
			contact.normal = { 0.0f, delta.y >= 0.0f ? 1.0f : -1.0f, 0.0f };
		}
		if (overlapZ < contact.penetration)
		{
			contact.penetration = std::max(0.0f, overlapZ);
			contact.normal = { 0.0f, 0.0f, delta.z >= 0.0f ? 1.0f : -1.0f };
		}
		if (std::abs(delta.x) <= 0.0001f && std::abs(delta.y) <= 0.0001f && std::abs(delta.z) <= 0.0001f)
		{
			contact.normal = { 0.0f, 1.0f, 0.0f };
		}

		contact.isTrigger = colliderA->IsTrigger() || colliderB->IsTrigger();
		return contact;
	}

} // namespace Ken4lowEngine
