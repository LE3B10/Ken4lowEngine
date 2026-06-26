#include "ActorWorld.h"
#include "ColliderComponent.h"
#include "RigidbodyComponent.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	void ActorWorld::Initialize()
	{
		for (auto& actor : actors_)
		{
			// 初期化処理は各Actorに委譲する
			actor->Initialize();

			// Actor初期化後に生成されたCollider / RigidbodyをPhysicsWorldへ登録する
			RegisterPhysicsComponents(*actor);
		}

		isInitialized_ = true; // ActorWorldの初期化が完了したことを示す
	}

	void ActorWorld::Update(float deltaTime)
	{
		for (auto& actor : actors_)
		{
			// ActorWorldは更新順だけ管理し、処理内容はActor/Component側に任せる
			actor->Update(deltaTime);

			if (actor->IsPendingDestroy())
			{
				// Actor削除前にPhysicsWorldが持つ外部参照を解除する。
				UnregisterPhysicsComponents(*actor);
			}
		}

		std::erase_if(actors_, [](const std::unique_ptr<Actor>& actor)
			{
				return actor->IsPendingDestroy(); // Destroy予約されたActorを更新後にまとめて削除する
			});
	}

	void ActorWorld::PostPhysicsUpdate(float deltaTime)
	{
		for (auto& actor : actors_)
		{
			// PhysicsWorld更新後の処理をActorへ流す。
			actor->PostPhysicsUpdate(deltaTime);
		}
	}

	void ActorWorld::Draw()
	{
		for (auto& actor : actors_)
		{
			// 通常描画を持つActorだけが内部Component経由で描画される
			actor->Draw();
		}
	}

	void ActorWorld::DrawShadow()
	{
		for (auto& actor : actors_)
		{
			// 影を落とすActorだけが内部Component経由でShadow描画される
			actor->DrawShadow();
		}
	}

	void ActorWorld::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("Actor World");

		for (auto& actor : actors_)
		{
			// Actor名をTreeNodeにして、ActorごとのComponent情報を見やすくする
			if (ImGui::TreeNode(actor->GetName().c_str()))
			{
				actor->DrawImGui();
				ImGui::TreePop();
			}
		}
#endif // USE_IMGUI
	}

	void ActorWorld::Finalize()
	{
		for (auto& actor : actors_)
		{
			// Actor破棄前にPhysicsWorldが持つ外部参照を解除する
			UnregisterPhysicsComponents(*actor);

			// Actor破棄前にComponent側のFinalizeまで流す
			actor->Finalize();
		}

		actors_.clear(); // Finalize後にActorを破棄し、古い状態が残らないようにする

		isInitialized_ = false; // 再Initialize時にSpawn済みActorを通常初期化できるように戻す
	}

	Actor* ActorWorld::FindActorByName(std::string_view name)
	{
		for (auto& actor : actors_)
		{
			// Actor名が一致した最初のActorを返す
			if (actor->GetName() == name)
			{
				return actor.get();
			}
		}

		return nullptr; // 名前が一致するActorが見つからなかった場合はnullptrを返す
	}

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

		if (!physicsWorld_ || actor.IsPhysicsRegistered())
		{
			return; // 既に登録済みなら二重登録しない。
		}

		auto* collider = actor.GetComponent<ColliderComponent>();
		auto* rigidbody = actor.GetComponent<RigidbodyComponent>();

		if (collider && rigidbody)
		{
			// ColliderからRigidbodyを参照できるようにして、Solverが押し戻し・速度補正できるようにする
			collider->GetCollider()->SetRigidbody(rigidbody->GetRigidbody());
		}

		if (collider)
		{
			// ActorのColliderをPhysicsWorldへ登録する
			physicsWorld_->RegisterCollider(collider->GetCollider());
		}

		if (rigidbody)
		{
			// ActorのRigidbodyをPhysicsWorldへ登録する
			physicsWorld_->RegisterRigidbody(rigidbody->GetRigidbody());
		}

		actor.SetPhysicsRegistered(true); // 登録済みフラグを立てる
	}

	void ActorWorld::UnregisterPhysicsComponents(Actor& actor)
	{
		if (!physicsWorld_ || !actor.IsPhysicsRegistered())
		{
			return; // 未登録なら二重解除しない。
		}

		if (auto* collider = actor.GetComponent<ColliderComponent>())
		{
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