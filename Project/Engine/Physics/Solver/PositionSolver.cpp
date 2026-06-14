#include "PositionSolver.h"

#include "Collider.h"
#include "PhysicsTypes.h"
#include "Rigidbody.h"

namespace Ken4lowEngine
{
	namespace
	{
		bool IsMovableBody(const Rigidbody* rigidbody)
		{
			// 入力で動くキャラクターをStatic環境から押し戻すため、DynamicとKinematicを位置補正対象にする。
			return rigidbody &&
				(rigidbody->GetBodyType() == BodyType::Dynamic || rigidbody->GetBodyType() == BodyType::Kinematic);
		}

		void MoveCollider(Collider* collider, const Vector3& correction)
		{
			// Colliderの中心だけを補正し、速度反射や摩擦などの応答は次Phaseへ残す。
			if (!collider)
			{
				return;
			}
			collider->SetCenterPosition(collider->GetCenterPosition() + correction);
		}
	}

	void PositionSolver::Resolve(Contact& contact) const
	{
		// Contact情報を使ってめり込みを補正する。penetrationが無い接触やTriggerは位置補正しない。
		if (contact.penetration <= 0.0f || contact.isTrigger || !contact.colliderA || !contact.colliderB)
		{
			return;
		}

		Rigidbody* rigidbodyA = contact.colliderA->GetRigidbody();
		Rigidbody* rigidbodyB = contact.colliderB->GetRigidbody();
		const bool movableA = IsMovableBody(rigidbodyA);
		const bool movableB = IsMovableBody(rigidbodyB);

		if (!movableA && !movableB)
		{
			return;
		}

		const Vector3 correction = contact.normal * contact.penetration;

		if (movableA && movableB)
		{
			// Dynamic同士は半分ずつ分けて離し、どちらか一方だけが大きく飛ばないようにする。
			MoveCollider(contact.colliderA, correction * -0.5f);
			MoveCollider(contact.colliderB, correction * 0.5f);
			return;
		}

		if (movableA)
		{
			// normalはAからB方向なので、Aだけ動ける場合は反対方向へ押し戻す。
			MoveCollider(contact.colliderA, correction * -1.0f);
			return;
		}

		// Bだけ動ける場合はnormal方向へ押し戻す。
		MoveCollider(contact.colliderB, correction);
	}

} // namespace Ken4lowEngine
