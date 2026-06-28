#include "ActorWorld.h"
#include "ColliderComponent.h"
#include "RigidbodyComponent.h"
#include "ActorJsonSerializer.h"

#ifdef USE_IMGUI
#include <imgui.h>
#include <cstdio>
#include <typeinfo>
#endif

namespace Ken4lowEngine
{
#ifdef USE_IMGUI
	namespace
	{
		constexpr size_t kNameEditBufferSize = 128;

		bool DrawNameInput(const char* label, const std::string& currentName, std::string& editName)
		{
			std::array<char, kNameEditBufferSize> buffer{};
			std::snprintf(buffer.data(), buffer.size(), "%s", currentName.c_str());

			if (ImGui::InputText(label, buffer.data(), buffer.size()))
			{
				editName = std::string(buffer.data());
				return true; // 名前が変更された場合にtrueを返す
			}

			return false; // 名前が変更されなかった場合にfalseを返す
		}
	}
#endif // USE_IMGUI

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
		if (ImGui::Begin("Actor World"))
		{
			ImGui::Text("Actor Count: %zu", actors_.size());
			ImGui::Separator();

			for (size_t actorIndex = 0; actorIndex < actors_.size(); ++actorIndex)
			{
				Actor* actor = actors_[actorIndex].get();
				if (!actor)
				{
					continue; // Actorがnullptrなら表示しない
				}

				Actor* beforeSelectedActor = selectedActor_;
				ActorComponent* beforeSelectedComponent = selectedComponent_;

				ImGui::PushID(static_cast<int>(actorIndex));
				actor->DrawHierarchyImGui(selectedActor_, selectedComponent_); // Actor World上にActor/Component階層を表示する
				ImGui::PopID();

				if (beforeSelectedActor != selectedActor_ || beforeSelectedComponent != selectedComponent_)
				{
					requestFocusActorDetails_ = true; // 選択中ActorまたはComponentが変化した場合にDetailsウィンドウを更新する
				}
			}
		}
		ImGui::End();

		DrawDetailsImGui(); // 選択中ActorまたはComponentのDetailsウィンドウを描画する
#endif // USE_IMGUI
	}

	void ActorWorld::DrawDetailsImGui()
	{
#ifdef USE_IMGUI
		if (requestFocusActorDetails_)
		{
			ImGui::SetNextWindowFocus(); // 選択が変わったらActor Detailsを前面へ出す
			requestFocusActorDetails_ = false;
		}

		if (ImGui::Begin("Actor Details"))
		{
			Actor* saveTargetActor = selectedActor_;

			if (!saveTargetActor && selectedComponent_)
			{
				saveTargetActor = selectedComponent_->GetOwner(); // Componentが選択中なら所有Actorを取得する
			}

			if (saveTargetActor)
			{
				if (ImGui::Button("Save Selected Actor JSON"))
				{
					const std::string actorName = saveTargetActor->GetName().empty()
						? saveTargetActor->GetClassTypeName() // 名前が空なら型名を使う
						: saveTargetActor->GetName();

					const std::string filePath = "Resources/ActorPrefabs/" + actorName + ".json"; // 保存先のファイルパスを生成する

					const bool succeded = ActorJsonSerializer::SaveActorToFile(*saveTargetActor, filePath);

					if (succeded)
					{
						lastActorJsonSaveMessage_ = "Saved : " + filePath; // 保存成功メッセージを保持する
					}
					else
					{
						lastActorJsonSaveMessage_ = "Failed to save : " + filePath; // 保存失敗メッセージを保持する
					}
				}

				if (!lastActorJsonSaveMessage_.empty())
				{
					ImGui::Text("%s", lastActorJsonSaveMessage_.c_str());
				}

				ImGui::Separator();
			}

			if (selectedComponent_)
			{
				const std::string componentName = selectedComponent_->GetName().empty()
					? "Unnamed Component"
					: selectedComponent_->GetName();

				ImGui::Text("Selected Component: %s", componentName.c_str());
				ImGui::Separator();

				selectedComponent_->DrawInspectorImGui(); // 選択中Componentの詳細を描画する。
			}
			else if (selectedActor_)
			{
				const std::string actorName = selectedActor_->GetName().empty()
					? "Unnamed Actor"
					: selectedActor_->GetName();

				ImGui::Text("Selected Actor: %s", actorName.c_str());
				ImGui::Separator();

				selectedActor_->DrawInspectorImGui(); // 選択中Actorの詳細を描画する。
			}
			else
			{
				ImGui::Text("No selection.");
			}
		}

		ImGui::End();
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
			return; // PhysicsWorldが無い、または登録済みの場合は何もしない
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