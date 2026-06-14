#include "PhysicsWorld.h"

#include "Collider.h"
#include "CollisionUtility.h"
#include "Engine/Physics/Solver/FrictionSolver.h"
#include "Engine/Physics/Solver/PositionSolver.h"
#include "Engine/Physics/Solver/VelocitySolver.h"
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

	void PhysicsWorld::Update(float deltaTime)
	{
		// 物理更新を固定時間で進め、フレームレート差による挙動のブレを抑える。
		const float clampedDeltaTime = std::clamp(deltaTime, 0.0f, maxDeltaTime_);
		lastSubStepCount_ = 0;

		if (!useFixedStep_)
		{
			accumulator_ = 0.0f;
			Step(clampedDeltaTime);
			lastSubStepCount_ = 1;
			return;
		}

		accumulator_ += clampedDeltaTime;
		while (accumulator_ >= fixedTimeStep_ && lastSubStepCount_ < maxSubSteps_)
		{
			Step(fixedTimeStep_);
			accumulator_ -= fixedTimeStep_;
			++lastSubStepCount_;
		}

		if (lastSubStepCount_ >= maxSubSteps_ && accumulator_ >= fixedTimeStep_)
		{
			// サブステップ上限へ到達した場合は蓄積時間を捨て、重いフレーム後の暴走を防ぐ。
			accumulator_ = 0.0f;
		}
	}

	void PhysicsWorld::Step(float deltaTime)
	{
		// 1回分の物理更新を実行する。固定更新時はUpdate()からfixedTimeStepで呼び出される。
		// Contactは毎フレーム作り直し、前フレームの接触情報を残さない。
		contacts_.clear();

		// 接地などのフレーム状態はContactから再計算するため、Step開始時に消しておく。
		ClearRigidbodyFrameState();

		// 将来の本格接続に備え、積分、検出、解決の順序だけを固定する。
		IntegrateBodies(deltaTime);
		DetectCollisions();
		ResolveContacts();
		UpdateRigidbodySleepState(deltaTime);
	}

	void PhysicsWorld::SetUseFixedStep(bool useFixedStep)
	{
		// 固定更新の切り替え時は未消化時間を捨て、切り替え直後の余分なStepを避ける。
		useFixedStep_ = useFixedStep;
		accumulator_ = 0.0f;
		lastSubStepCount_ = 0;
	}

	void PhysicsWorld::SetFixedTimeStep(float fixedTimeStep)
	{
		// 固定ステップは極端な値を避け、15〜240Hz相当の範囲に収める。
		fixedTimeStep_ = std::clamp(fixedTimeStep, 1.0f / 240.0f, 1.0f / 15.0f);
		accumulator_ = std::min(accumulator_, fixedTimeStep_);
	}

	void PhysicsWorld::SetMaxDeltaTime(float maxDeltaTime)
	{
		// 入力deltaTimeの上限は、重いフレームで過剰な物理更新が走らない範囲に収める。
		maxDeltaTime_ = std::clamp(maxDeltaTime, 0.016f, 0.5f);
	}

	void PhysicsWorld::SetMaxSubSteps(int maxSubSteps)
	{
		// サブステップ回数は暴走防止のため上限を持たせる。
		maxSubSteps_ = std::clamp(maxSubSteps, 1, 16);
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
		PositionSolver positionSolver{};
		VelocitySolver velocitySolver{};
		FrictionSolver frictionSolver{};

		// Contactごとに位置補正、速度補正、摩擦補正、接地状態更新を行い、応答分離の入口を保つ。
		for (Contact& contact : contacts_)
		{
			if (positionSolveEnabled_)
			{
				positionSolver.Resolve(contact);
				velocitySolver.Resolve(contact);
			}
			if (frictionSolveEnabled_)
			{
				frictionSolver.Resolve(contact);
			}
			UpdateGroundedState(contact);
		}
	}

	void PhysicsWorld::ClearRigidbodyFrameState()
	{
		// Rigidbody側の接触状態は毎フレームContactから作り直す。
		for (Rigidbody* rigidbody : rigidbodies_)
		{
			if (rigidbody)
			{
				rigidbody->ClearFrameState();
			}
		}
	}

	void PhysicsWorld::UpdateRigidbodySleepState(float deltaTime)
	{
		// Step末尾で各Rigidbodyの停止継続時間を評価し、不要な積分を抑えられるようにする。
		for (Rigidbody* rigidbody : rigidbodies_)
		{
			if (rigidbody)
			{
				rigidbody->UpdateSleepState(deltaTime);
			}
		}
	}

	void PhysicsWorld::UpdateGroundedState(const Contact& contact) const
	{
		// Triggerは床判定に使わず、Contact normalが上向き床面を示す場合だけ接地扱いにする。
		if (contact.isTrigger || !contact.colliderA || !contact.colliderB)
		{
			return;
		}

		Rigidbody* rigidbodyA = contact.colliderA->GetRigidbody();
		Rigidbody* rigidbodyB = contact.colliderB->GetRigidbody();
		constexpr float kGroundNormalThreshold = 0.5f;

		if (rigidbodyA && rigidbodyA->GetBodyType() == BodyType::Dynamic && contact.normal.y < -kGroundNormalThreshold)
		{
			rigidbodyA->SetGrounded(true);
		}
		if (rigidbodyB && rigidbodyB->GetBodyType() == BodyType::Dynamic && contact.normal.y > kGroundNormalThreshold)
		{
			rigidbodyB->SetGrounded(true);
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
