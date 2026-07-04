#include "ActorWorld.h"

#include "ComponentFactory.h"
#include "SceneComponent.h"

#include <string>

namespace Ken4lowEngine
{
	void ActorWorld::AddComponentToSelectedActor(std::string_view componentClassName)
	{
		Actor* targetActor = selectedActor_;

		if (!targetActor && selectedComponent_)
		{
			targetActor = selectedComponent_->GetOwner(); // Component選択中なら所有Actorへ追加する
		}

		if (!targetActor)
		{
			lastActorJsonSaveMessage_ = "Add Component failed : no selected Actor";
			return;
		}

		const std::string className{ componentClassName };

		if (!ComponentFactory::IsAllowMultiple(className) && targetActor->HasComponentClass(className))
		{
			lastActorJsonSaveMessage_ = "Add Component failed : already exists" + className;
			return; // alloMutiple = false のComponentはUI以外の経路からも重複追加させない
		}

		// 既にPhysicsWorldへ登録済みなら、一度解除してからComponentを追加する
		const bool wasPhysicsRegistered = targetActor->IsPhysicsRegistered();
		if (wasPhysicsRegistered)
		{
			UnregisterPhysicsComponents(*targetActor);
		}

		ActorComponent* newComponent = nullptr;

		// RootComponentが無いActorにSceneComponent系を追加する場合はRootとして生成する
		if (!targetActor->GetRootComponent())
		{
			newComponent = ComponentFactory::CreateRootSceneComponent(targetActor, componentClassName);
		}

		if (!newComponent)
		{
			newComponent = ComponentFactory::CreateComponent(targetActor, componentClassName);
		}

		if (!newComponent)
		{
			if (wasPhysicsRegistered)
			{
				RegisterPhysicsComponents(*targetActor); // 追加失敗時は元のPhysics登録状態へ戻す
			}

			lastActorJsonSaveMessage_ = "Add Component failed : " + std::string(componentClassName);
			return;
		}

		// Component名が重複しないようにする
		newComponent->SetName(MakeUniqueComponentName(*targetActor, std::string(componentClassName)));

		// SceneComponent系なら、Rootがある場合はRoot配下へAttachする
		if (SceneComponent* newSceneComponent = dynamic_cast<SceneComponent*>(newComponent))
		{
			SceneComponent* rootComponent = targetActor->GetRootComponent();

			if (rootComponent && rootComponent != newSceneComponent)
			{
				newSceneComponent->AttachTo(rootComponent); // 追加したSceneComponentはRoot配下に置く
			}

			newSceneComponent->RefreshWorldTransform();
		}

		if (isInitialized_)
		{
			newComponent->Initialize(); // 追加したComponentだけ初期化する
		}

		if (wasPhysicsRegistered)
		{
			RegisterPhysicsComponents(*targetActor); // Collider/Rigidbody追加に対応するためPhysics登録を作り直す
		}

		selectedActor_ = nullptr;
		selectedComponent_ = newComponent;
		requestFocusActorDetails_ = true;

		lastActorJsonSaveMessage_ = "Added Component : " + newComponent->GetName();
	}

	void ActorWorld::ProcessPendingComponentDelete()
	{
		if (!hasPendingDeleteComponent_ || !pendingDeleteComponent_)
		{
			return; // Component削除予約が無い場合は何もしない。
		}

		ActorComponent* deleteComponent = pendingDeleteComponent_;

		hasPendingDeleteComponent_ = false; // 再処理防止。
		pendingDeleteComponent_ = nullptr;

		Actor* owner = deleteComponent->GetOwner();
		if (!owner)
		{
			lastActorJsonSaveMessage_ = "Delete Component failed : no owner";
			return;
		}

		// ActorWorldが所有しているActorか確認する。
		bool ownerExists = false;
		for (const auto& actor : actors_)
		{
			if (actor.get() == owner)
			{
				ownerExists = true;
				break;
			}
		}

		if (!ownerExists)
		{
			lastActorJsonSaveMessage_ = "Delete Component failed : owner not found";
			return;
		}

		if (deleteComponent == owner->GetRootComponent())
		{
			lastActorJsonSaveMessage_ = "Delete Component failed : RootComponent cannot be deleted";
			return;
		}

		const std::string deletedComponentName = deleteComponent->GetName();

		const bool wasPhysicsRegistered = owner->IsPhysicsRegistered();
		if (wasPhysicsRegistered)
		{
			UnregisterPhysicsComponents(*owner); // Collider/Rigidbodyを消す可能性があるので一度解除する。
		}

		if (selectedComponent_ == deleteComponent)
		{
			selectedComponent_ = nullptr; // 削除対象を選択していた場合は選択解除する。
			selectedActor_ = owner;       // 削除後は所有Actorを選択状態に戻す。
		}

		const bool removed = owner->RemoveComponent(deleteComponent);

		if (wasPhysicsRegistered)
		{
			RegisterPhysicsComponents(*owner); // 残ったCollider/Rigidbodyを再登録する。
		}

		if (!removed)
		{
			lastActorJsonSaveMessage_ = "Delete Component failed : " + deletedComponentName;
			return;
		}

		lastActorJsonSaveMessage_ = "Deleted Component : " + deletedComponentName;
	}

	void ActorWorld::DeleteSelectedComponent()
	{
		if (!selectedComponent_)
		{
			lastActorJsonSaveMessage_ = "Delete Component failed : no selected Component";
			return;
		}

		Actor* owner = selectedComponent_->GetOwner();
		if (!owner)
		{
			lastActorJsonSaveMessage_ = "Delete Component failed : no owner";
			return;
		}

		if (selectedComponent_ == owner->GetRootComponent())
		{
			lastActorJsonSaveMessage_ = "Delete Component failed : RootComponent cannot be deleted";
			return;
		}

		pendingDeleteComponent_ = selectedComponent_;
		hasPendingDeleteComponent_ = true;

		lastActorJsonSaveMessage_ = "Delete Component requested : " + selectedComponent_->GetName();
	}

	void ActorWorld::DuplicateSelectedComponent()
	{
		if (!selectedComponent_)
		{
			lastActorJsonSaveMessage_ = "Duplicate Component failed : no selected Component";
			return;
		}

		Actor* owner = selectedComponent_->GetOwner();
		if (!owner)
		{
			lastActorJsonSaveMessage_ = "Duplicate Component failed : no owner Actor";
			return;
		}

		if (selectedComponent_ == owner->GetRootComponent())
		{
			lastActorJsonSaveMessage_ = "Duplicate Component failed : RootComponent cannot be duplicated";
			return;
		}

		const bool wasPhysicsRegistered = owner->IsPhysicsRegistered();
		if (wasPhysicsRegistered)
		{
			UnregisterPhysicsComponents(*owner); // Physics登録済みのときだけ一度解除する
		}

		nlohmann::json sourceJson;
		selectedComponent_->ToJson(sourceJson); // Serialize / Deserializeを使ってComponent設定を安全に複製する

		if (!sourceJson.contains("Class") || !sourceJson["Class"].is_string())
		{
			if (wasPhysicsRegistered)
			{
				RegisterPhysicsComponents(*owner);
			}

			lastActorJsonSaveMessage_ = "Duplicate Component failed : invalid Component class";
			return;
		}

		const std::string className = sourceJson["Class"].get<std::string>();

		if (!ComponentFactory::IsAllowMultiple(className) && owner->HasComponentClass(className))
		{
			if (wasPhysicsRegistered)
			{
				RegisterPhysicsComponents(*owner);
			}

			lastActorJsonSaveMessage_ = "Duplicate Component failed : already exists " + className;
			return; // Camera / Rigidbodyなど１つだけのComponentは複製させない
		}

		ActorComponent* duplicatedComponent = ComponentFactory::CreateComponent(owner, className);
		if (!duplicatedComponent)
		{
			if (wasPhysicsRegistered)
			{
				RegisterPhysicsComponents(*owner); // 追加失敗時は元のPhysics登録状態へ戻す
			}

			lastActorJsonSaveMessage_ = "Duplicate Component failed : " + className;
			return;
		}

		duplicatedComponent->FromJson(sourceJson);

		const std::string baseName = selectedComponent_->GetName().empty()
			? className
			: selectedComponent_->GetName();

		duplicatedComponent->SetName(MakeUniqueComponentName(*owner, baseName + "_Copy"));

		if (SceneComponent* duplicatedScene = dynamic_cast<SceneComponent*>(duplicatedComponent))
		{
			SceneComponent* sourceScene = dynamic_cast<SceneComponent*>(selectedComponent_);

			if (sourceScene && sourceScene->GetParent())
			{
				duplicatedScene->AttachTo(sourceScene->GetParent());
			}
			else if (owner->GetRootComponent() && owner->GetRootComponent() != duplicatedScene)
			{
				duplicatedScene->AttachTo(owner->GetRootComponent());
			}

			duplicatedScene->RefreshWorldTransform();
		}

		if (isInitialized_)
		{
			duplicatedComponent->Initialize();
		}

		if (wasPhysicsRegistered)
		{
			RegisterPhysicsComponents(*owner); // 複製後のCollider/Rigidbodyを再登録する
		}

		selectedActor_ = nullptr;
		selectedComponent_ = duplicatedComponent;
		requestFocusActorDetails_ = true;

		lastActorJsonSaveMessage_ = "Duplicated Component : " + duplicatedComponent->GetName();
	}

}