#include "ActorWorld.h"

#include "ColliderComponent.h"
#include "RigidbodyComponent.h"

namespace Ken4lowEngine
{
	void ActorWorld::SetupDefaultPhysicsSettings()
	{
		if (!physicsWorld_)
		{
			return; // PhysicsWorldが設定されていない場合は何もしない
		}

		// ActorComponent物理では、まず同一Layer同士をBLockにして衝突確認をしやすくする
		physicsWorld_->GetResponseMatrix().SetResponse(
			PhysicsCollisionLayer::DynamicActor,
			PhysicsCollisionLayer::DynamicActor,
			CollisionResponseType::Block); // 動くActor同士は物理的に衝突させる。

		physicsWorld_->GetResponseMatrix().SetResponse(
			PhysicsCollisionLayer::DynamicActor,
			PhysicsCollisionLayer::WorldStatic,
			CollisionResponseType::Block); // 動くActorと床は物理的に衝突させる。
	}

	void ActorWorld::RegisterPhysicsComponents(Actor& actor)
	{
		if (!physicsWorld_ || actor.IsPhysicsRegistered() || !actor.IsActive())
		{
			return; // PhysicsWorldが無い、登録済み、または無効なActorの場合は何もしない
		}

		auto colliders = actor.GetComponents<ColliderComponent>();
		auto* rigidbody = actor.GetComponent<RigidbodyComponent>();
		Rigidbody* physicsRigidbody = rigidbody ? rigidbody->GetRigidbody() : nullptr;

		if (physicsRigidbody)
		{
			// ActorのRigidbodyをPhysicsWorldへ登録する
			physicsWorld_->RegisterRigidbody(physicsRigidbody);
		}

		for (ColliderComponent* collider : colliders)
		{
			if (!collider || !collider->GetCollider())
			{
				continue; // Colliderがnullptrなら登録しない
			}

			if (physicsRigidbody)
			{
				collider->GetCollider()->SetRigidbody(physicsRigidbody); // ColliderにRigidbodyを紐付ける
			}

			// ActorのColliderをPhysicsWorldへ登録する
			physicsWorld_->RegisterCollider(collider->GetCollider());
		}

		actor.SetPhysicsRegistered(true); // 登録済みフラグを立てる
	}

	void ActorWorld::UnregisterPhysicsComponents(Actor& actor)
	{
		if (!physicsWorld_ || !actor.IsPhysicsRegistered())
		{
			return; // 未登録なら二重解除しない。
		}

		auto colliders = actor.GetComponents<ColliderComponent>();
		for (ColliderComponent* collider : colliders)
		{
			if (!collider || !collider->GetCollider())
			{
				continue; // Colliderがnullptrなら解除しない
			}

			// ActorのColliderをPhysicsWorldから登録解除する
			physicsWorld_->UnregisterCollider(collider->GetCollider());
		}

		if (auto* rigidbody = actor.GetComponent<RigidbodyComponent>())
		{
			// ActorのRigidbodyをPhysicsWorldから登録解除する
			physicsWorld_->UnregisterRigidbody(rigidbody->GetRigidbody());
		}

		actor.SetPhysicsRegistered(false); // 登録済みフラグを下ろす
	}
}