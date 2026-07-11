#pragma once

#include "EditorObjectInfo.h"

#include <ActorWorld.h>
#include <SceneComponent.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>
	/// Actor / Componentの実体を直接所有せず、World OutlinerとDetails用の軽量情報へ変換します。
	/// </summary>
	inline void CollectActorWorldEditorObjects(
		ActorWorld& actorWorld,
		std::vector<EditorObjectInfo>& outObjects,
		std::string_view sceneName)
	{
		const std::string sceneNameText(sceneName);
		int actorSortOrder = 0;

		for (const auto& actorOwner : actorWorld.GetActors())
		{
			Actor* actor = actorOwner.get();
			if (!actor || actor->IsPendingDestroy())
			{
				continue;
			}

			const std::string actorKey = sceneNameText + "/Actor/" +
				std::to_string(static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(actor)));
			const uint64_t actorId = MakeStableEditorObjectId(actorKey);

			EditorObjectInfo actorInfo{};
			actorInfo.id = actorId;
			actorInfo.sortOrder = actorSortOrder++;
			actorInfo.displayName = actor->GetName().empty() ? actor->GetClassTypeName() : actor->GetName();
			actorInfo.typeName = actor->GetClassTypeName();
			actorInfo.sceneName = sceneNameText;
			actorInfo.icon = "[A]";
			actorInfo.objectKind = EditorObjectKind::Actor;
			actorInfo.canToggleActive = true;
			actorInfo.readActive = [actor]() { return actor->IsActive(); };
			actorInfo.writeActive = [actor](bool active) { actor->SetActive(active); };
			actorInfo.canRename = true;
			actorInfo.rename = [actor](std::string_view name) { actor->SetName(name); };
			actorInfo.inspectorHint = "Actor / Component Details";

			if (SceneComponent* root = actor->GetRootComponent())
			{
				actorInfo.canEditTransform = true;
				actorInfo.inspectorType = EditorInspectorType::Transform;
				actorInfo.readTransform = [root](EditorTransform& outTransform)
					{
						outTransform.position = root->GetLocalPosition();
						outTransform.rotation = root->GetLocalRotation();
						outTransform.scale = root->GetLocalScale();
						return true;
					};
				actorInfo.writeTransform = [root](const EditorTransform& transform)
					{
						root->SetLocalPosition(transform.position);
						root->SetLocalRotation(transform.rotation);
						root->SetLocalScale(transform.scale);
						root->RefreshWorldTransform();
					};
			}

			actorInfo.drawInspector = [&actorWorld, actor]()
				{
					actorWorld.SetSelectedEditorObject(actor, nullptr);
					actorWorld.DrawSelectedInspectorContent();
				};
			outObjects.push_back(std::move(actorInfo));

			std::unordered_map<const ActorComponent*, uint64_t> componentIds;
			int componentSortOrder = 0;
			for (const auto& componentOwner : actor->GetComponents())
			{
				ActorComponent* component = componentOwner.get();
				if (!component)
				{
					continue;
				}
				const std::string componentKey = actorKey + "/Component/" +
					std::to_string(static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(component)));
				componentIds.emplace(component, MakeStableEditorObjectId(componentKey));
			}

			for (const auto& componentOwner : actor->GetComponents())
			{
				ActorComponent* component = componentOwner.get();
				if (!component)
				{
					continue;
				}

				EditorObjectInfo componentInfo{};
				componentInfo.id = componentIds.at(component);
				componentInfo.parentId = actorId;
				componentInfo.sortOrder = componentSortOrder++;
				componentInfo.displayName = component->GetName().empty() ? component->GetClassTypeName() : component->GetName();
				componentInfo.typeName = component->GetClassTypeName();
				componentInfo.sceneName = sceneNameText;
				componentInfo.icon = "[C]";
				componentInfo.objectKind = EditorObjectKind::Component;
				componentInfo.isRootComponent = component == actor->GetRootComponent();
				componentInfo.canToggleActive = true;
				componentInfo.readActive = [component]() { return component->IsActive(); };
				componentInfo.writeActive = [component](bool active) { component->SetActive(active); };
				componentInfo.canRename = true;
				componentInfo.rename = [component](std::string_view name) { component->SetName(name); };
				componentInfo.inspectorHint = componentInfo.isRootComponent
					? "Root Component"
					: "Actor Component";

				if (auto* sceneComponent = dynamic_cast<SceneComponent*>(component))
				{
					if (SceneComponent* parent = sceneComponent->GetParent())
					{
						const auto parentId = componentIds.find(parent);
						if (parentId != componentIds.end())
						{
							componentInfo.parentId = parentId->second;
						}
					}

					componentInfo.canEditTransform = true;
					componentInfo.inspectorType = EditorInspectorType::Transform;
					componentInfo.readTransform = [sceneComponent](EditorTransform& outTransform)
						{
							outTransform.position = sceneComponent->GetLocalPosition();
							outTransform.rotation = sceneComponent->GetLocalRotation();
							outTransform.scale = sceneComponent->GetLocalScale();
							return true;
						};
					componentInfo.writeTransform = [sceneComponent](const EditorTransform& transform)
						{
							sceneComponent->SetLocalPosition(transform.position);
							sceneComponent->SetLocalRotation(transform.rotation);
							sceneComponent->SetLocalScale(transform.scale);
							sceneComponent->RefreshWorldTransform();
						};
				}

				componentInfo.drawInspector = [&actorWorld, actor, component]()
					{
						actorWorld.SetSelectedEditorObject(actor, component);
						actorWorld.DrawSelectedInspectorContent();
					};
				outObjects.push_back(std::move(componentInfo));
			}
		}
	}
} // namespace Ken4lowEngine
