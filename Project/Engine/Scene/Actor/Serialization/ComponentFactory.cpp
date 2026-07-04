#include "ComponentFactory.h"

#include "Actor.h"
#include "ModelComponent.h"
#include "CameraComponent.h"
#include "ColliderComponent.h"
#include "RigidbodyComponent.h"
#include "InstancedModelComponent.h"

namespace Ken4lowEngine
{
	namespace
	{
		const std::vector<ComponentFactory::ComponentTypeInfo> kRegisteredComponentTypes =
		{
			{
				"SceneComponent",
				true,
				[](Actor* owner) -> ActorComponent*
			{
				if (!owner)
				{
					return nullptr; // 所有Actorが無い場合はComponentを生成できない
				}

				return &owner->AddComponent<SceneComponent>();
			}
			},
			{
				"ModelComponent",
				true,
				[](Actor* owner) -> ActorComponent*
			{
				if (!owner)
				{
					return nullptr; // 所有Actorが無い場合はComponentを生成できない
				}

				return &owner->AddComponent<ModelComponent>();
			}
			},
			{
				"CameraComponent",
				false,
				[](Actor* owner) -> ActorComponent*
			{
				if (!owner)
				{
					return nullptr; // 所有Actorが無い場合はComponentを生成できない
				}

				return &owner->AddComponent<CameraComponent>();
			}
			},
			{
				"ColliderComponent",
				true,
				[](Actor* owner) -> ActorComponent*
			{
				if (!owner)
				{
					return nullptr; // 所有Actorが無い場合はComponentを生成できない
				}

				return &owner->AddComponent<ColliderComponent>();
			}
			},
			{
				"RigidbodyComponent",
				false,
				[](Actor* owner) -> ActorComponent*
			{
				if (!owner)
				{
					return nullptr; // 所有Actorが無い場合はComponentを生成できない
				}

				return &owner->AddComponent<RigidbodyComponent>();
			}
			},
			{
				"InstancedModelComponent",
				true,
				[](Actor* owner) -> ActorComponent*
			{
				if (!owner)
				{
					return nullptr; // 所有Actorが無い場合はComponentを生成できない
				}

				return &owner->AddComponent<InstancedModelComponent>();
			}
			},
		};
	}

	ActorComponent* ComponentFactory::CreateComponent(Actor* owner, std::string_view className)
	{
		if (!owner)
		{
			return nullptr; // Actorがnullptrの場合はComponentを生成しない
		}

		const ComponentTypeInfo* typeInfo = FindComponentType(className);

		if (!typeInfo)
		{
			return nullptr; // 未登録のComponentClass名の場合は生成しない
		}

		return typeInfo->createFunc(owner);
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

	const std::vector<ComponentFactory::ComponentTypeInfo>& ComponentFactory::GetRegisteredComponentTypes()
	{
		// Add Component UIとFactory生成対象を同じ一覧に揃える
		return kRegisteredComponentTypes;
	}

	bool ComponentFactory::IsAllowMultiple(std::string_view className)
	{
		const ComponentTypeInfo* typeInfo = FindComponentType(className);

		if (!typeInfo)
		{
			return false; // 未登録のComponentClass名の場合は複数追加不可とする
		}

		return typeInfo->allowMultiple; // 同一Actorに複数追加可能かどうかを返す
	}

	const ComponentFactory::ComponentTypeInfo* ComponentFactory::FindComponentType(std::string_view className)
	{
		for (const ComponentFactory::ComponentTypeInfo& typeInfo : kRegisteredComponentTypes)
		{
			if (typeInfo.className == className)
			{
				return &typeInfo; // Class名が一致するComponent情報を返す
			}
		}

		return nullptr; // 一致するComponent情報がない場合はnullptrを返す
	}

}