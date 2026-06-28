#include "ComponentFactory.h"

#include "Actor.h"
#include "ModelComponent.h"
#include "CameraComponent.h"
#include "ColliderComponent.h"
#include "RigidbodyComponent.h"
#include "InstancedModelComponent.h"

namespace Ken4lowEngine
{

	ActorComponent* ComponentFactory::CreateComponent(Actor* owner, std::string_view className)
	{
		if (!owner)
		{
			return nullptr; // Actorがnullptrの場合はComponentを生成しない
		}

		if (className == "SceneComponent")
		{
			return &owner->AddComponent<SceneComponent>();			// SceneComponentをActorへ追加して返す
		}
		else if (className == "ModelComponent")
		{
			return &owner->AddComponent<ModelComponent>();			// ModelComponentをActorへ追加して返す
		}
		else if (className == "CameraComponent")
		{
			return &owner->AddComponent<CameraComponent>();			// CameraComponentをActorへ追加して返す
		}
		else if (className == "ColliderComponent")
		{
			return &owner->AddComponent<ColliderComponent>();		// ColliderComponentをActorへ追加して返す
		}
		else if (className == "RigidbodyComponent")
		{
			return &owner->AddComponent<RigidbodyComponent>();		// RigidbodyComponentをActorへ追加して返す
		}
		else if (className == "InstancedModelComponent")
		{
			return &owner->AddComponent<InstancedModelComponent>(); // InstancedModelComponentをActorへ追加して返す
		}

		return nullptr; // 未知のComponentクラス名の場合はnullptrを返す
	}

	SceneComponent* ComponentFactory::CreateRootSceneComponent(Actor* owner, std::string_view className)
	{
		if (!owner)
		{
			return nullptr; // Actorがnullptrの場合はComponentを生成しない
		}

		if (className == "SceneComponent")
		{
			return &owner->CreateRootComponent<SceneComponent>(); // SceneComponentをRootComponentとして生成して返す
		}
		else if (className == "ModelComponent")
		{
			return &owner->CreateRootComponent<ModelComponent>(); // ModelComponentをRootComponentとして生成して返す
		}
		else if (className == "CameraComponent")
		{
			return &owner->CreateRootComponent<CameraComponent>(); // CameraComponentをRootComponentとして生成して返す
		}
		else if (className == "ColliderComponent")
		{
			return &owner->CreateRootComponent<ColliderComponent>(); // ColliderComponentをRootComponentとして生成して返す
		}
		else if (className == "InstancedModelComponent")
		{
			return &owner->CreateRootComponent<InstancedModelComponent>(); // InstancedModelComponentをRootComponentとして生成して返す
		}

		return nullptr; // RootにできないComponent、又は未対応のClassなら生成しない
	}

}